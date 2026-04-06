#include "logger.h"
#include "src/server/WsServer.h"
#include "src/server/BroadcastService.h"
#include "src/game/GameWriter.h"

#include <boost/asio.hpp>
#include <memory>
#include <thread>

static constexpr std::uint16_t WEBSOCKET_PORT = 8765;

namespace asio = boost::asio;
using     tcp  = asio::ip::tcp;

static asio::io_context          g_ioc;
static std::unique_ptr<WsServer> g_server;
static std::thread               g_ioThread;

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    SetupLog();

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* msg) {
        if (msg->type == SKSE::MessagingInterface::kPostLoadGame && !g_server) {
            tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), WEBSOCKET_PORT);
            g_server = std::make_unique<WsServer>(g_ioc, endpoint,
                [](const std::string& json) {
                    GameWriter::ApplyClientCommand(json);
                });
            BroadcastService::Start(g_ioc);
            g_ioThread = std::thread([] { g_ioc.run(); });
            g_ioThread.detach();
        }
    });

    return true;
}
