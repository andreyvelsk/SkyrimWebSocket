// For Ctrl-C
#include <signal.h>

// for std::format
#include <format>

// For console input and output
#include <iostream>

// WebSockets!
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

constexpr auto WEBSOCKET_PORT = 6969;

typedef websocketpp::client<websocketpp::config::asio_client> WebSocketClient;
typedef websocketpp::config::asio_client::message_type::ptr WebSocketMessagePtr;

bool connected;
WebSocketClient _webSocketClient;
websocketpp::connection_hdl _serverConnection;

void SendMessage(const std::string& messageText) {
    if (connected) {
        _webSocketClient.send(_serverConnection, messageText, websocketpp::frame::opcode::text);
    } else {
        std::cout << std::format("Cannot send message '{}'. Not connected.", messageText) << std::endl;
    }
}

void InitializeWebSocketClient() {
    _webSocketClient.set_access_channels(websocketpp::log::alevel::none);
    _webSocketClient.clear_access_channels(websocketpp::log::alevel::all);
    _webSocketClient.set_error_channels(websocketpp::log::alevel::none);
    _webSocketClient.clear_error_channels(websocketpp::log::alevel::all);
    _webSocketClient.set_open_handler([](const websocketpp::connection_hdl& connection) {
        _serverConnection = connection;
        connected = true;
    });
    _webSocketClient.set_message_handler([](const websocketpp::connection_hdl&, const WebSocketMessagePtr& message) {
        std::cout << std::format("Received message: {}", message->get_payload()) << std::endl;
    });
    _webSocketClient.init_asio();
    auto uri = std::format("ws://localhost:{}", WEBSOCKET_PORT);
    websocketpp::lib::error_code errorCode;
    auto connection = _webSocketClient.get_connection(uri, errorCode);
    _webSocketClient.connect(connection);
    std::thread([]() { _webSocketClient.run(); }).detach();
}

int main() {
    InitializeWebSocketClient();
    signal(SIGINT, [](int) { std::exit(0); });
    while (true) {
        std::cout << "Enter Skyrim ~ Console Command:";
        std::string input;
        std::getline(std::cin, input);
        if (input.empty())
            break;
        else
            SendMessage(input);
    }
}
