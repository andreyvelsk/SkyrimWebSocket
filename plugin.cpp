#include "logger.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>
#include <string>
#include <thread>

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace ws    = beast::websocket;
using     tcp   = asio::ip::tcp;

// ---------------------------------------------------------------------------
// Helpers – must be called on the main game thread
// ---------------------------------------------------------------------------

static void PrintConsole(const std::string& msg)
{
    if (auto* log = RE::ConsoleLog::GetSingleton())
        log->Print(msg.c_str());
}

static void ExecuteCommand(const std::string& cmd)
{
    const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
    const auto script  = factory ? factory->Create() : nullptr;
    if (script) {
        script->SetCommand(cmd);
        RE::ScriptCompiler compiler;
        script->CompileAndRun(&compiler, RE::PlayerCharacter::GetSingleton(),
            RE::COMPILER_NAME::kSystemWindowCompiler);
        delete script;
    }
}

// ---------------------------------------------------------------------------
// WebSocket session – one per connected client
// ---------------------------------------------------------------------------

class WsSession : public std::enable_shared_from_this<WsSession>
{
    ws::stream<tcp::socket> ws_;
    beast::flat_buffer      buf_;

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

private:
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
                ExecuteCommand(msg);
            });

            self->doRead();
        });
    }
};

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
            SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Server failed to bind on port 8765"); });
            return;
        }
        acceptor_.listen(asio::socket_base::max_listen_connections, ec);
        if (ec) {
            SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Server failed to listen"); });
            return;
        }

        SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Server started on 127.0.0.1:8765"); });
        doAccept();
    }

private:
    void doAccept()
    {
        acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
            if (!ec)
                std::make_shared<WsSession>(std::move(socket))->run();
            doAccept();
        });
    }
};

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

static asio::io_context          g_ioc;
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
            tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 8765);
            g_server   = std::make_unique<WsServer>(g_ioc, endpoint);
            g_ioThread = std::thread([] { g_ioc.run(); });
        }
    });

    return true;
}
