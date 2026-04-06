#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <deque>
#include <functional>
#include <memory>
#include <string>

class WsSession : public std::enable_shared_from_this<WsSession>
{
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_;
    boost::beast::flat_buffer                                      buf_;
    std::deque<std::string>                                        writeQueue_;
    bool                                                           writing_ = false;
    std::function<void(const std::string&)>                        onMessage_;

public:
    WsSession(boost::asio::ip::tcp::socket               socket,
              std::function<void(const std::string&)>    onMessage);

    void run();
    void send(const std::string& msg);  // called only from the io_context thread

private:
    void doWrite();
    void doRead();
};
