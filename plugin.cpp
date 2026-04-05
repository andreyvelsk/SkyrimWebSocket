#include "logger.h"
#include <ixwebsocket/IXWebSocketServer.h>

static std::unique_ptr<ix::WebSocketServer> g_wsServer;

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SKSE::Init(skse);
    SetupLog();

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message *msg) {
        if (msg->type == SKSE::MessagingInterface::kPostLoadGame) {
            g_wsServer = std::make_unique<ix::WebSocketServer>(8765, "127.0.0.1");
            auto res = g_wsServer->listen();
            if (res.first) {
                RE::ConsoleLog::GetSingleton()->Print("websocket server init success");
            } else {
                RE::ConsoleLog::GetSingleton()->Print("websocket server init failed");
            }
        }
    });

    return true;
}
