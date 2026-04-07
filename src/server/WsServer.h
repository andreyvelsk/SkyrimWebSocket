#pragma once

#include <boost/asio.hpp>
#include <memory>

class WsServer
{
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::io_context&       ioc_;

public:
    WsServer(boost::asio::io_context&         ioc,
             boost::asio::ip::tcp::endpoint   endpoint);

private:
    void doAccept();
};
