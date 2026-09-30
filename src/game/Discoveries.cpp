#include "Discoveries.h"

#include "MapMarkers.h"

#include <deque>
#include <mutex>
#include <string>

namespace Discoveries
{
    namespace
    {
        constexpr std::size_t kMaxEntries = 20;

        struct Entry
        {
            std::uint64_t seq = 0;
            std::string   name;
            std::string   type;
            std::string   worldspace;
            float         x = 0.f;
            float         y = 0.f;
        };

        std::mutex        s_mutex;
        std::deque<Entry> s_entries;
        std::uint64_t     s_seq = 0;
    }

    void Record(const RE::LocationDiscovery::Event& a_event)
    {
        Entry e;
        if (auto* data = a_event.mapMarkerData) {
            const char* name = data->locationName.GetFullName();
            e.name = name ? name : "";
            e.type = std::string(MapMarkers::TypeName(static_cast<std::uint16_t>(data->type.underlying())));
        }
        if (e.name.empty())
            return;
        e.worldspace = a_event.worldspaceID ? a_event.worldspaceID : "";
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            const auto pos = player->GetPosition();
            e.x = pos.x;
            e.y = pos.y;
        }

        std::scoped_lock lock(s_mutex);
        e.seq = ++s_seq;
        s_entries.push_back(std::move(e));
        while (s_entries.size() > kMaxEntries)
            s_entries.pop_front();
    }

    nlohmann::json Read()
    {
        std::scoped_lock lock(s_mutex);
        nlohmann::json recent = nlohmann::json::array();
        for (const auto& e : s_entries) {
            recent.push_back({
                { "seq", e.seq },
                { "name", e.name },
                { "type", e.type },
                { "worldspace", e.worldspace },
                { "x", e.x },
                { "y", e.y },
            });
        }
        return { { "seq", s_seq }, { "recent", std::move(recent) } };
    }
}
