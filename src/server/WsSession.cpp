#include "WsSession.h"
#include "MessageRouter.h"
#include "../game/GameReader.h"
#include "../Utils.h"

namespace asio  = boost::asio;
namespace beast = boost::beast;
using     tcp   = asio::ip::tcp;

static constexpr int MIN_FREQUENCY_MS = 50;

WsSession::WsSession(tcp::socket socket, asio::io_context& ioc)
    : ws_(std::move(socket)), ioc_(ioc)
{}

WsSession::~WsSession()
{
    CancelAllSubscriptions();
}

void WsSession::run()
{
    ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
        if (ec) return;
        SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Client connected"); });
        self->doRead();
    });
}

void WsSession::send(const std::string& msg)
{
    writeQueue_.push_back(msg);
    if (!writing_)
        doWrite();
}

void WsSession::doWrite()
{
    if (writeQueue_.empty()) {
        writing_ = false;
        return;
    }
    writing_ = true;
    ws_.async_write(asio::buffer(writeQueue_.front()),
        [self = shared_from_this()](beast::error_code ec, std::size_t) {
            if (!self->writeQueue_.empty())
                self->writeQueue_.pop_front();
            if (ec) {
                // On any write error: stop the write loop and drop any pending
                // queued messages — the socket is done, and the read error will
                // tear down the session.
                self->writing_ = false;
                self->writeQueue_.clear();
                return;
            }
            self->doWrite();
        });
}

void WsSession::doRead()
{
    ws_.async_read(buf_, [self = shared_from_this()](beast::error_code ec, std::size_t) {
        if (ec) {
            self->CancelAllSubscriptions();
            SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Client disconnected"); });
            return;
        }

        std::string msg = beast::buffers_to_string(self->buf_.data());
        self->buf_.consume(self->buf_.size());

        MessageRouter::Dispatch(self, msg);

        self->doRead();
    });
}

void WsSession::SetSubscription(SubscriptionState state)
{
    // Bump the generation for this id.  Any in-flight push from the previous
    // subscription with the same id will see a mismatched generation on return
    // and discard its result instead of clobbering the fresh lastValues.
    const std::string id      = state.id;
    std::uint64_t     prevGen = 0;
    if (auto it = subscriptions_.find(id); it != subscriptions_.end())
        prevGen = it->second.generation;

    // Cancel any existing subscription with the same id.
    CancelSubscription(id);

    state.frequencyMs = std::max(MIN_FREQUENCY_MS, state.frequencyMs);

    SubscriptionEntry entry;
    entry.state      = std::move(state);
    entry.timer      = std::make_unique<asio::steady_timer>(ioc_);
    entry.generation = prevGen + 1;

    subscriptions_.emplace(id, std::move(entry));
    schedulePush(id);
}

void WsSession::CancelSubscription(const std::string& id)
{
    auto it = subscriptions_.find(id);
    if (it != subscriptions_.end()) {
        it->second.timer->cancel();
        subscriptions_.erase(it);
    }
}

void WsSession::CancelAllSubscriptions()
{
    for (auto& [id, entry] : subscriptions_)
        entry.timer->cancel();
    subscriptions_.clear();
}

void WsSession::schedulePush(const std::string& id)
{
    auto it = subscriptions_.find(id);
    if (it == subscriptions_.end())
        return;

    it->second.timer->expires_after(std::chrono::milliseconds(it->second.state.frequencyMs));
    it->second.timer->async_wait([self = shared_from_this(), id](beast::error_code ec) {
        if (!ec)
            self->doPush(id);
    });
}

void WsSession::doPush(const std::string& id)
{
    auto it = subscriptions_.find(id);
    if (it == subscriptions_.end())
        return;

    // Snapshot the current subscription state and its generation for the
    // game thread.  On return we verify the generation still matches before
    // propagating lastValues, which protects against the subscription having
    // been replaced while the push was in flight.
    SubscriptionState snapshot   = it->second.state;
    const std::uint64_t generation = it->second.generation;

    SKSE::GetTaskInterface()->AddTask([self = shared_from_this(), snapshot, generation]() mutable {
        std::string json = GameReader::BuildSubscriptionJson(snapshot);

        asio::post(self->ioc_, [self, json, id = snapshot.id,
                                lastValues = snapshot.lastValues, generation] {
            auto it = self->subscriptions_.find(id);
            if (it == self->subscriptions_.end())
                return;
            if (it->second.generation != generation)
                return;  // subscription was replaced while push was in flight

            // Propagate updated lastValues back to the live subscription.
            it->second.state.lastValues = lastValues;

            if (!json.empty())
                self->send(json);

            self->schedulePush(id);
        });
    });
}

