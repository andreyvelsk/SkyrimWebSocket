#pragma once

#include "SubscriptionState.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <deque>
#include <memory>
#include <optional>
#include <string>

class WsSession : public std::enable_shared_from_this<WsSession>
{
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_;
    boost::beast::flat_buffer                                      buf_;
    std::deque<std::string>                                        writeQueue_;
    bool                                                           writing_ = false;

    boost::asio::io_context&                   ioc_;
    std::optional<SubscriptionState>           subscription_;
    std::unique_ptr<boost::asio::steady_timer> subTimer_;

public:
    WsSession(boost::asio::ip::tcp::socket socket,
              boost::asio::io_context&     ioc);

    ~WsSession();

    void run();
    void send(const std::string& msg);  // called only from the io_context thread

    // Replaces the current subscription and starts the push timer.
    // Called only from the io_context thread.
    void SetSubscription(SubscriptionState state);

    // Stops the push timer and clears the current subscription.
    // Called only from the io_context thread.
    void CancelSubscription();

    boost::asio::io_context& ioc() { return ioc_; }

private:
    void doWrite();
    void doRead();
    void schedulePush();
    void doPush();
};

