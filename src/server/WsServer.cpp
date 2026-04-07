#include "WsServer.h"
#include "WsSession.h"
#include "SessionManager.h"
#include "../Utils.h"

#include <boost/beast.hpp>

namespace asio  = boost::asio;
namespace beast = boost::beast;
using     tcp   = asio::ip::tcp;

WsServer::WsServer(asio::io_context& ioc, tcp::endpoint endpoint,
                   std::function<void(const std::string&)> onMessage)
    : acceptor_(ioc), onMessage_(std::move(onMessage))
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
        SKSE::GetTaskInterface()->AddTask([p = endpoint.port()] {
            PrintConsole("[WS] Server failed to bind on port " + std::to_string(p));
        });
        return;
    }

    acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
        SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Server failed to listen"); });
        return;
    }

    SKSE::GetTaskInterface()->AddTask([p = endpoint.port()] {
        PrintConsole("[WS] Server started on 0.0.0.0:" + std::to_string(p));
    });
    doAccept();
}

void WsServer::doAccept()
{
    acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            auto session = std::make_shared<WsSession>(std::move(socket), onMessage_);
            SessionManager::Register(session);
            session->run();
        }
        doAccept();
    });
}
