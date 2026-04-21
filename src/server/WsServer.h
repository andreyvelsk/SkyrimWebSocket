#pragma once

#include <boost/asio.hpp>
#include <memory>

class WsServer
{
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::io_context&       ioc_;
    bool                           ok_ = false;

public:
    WsServer(boost::asio::io_context&         ioc,
             boost::asio::ip::tcp::endpoint   endpoint);

    // True when the acceptor is open and listening.
    bool ok() const { return ok_; }

private:
    void doAccept();
};
