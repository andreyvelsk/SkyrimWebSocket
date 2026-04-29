#include "EventBus.h"

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
        // is safe to register globally.        //
        // We deliberately do NOT subscribe to TESQuestStageEvent: it fires
        // on every quest-stage change in the game (very frequent), and the
        // vast majority of stages do not reveal map markers.  Bumping on
        // every stage forces a full re-walk of all worldspaces on the next
        // poll, which is the exact freeze we are trying to avoid.  Markers
        // that are script-revealed mid-quest will refresh on the next cell
        // load or when the player opens MapMenu.
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
                BumpKeys({"Map::Markers", "Map::Markers::All"}, "TESCellFullyLoaded");
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
                if (event->menuName != RE::MapMenu::MENU_NAME)
                    return RE::BSEventNotifyControl::kContinue;

                // Bump on both open and close so:
                //  * opening the map pushes fresh data even if the player
                //    fast-travelled between polls,
                //  * closing the map captures any player-placed marker change.
                BumpKeys({"Map::Markers", "Map::Markers::All"},
                         event->opening ? "MapMenu open" : "MapMenu close");
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
        // For now we expose this only for map-marker fields; other heavy
        // resolvers (Inventory::*, Magic::*) can opt in later.
        RegisterKey("Map::Markers");
        RegisterKey("Map::Markers::All");

        // ── Install SKSE event sinks ─────────────────────────────────────
        if (auto* src = RE::ScriptEventSourceHolder::GetSingleton()) {
            src->AddEventSink<RE::TESCellFullyLoadedEvent>(CellLoadSink::GetSingleton());
            src->AddEventSink<RE::TESLoadGameEvent>(LoadGameSink::GetSingleton());
        } else {
            logger::warn("[EventBus] ScriptEventSourceHolder unavailable; "
                         "TES* sinks not installed");
        }

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
