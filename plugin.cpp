#include <ixwebsocket/IXWebSocketServer.h> // Подключаем новую библиотеку
#include "logger.h"

constexpr auto WEBSOCKET_PORT = 6969;

// Создаем глобальный объект сервера
ix::WebSocketServer _webSocketServer(WEBSOCKET_PORT, "0.0.0.0");

// Функция для выполнения консольной команды внутри игры
void RunConsoleCommand(const std::string &commandText) {
    auto *script = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>()->Create();
    if (script) {
        script->SetCommand(commandText);
        script->CompileAndRun(RE::PlayerCharacter::GetSingleton());
    }
}

// Наш сервер
void RunWebSocketServer() {
    // Настраиваем логику: что делать, когда клиент подключается или присылает сообщение
    _webSocketServer.setOnClientMessageCallback([](std::shared_ptr<ix::ConnectionState> /*connectionState*/, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg) {
        
        // Если пришло обычное текстовое сообщение
        if (msg->type == ix::WebSocketMessageType::Message) {
            auto messageText = msg->str;
            logger::info("WebSocket message received: '{}'", messageText);
            
            // 1. Отправляем ответ обратно клиенту, который прислал сообщение
            webSocket.send(std::format("Success! Skyrim is running command '{}'", messageText));
            
            // 2. Выполняем саму команду в игре
            RunConsoleCommand(messageText);
        } 
        // Если произошла ошибка подключения
        else if (msg->type == ix::WebSocketMessageType::Error) {
            logger::error("WebSocket Error: {}", msg->errorInfo.reason);
        }
    });

    // Пытаемся запустить сервер
    auto res = _webSocketServer.listen();
    if (!res.first) {
        logger::error("WebSocket server failed to start: {}", res.second);
        return;
    }

    logger::info("WebSocket server listening on port {}", WEBSOCKET_PORT);
    
    // Запускаем обработку подключений и ставим на паузу этот фоновый поток
    _webSocketServer.start();
    _webSocketServer.wait(); 
}

// Точка входа в SKSE плагин
SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SKSE::Init(skse);
    SetupLog();
    
    logger::info("Starting WebSocket Server Thread...");
    
    // Запускаем сервер в отдельном фоновом потоке, чтобы не подвесить загрузку игры
    std::thread(RunWebSocketServer).detach();
    
    return true;
}