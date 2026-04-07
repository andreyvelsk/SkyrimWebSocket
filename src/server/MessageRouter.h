#pragma once

#include <memory>
#include <string>

class WsSession;

namespace MessageRouter
{
    // Called on the io_context thread when a raw message arrives from the client.
    void Dispatch(std::shared_ptr<WsSession> session, const std::string& raw);
}
