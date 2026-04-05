#include "logger.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace asio = boost::asio;

static asio::io_context g_ioc;
static asio::ip::tcp::acceptor g_acceptor(g_ioc);

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SKSE::Init(skse);
    SetupLog();

    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message *msg) {
        if (msg->type == SKSE::MessagingInterface::kPostLoadGame) {
            boost::system::error_code ec;
            asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1", ec), 8765);
            if (!ec) g_acceptor.open(endpoint.protocol(), ec);
            if (!ec) g_acceptor.bind(endpoint, ec);
            if (!ec) {
                RE::ConsoleLog::GetSingleton()->Print("websocket server init success");
            } else {
                RE::ConsoleLog::GetSingleton()->Print("websocket server init failed");
            }
        }
    });

    return true;
}
