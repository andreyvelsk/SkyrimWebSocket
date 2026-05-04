#include "EventBus.h"

#include "PlayerReader.h"

#include "../../logger.h"

#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>

namespace EventBus
{
    namespace
    {
        // Each registered event-driven key gets its own atomic counter.
        // We allocate them once at Install() and never erase, so raw pointers
        // are stable for the lifetime of the process.
        struct KeyEntry
        {
            std::string                      key;
            std::unique_ptr<std::atomic<std::uint64_t>> version;
        };

        std::unordered_map<std::string, std::atomic<std::uint64_t>*> g_versions;
        std::vector<KeyEntry>                                        g_storage;
        bool                                                         g_installed = false;

        // Shared resolver cache.  Game-thread only — ResolveCached is the
        // sole accessor and is documented to run on the game thread, so we
        // do not need a mutex.
        struct CacheEntry
        {
            // 0 means "never resolved".  Cache is considered valid only when
            // version != 0 AND version == GetVersion(key).
            std::uint64_t  version = 0;
            nlohmann::json value;
        };
        std::unordered_map<std::string, CacheEntry> g_cache;

        void RegisterKey(const std::string& key)
        {
            auto entry    = KeyEntry{key, std::make_unique<std::atomic<std::uint64_t>>(0)};
            auto* counter = entry.version.get();
            g_storage.push_back(std::move(entry));
            g_versions.emplace(key, counter);
        }

        // Bump every registered key's version.  Used by global events
        // (load game) that invalidate everything.
        void BumpAll(const char* reason)
        {
            for (auto& [key, counter] : g_versions) {
                const auto v = counter->fetch_add(1, std::memory_order_relaxed) + 1;
                logger::trace("[EventBus] bump '{}' -> {} ({})", key, v, reason);
            }
        }

        // Bump a specific subset of keys.
        void BumpKeys(std::initializer_list<const char*> keys, const char* reason)
        {
            for (const char* k : keys) {
                auto it = g_versions.find(k);
                if (it == g_versions.end())
                    continue;
                const auto v = it->second->fetch_add(1, std::memory_order_relaxed) + 1;
                logger::trace("[EventBus] bump '{}' -> {} ({})", k, v, reason);
            }
        }

        // ── SKSE event sinks ─────────────────────────────────────────────
        //
        // All sinks are stateless singletons.  ProcessEvent runs on the game
        // thread; we keep the work to a couple of atomic increments so this
        // is safe to register globally.
        class CellLoadSink final : public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>
        {
        public:
            static CellLoadSink* GetSingleton()
            {
                static CellLoadSink s;
                return &s;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESCellFullyLoadedEvent*,
                RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*) override
            {
                BumpKeys({"Map::Markers::Locations", "Map::Markers::All"},
                         "TESCellFullyLoaded");
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            static MenuSink* GetSingleton()
            {
                static MenuSink s;
                return &s;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent*                 event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (!event)
                    return RE::BSEventNotifyControl::kContinue;

                if (event->menuName == RE::JournalMenu::MENU_NAME) {
                    if (!event->opening)
                        PlayerReader::CaptureQuestJournalState();
                    logger::trace("[EventBus] observed JournalMenu {}", event->opening ? "open" : "close");
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (event->menuName != RE::MapMenu::MENU_NAME)
                    return RE::BSEventNotifyControl::kContinue;

                // Bump on both open and close so:
                //  * opening the map pushes fresh data even if the player
                //    fast-travelled between polls,
                //  * closing the map captures any player-placed marker change.
                BumpKeys({"Map::Markers::Locations", "Map::Markers::All"},
                         event->opening ? "MapMenu open" : "MapMenu close");
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        // Fires when the player walks close enough to a location for the
        // engine to flip its map marker to discovered (the
        // "Location Discovered" pop-up).  This is the canonical event for
        // exterior marker discovery and is what we were missing earlier:
        // TESCellFullyLoadedEvent fires on cell loads, not on the moment of
        // discovery.
        class LocationDiscoverySink final : public RE::BSTEventSink<RE::LocationDiscovery::Event>
        {
        public:
            static LocationDiscoverySink* GetSingleton()
            {
                static LocationDiscoverySink s;
                return &s;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::LocationDiscovery::Event*,
                RE::BSTEventSource<RE::LocationDiscovery::Event>*) override
            {
                BumpKeys({"Map::Markers::Locations", "Map::Markers::All"},
                         "LocationDiscovery");
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        // Quest stages frequently script-reveal map markers ("a marker has
        // been added to your map").  Many stages do NOT touch markers, but
        // bumping is cheap (one atomic increment) and the actual re-walk is
        // de-duplicated by the shared resolver cache: multiple bumps within
        // a single poll interval still produce only one walk.
        class QuestStageSink final : public RE::BSTEventSink<RE::TESQuestStageEvent>
        {
        public:
            static QuestStageSink* GetSingleton()
            {
                static QuestStageSink s;
                return &s;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESQuestStageEvent*,
                RE::BSTEventSource<RE::TESQuestStageEvent>*) override
            {
                BumpKeys({"Map::Markers::Locations", "Map::Markers::All"},
                         "TESQuestStage");
                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class LoadGameSink final : public RE::BSTEventSink<RE::TESLoadGameEvent>
        {
        public:
            static LoadGameSink* GetSingleton()
            {
                static LoadGameSink s;
                return &s;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESLoadGameEvent*,
                RE::BSTEventSource<RE::TESLoadGameEvent>*) override
            {
                // A new save invalidates every cached value.
                PlayerReader::ResetQuestJournalState();
                BumpAll("TESLoadGame");
                return RE::BSEventNotifyControl::kContinue;
            }
        };
    }  // namespace

    void Install()
    {
        if (g_installed)
            return;
        g_installed = true;

        // ── Register the event-driven registry keys ──────────────────────
        // For now we expose this only for heavy worldspace map-marker fields;
        // other expensive resolvers can opt in later. Quest markers are
        // intentionally polled and JSON-compared because the journal UI can
        // change tracking state through Scaleform callbacks without a stable
        // native event for every toggle.
        RegisterKey("Map::Markers::Locations");
        RegisterKey("Map::Markers::All");

        // ── Install SKSE event sinks ─────────────────────────────────────
        if (auto* src = RE::ScriptEventSourceHolder::GetSingleton()) {
            src->AddEventSink<RE::TESCellFullyLoadedEvent>(CellLoadSink::GetSingleton());
            src->AddEventSink<RE::TESQuestStageEvent>(QuestStageSink::GetSingleton());
            src->AddEventSink<RE::TESLoadGameEvent>(LoadGameSink::GetSingleton());
        } else {
            logger::warn("[EventBus] ScriptEventSourceHolder unavailable; "
                         "TES* sinks not installed");
        }

        // LocationDiscovery has its own event source, not routed via
        // ScriptEventSourceHolder.
        if (auto* lds = RE::LocationDiscovery::GetEventSource())
            lds->AddEventSink<RE::LocationDiscovery::Event>(LocationDiscoverySink::GetSingleton());
        else
            logger::warn("[EventBus] LocationDiscovery::GetEventSource() returned null; "
                         "discovery sink not installed");

        if (auto* ui = RE::UI::GetSingleton())
            ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuSink::GetSingleton());
        else
            logger::warn("[EventBus] UI singleton unavailable; MenuOpenCloseEvent sink not installed");

        logger::info("[EventBus] installed; event-driven keys: {}", g_versions.size());
    }

    bool IsEventDriven(const std::string& registryKey)
    {
        return g_versions.find(registryKey) != g_versions.end();
    }

    std::uint64_t GetVersion(const std::string& registryKey)
    {
        auto it = g_versions.find(registryKey);
        if (it == g_versions.end())
            return 0;
        return it->second->load(std::memory_order_relaxed);
    }

    CachedValue ResolveCached(const std::string&                          registryKey,
                              const std::function<nlohmann::json()>&      compute)
    {
        const auto current = GetVersion(registryKey);
        auto&      slot    = g_cache[registryKey];

        // Hit: cached entry was produced at the current version (and is not
        // the initial sentinel 0).  Reuse it without invoking `compute`.
        if (slot.version == current && slot.version != 0) {
            logger::trace("[EventBus] cache hit '{}' v={}", registryKey, current);
            return {slot.version, slot.value};
        }

        logger::debug("[EventBus] cache miss '{}' prev_v={} new_v={} — resolving",
                      registryKey, slot.version, current);
        slot.value   = compute();
        slot.version = current;
        return {slot.version, slot.value};
    }
}
