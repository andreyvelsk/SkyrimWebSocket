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
    CancelSubscription();
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
            self->writeQueue_.pop_front();
            if (ec) return;
            self->doWrite();
        });
}

void WsSession::doRead()
{
    ws_.async_read(buf_, [self = shared_from_this()](beast::error_code ec, std::size_t) {
        if (ec) {
            self->CancelSubscription();
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
    CancelSubscription();
    state.frequencyMs = std::max(MIN_FREQUENCY_MS, state.frequencyMs);
    subscription_     = std::move(state);
    subTimer_         = std::make_unique<asio::steady_timer>(ioc_);
    schedulePush();
}

void WsSession::CancelSubscription()
{
    if (subTimer_) {
        subTimer_->cancel();
        subTimer_.reset();
    }
    subscription_.reset();
}

void WsSession::schedulePush()
{
    if (!subscription_ || !subTimer_)
        return;

    subTimer_->expires_after(std::chrono::milliseconds(subscription_->frequencyMs));
    subTimer_->async_wait([self = shared_from_this()](beast::error_code ec) {
        if (ec || !self->subscription_)
            return;
        self->doPush();
    });
}

void WsSession::doPush()
{
    if (!subscription_)
        return;

    // Snapshot the current subscription state for the game thread.
    // lastValues is propagated back to the session after the read.
    SubscriptionState snapshot = *subscription_;

    SKSE::GetTaskInterface()->AddTask([self = shared_from_this(), snapshot]() mutable {
        std::string json = GameReader::BuildSubscriptionJson(snapshot);

        asio::post(self->ioc_, [self, json, lastValues = snapshot.lastValues] {
            if (!self->subscription_)
                return;

            // Propagate updated lastValues back to the live subscription.
            self->subscription_->lastValues = lastValues;

            if (!json.empty())
                self->send(json);

            self->schedulePush();
        });
    });
}

