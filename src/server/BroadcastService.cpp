#include "BroadcastService.h"
#include "SessionManager.h"
#include "../game/GameReader.h"

#include <boost/beast.hpp>
#include <chrono>
#include <memory>

namespace asio  = boost::asio;
namespace beast = boost::beast;

static constexpr int BROADCAST_INTERVAL_MS = 200;

static asio::io_context*                   s_ioc   = nullptr;
static std::unique_ptr<asio::steady_timer> s_timer;

namespace BroadcastService
{
    static void Schedule();

    static void DoBroadcast()
    {
        auto sessions = SessionManager::GetActive();
        if (sessions.empty()) {
            Schedule();
            return;
        }

        SKSE::GetTaskInterface()->AddTask([sessions] {
            std::string json = GameReader::BuildPlayerStateJson();
            if (json.empty()) {
                asio::post(*s_ioc, [] { Schedule(); });
                return;
            }

            asio::post(*s_ioc, [sessions, json] {
                for (auto& s : sessions)
                    s->send(json);
                Schedule();
            });
        });
    }

    static void Schedule()
    {
        s_timer->expires_after(std::chrono::milliseconds(BROADCAST_INTERVAL_MS));
        s_timer->async_wait([](beast::error_code ec) {
            if (!ec)
                DoBroadcast();
        });
    }

    void Start(asio::io_context& ioc)
    {
        s_ioc   = &ioc;
        s_timer = std::make_unique<asio::steady_timer>(ioc);
        Schedule();
    }
}
