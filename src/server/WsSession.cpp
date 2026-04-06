#include "WsSession.h"
#include "../Utils.h"

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace ws    = beast::websocket;
using     tcp   = asio::ip::tcp;

WsSession::WsSession(tcp::socket socket, std::function<void(const std::string&)> onMessage)
    : ws_(std::move(socket)), onMessage_(std::move(onMessage))
{}

void WsSession::run()
{
    ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
        if (ec) return;
        SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Client connected"); });
        self->doRead();
    });
}

void WsSession::send(const std::string& msg)
{
    writeQueue_.push_back(msg);
    if (!writing_)
        doWrite();
}

void WsSession::doWrite()
{
    if (writeQueue_.empty()) {
        writing_ = false;
        return;
    }
    writing_ = true;
    ws_.async_write(asio::buffer(writeQueue_.front()),
        [self = shared_from_this()](beast::error_code ec, std::size_t) {
            self->writeQueue_.pop_front();
            if (ec) return;
            self->doWrite();
        });
}

void WsSession::doRead()
{
    ws_.async_read(buf_, [self = shared_from_this()](beast::error_code ec, std::size_t) {
        if (ec) {
            SKSE::GetTaskInterface()->AddTask([] { PrintConsole("[WS] Client disconnected"); });
            return;
        }

        std::string msg = beast::buffers_to_string(self->buf_.data());
        self->buf_.consume(self->buf_.size());

        SKSE::GetTaskInterface()->AddTask([msg, cb = self->onMessage_] {
            if (cb)
                cb(msg);
        });

        self->doRead();
    });
}
