#include "SessionManager.h"
#include "WsSession.h"

#include <mutex>

namespace SessionManager
{
    static std::mutex                            s_mtx;
    static std::vector<std::weak_ptr<WsSession>> s_sessions;

    void Register(const std::shared_ptr<WsSession>& session)
    {
        std::lock_guard lock(s_mtx);
        s_sessions.emplace_back(session);
    }

    std::vector<std::shared_ptr<WsSession>> GetActive()
    {
        std::lock_guard lock(s_mtx);
        std::vector<std::shared_ptr<WsSession>> active;
        for (auto it = s_sessions.begin(); it != s_sessions.end(); ) {
            if (auto s = it->lock()) {
                active.push_back(s);
                ++it;
            } else {
                it = s_sessions.erase(it);
            }
        }
        return active;
    }
}
