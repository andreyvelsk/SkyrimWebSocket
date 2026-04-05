#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "logger.h"

constexpr auto WEBSOCKET_PORT = 6969;

typedef websocketpp::server<websocketpp::config::asio> WebSocketServer;
typedef WebSocketServer::message_ptr WebSocketMessagePtr;
typedef WebSocketServer::message_handler WebSocketMessageHandler;

WebSocketServer _webSocketServer;

void RunConsoleCommand(const std::string &commandText) {
    auto *script = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()->Create();
    script->SetCommand(commandText);
    script->CompileAndRun(RE::PlayerCharacter::GetSingleton());
}

void RunWebSocketServer() {
    try {
        _webSocketServer.set_message_handler([](websocketpp::connection_hdl connection, WebSocketMessagePtr message) {
            auto messageText = message->get_payload();
            logger::info("WebSocket message '{}'", messageText);
            // Want to reply to the caller?
            // <server>.send(<incoming connection>, <message>, <type>)
            _webSocketServer.send(connection, std::format("Running command '{}'", messageText),
                                  websocketpp::frame::opcode::text);
            RunConsoleCommand(messageText);
        });
        _webSocketServer.set_access_channels(websocketpp::log::alevel::all);
        _webSocketServer.init_asio();
        _webSocketServer.listen(WEBSOCKET_PORT);
        logger::info("WebSocket server listening on {}", WEBSOCKET_PORT);
        _webSocketServer.start_accept();
        _webSocketServer.run();
    } catch (websocketpp::exception const &e) {
        logger::error("websocketpp Server Error: {}", e.what());
    } catch (...) {
        logger::error("Unknown Server Error");
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SKSE::Init(skse);
    SetupLog();
    logger::info("Running server...");
    std::thread(RunWebSocketServer).detach();
    return true;
}
