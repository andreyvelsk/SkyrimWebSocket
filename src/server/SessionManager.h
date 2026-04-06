#pragma once

#include <memory>
#include <vector>

class WsSession;

namespace SessionManager
{
    void Register(const std::shared_ptr<WsSession>& session);
    std::vector<std::shared_ptr<WsSession>> GetActive();
}
