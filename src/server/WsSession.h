#pragma once

#include "SubscriptionState.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>

class WsSession : public std::enable_shared_from_this<WsSession>
{
    boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_;
    boost::beast::flat_buffer                                      buf_;
    std::deque<std::string>                                        writeQueue_;
    bool                                                           writing_ = false;

    boost::asio::io_context& ioc_;

    struct SubscriptionEntry {
        SubscriptionState                          state;
        std::unique_ptr<boost::asio::steady_timer> timer;
    };

    std::unordered_map<std::string, SubscriptionEntry> subscriptions_;

public:
    WsSession(boost::asio::ip::tcp::socket socket,
              boost::asio::io_context&     ioc);

    ~WsSession();

    void run();
    void send(const std::string& msg);  // called only from the io_context thread

    // Adds or replaces the subscription identified by state.id and starts its push timer.
    // Called only from the io_context thread.
    void SetSubscription(SubscriptionState state);

    // Stops and removes the subscription with the given id.
    // Called only from the io_context thread.
    void CancelSubscription(const std::string& id);

    // Stops and removes all active subscriptions.
    // Called only from the io_context thread.
    void CancelAllSubscriptions();

    boost::asio::io_context& ioc() { return ioc_; }

private:
    void doWrite();
    void doRead();
    void schedulePush(const std::string& id);
    void doPush(const std::string& id);
};

