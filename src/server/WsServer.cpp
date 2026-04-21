#include "WsServer.h"
#include "WsSession.h"
#include "../Utils.h"

#include <boost/beast.hpp>

namespace asio   = boost::asio;
namespace beast  = boost::beast;
namespace logger = SKSE::log;
using     tcp    = asio::ip::tcp;

WsServer::WsServer(asio::io_context& ioc, tcp::endpoint endpoint)
    : acceptor_(ioc), ioc_(ioc)
{
    beast::error_code ec;

    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        logger::error("WsServer: acceptor open failed: {}", ec.message());
        SKSE::GetTaskInterface()->AddTask([msg = ec.message()] {
            PrintConsole("[WS] Server failed to open: " + msg);
        });
        return;
    }

    acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
    acceptor_.bind(endpoint, ec);
    if (ec) {
        logger::error("WsServer: bind failed on port {}: {}", endpoint.port(), ec.message());
        SKSE::GetTaskInterface()->AddTask([p = endpoint.port(), msg = ec.message()] {
            PrintConsole("[WS] Server failed to bind on port " + std::to_string(p) + ": " + msg);
        });
        return;
    }

    acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
        logger::error("WsServer: listen failed: {}", ec.message());
        SKSE::GetTaskInterface()->AddTask([msg = ec.message()] {
            PrintConsole("[WS] Server failed to listen: " + msg);
        });
        return;
    }

    ok_ = true;

    logger::info("WsServer: started on {}:{}", endpoint.address().to_string(), endpoint.port());
    SKSE::GetTaskInterface()->AddTask([addr = endpoint.address().to_string(), p = endpoint.port()] {
        PrintConsole("[WS] Server started on " + addr + ":" + std::to_string(p));
    });
    doAccept();
}

void WsServer::doAccept()
{
    acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
        if (ec) {
            logger::warn("WsServer: accept failed: {}", ec.message());
        } else {
            auto session = std::make_shared<WsSession>(std::move(socket), ioc_);
            session->run();
        }
        if (acceptor_.is_open())
            doAccept();
    });
}
