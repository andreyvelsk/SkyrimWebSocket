#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>

namespace EventBus
{
    // Initialise the event-driven optimisation layer:
    //  * registers the event-driven registry keys (currently
    //    Map::Markers::Locations, Map::Markers::All)
    //  * installs SKSE event sinks (cell load, menu open/close, load game)
    //    that bump the version of every registered key.
    //
    // Safe to call once after kDataLoaded.  A second call is a no-op.
    // Must be called on the main game thread.
    void Install();

    // Returns true when the given registry key has been registered as
    // event-driven via Install().  Polling resolvers can use this to decide
    // whether a version-based skip is allowed.
    bool IsEventDriven(const std::string& registryKey);

    // Returns the current version counter for a registry key.  Each event that
    // affects the key bumps this counter by one.  Returns 0 if the key was
    // never registered (in which case IsEventDriven also returns false).
    //
    // Cheap and lock-free.  Safe to call from any thread.
    std::uint64_t GetVersion(const std::string& registryKey);

    // Shared per-key cache of the most recent resolver output, keyed by the
    // EventBus version that produced it.  Subscribers all read the same cached
    // value: the resolver runs at most once per (key, version), regardless of
    // how many subscribers poll for the field or how often.
    //
    // If the cached version matches GetVersion(key), `compute` is not called
    // and the cached JSON value is returned.  Otherwise `compute` runs, its
    // result is stored, and returned.
    //
    // Returns the cached/freshly-computed JSON together with the version it
    // was produced at.  Must be called on the game thread (the resolver and
    // the cache are not thread-safe).
    struct CachedValue
    {
        std::uint64_t  version;
        nlohmann::json value;
    };
    CachedValue ResolveCached(const std::string&                          registryKey,
                              const std::function<nlohmann::json()>&      compute);
}
