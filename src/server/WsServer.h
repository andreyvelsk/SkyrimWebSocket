#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <string>

class WsServer
{
    boost::asio::ip::tcp::acceptor          acceptor_;
    std::function<void(const std::string&)> onMessage_;

public:
    WsServer(boost::asio::io_context&                    ioc,
             boost::asio::ip::tcp::endpoint              endpoint,
             std::function<void(const std::string&)>     onMessage);

private:
    void doAccept();
};
