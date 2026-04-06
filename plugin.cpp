#include "logger.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <cmath>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static constexpr std::uint16_t WEBSOCKET_PORT        = 8765;
static constexpr int           BROADCAST_INTERVAL_MS = 200;

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace ws    = beast::websocket;
using     tcp   = asio::ip::tcp;

// ---------------------------------------------------------------------------
// Global I/O objects (declared early so helpers below can reference them)
// ---------------------------------------------------------------------------

static asio::io_context                    g_ioc;
static std::unique_ptr<asio::steady_timer> g_broadcastTimer;

// ---------------------------------------------------------------------------
// Helpers – must be called on the main game thread
// ---------------------------------------------------------------------------

static void PrintConsole(const std::string& msg)
{
    if (auto* log = RE::ConsoleLog::GetSingleton())
        log->Print(msg.c_str());
}

// ---------------------------------------------------------------------------
// WebSocket session – one per connected client
// ---------------------------------------------------------------------------

class WsSession : public std::enable_shared_from_this<WsSession>
{
    ws::stream<tcp::socket> ws_;
    beast::flat_buffer      buf_;
    std::deque<std::string> writeQueue_;
    bool                    writing_ = false;

public:
    explicit WsSession(tcp::socket socket) : ws_(std::move(socket)) {}

    void run()
    {
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            if (ec) return;
            SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Client connected"); });
            self->doRead();
        });
    }

    // Must be called only from the io_context thread
    void send(const std::string& msg)
    {
        writeQueue_.push_back(msg);
        if (!writing_)
            doWrite();
    }

private:
    void doWrite()
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

    void doRead()
    {
        ws_.async_read(buf_, [self = shared_from_this()](beast::error_code ec, std::size_t) {
            if (ec) {
                SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Client disconnected"); });
                return;
            }

            std::string msg = beast::buffers_to_string(self->buf_.data());
            self->buf_.consume(self->buf_.size());

            SKSE::GetTaskInterface()->AddTask([msg] {
                PrintConsole("[WS] Received: " + msg);
            });

            self->doRead();
        });
    }
};

// ---------------------------------------------------------------------------
// Session registry – weak_ptr list so dead sessions are pruned automatically
// ---------------------------------------------------------------------------

static std::mutex                              g_sessionsMtx;
static std::vector<std::weak_ptr<WsSession>>  g_sessions;

static void RegisterSession(const std::shared_ptr<WsSession>& session)
{
    std::lock_guard lock(g_sessionsMtx);
    g_sessions.emplace_back(session);
}

static std::vector<std::shared_ptr<WsSession>> GetActiveSessions()
{
    std::lock_guard lock(g_sessionsMtx);
    std::vector<std::shared_ptr<WsSession>> active;
    for (auto it = g_sessions.begin(); it != g_sessions.end(); ) {
        if (auto s = it->lock()) {
            active.push_back(s);
            ++it;
        } else {
            it = g_sessions.erase(it);
        }
    }
    return active;
}

// ---------------------------------------------------------------------------
// Broadcast timer – sends player state JSON to all clients every 200 ms
// ---------------------------------------------------------------------------

static void ScheduleBroadcast();

static void DoBroadcast()
{
    auto sessions = GetActiveSessions();
    if (sessions.empty()) {
        ScheduleBroadcast();
        return;
    }

    // Read player data on the game thread, then post the send back to ioc
    SKSE::GetTaskInterface()->AddTask([sessions] {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            asio::post(g_ioc, [] { ScheduleBroadcast(); });
            return;
        }

        auto* actor = static_cast<RE::Actor*>(player);
        float health  = actor->GetActorValue(RE::ActorValue::kHealth);
        float magicka = actor->GetActorValue(RE::ActorValue::kMagicka);
        float stamina = actor->GetActorValue(RE::ActorValue::kStamina);

        if (!std::isfinite(health))  health  = 0.f;
        if (!std::isfinite(magicka)) magicka = 0.f;
        if (!std::isfinite(stamina)) stamina = 0.f;

        char buf[128];
        std::snprintf(buf, sizeof(buf),
            R"({"health":%.1f,"magicka":%.1f,"stamina":%.1f})",
            health, magicka, stamina);
        std::string json(buf);

        asio::post(g_ioc, [sessions, json] {
            for (auto& s : sessions)
                s->send(json);
            ScheduleBroadcast();
        });
    });
}

static void ScheduleBroadcast()
{
    g_broadcastTimer->expires_after(std::chrono::milliseconds(BROADCAST_INTERVAL_MS));
    g_broadcastTimer->async_wait([](beast::error_code ec) {
        if (!ec)
            DoBroadcast();
    });
}

// ---------------------------------------------------------------------------
// WebSocket server – async accept loop
// ---------------------------------------------------------------------------

class WsServer
{
    tcp::acceptor acceptor_;

public:
    WsServer(asio::io_context& ioc, tcp::endpoint endpoint) : acceptor_(ioc)
    {
        beast::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Server failed to open"); });
            return;
        }
        acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
        acceptor_.bind(endpoint, ec);
        if (ec) {
            SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Server failed to bind on port " + std::to_string(WEBSOCKET_PORT)); });
            return;
        }
        acceptor_.listen(asio::socket_base::max_listen_connections, ec);
        if (ec) {
            SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Server failed to listen"); });
            return;
        }

        SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Server started on 127.0.0.1:" + std::to_string(WEBSOCKET_PORT)); });
        doAccept();
    }

private:
    void doAccept()
    {
        acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<WsSession>(std::move(socket));
                RegisterSession(session);
                session->run();
            }
            doAccept();
        });
    }
};

// ---------------------------------------------------------------------------
// Remaining global state
// ---------------------------------------------------------------------------

static std::unique_ptr<WsServer> g_server;
static std::thread               g_ioThread;

// ---------------------------------------------------------------------------
// SKSE plugin entry point
// ---------------------------------------------------------------------------

SKSEPluginLoad(const SKSE::LoadInterface *skse)
{
    SKSE::Init(skse);
    SetupLog();

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message *msg) {
        if (msg->type == SKSE::MessagingInterface::kPostLoadGame && !g_server) {
            tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), WEBSOCKET_PORT);
            g_broadcastTimer = std::make_unique<asio::steady_timer>(g_ioc);
            g_server         = std::make_unique<WsServer>(g_ioc, endpoint);
            ScheduleBroadcast();
            g_ioThread = std::thread([] { g_ioc.run(); });
            g_ioThread.detach();
        }
    });

    return true;
}
