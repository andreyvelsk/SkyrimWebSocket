#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace EventBus
{
    // Initialise the event-driven optimisation layer:
    //  * registers the event-driven registry keys (currently Map::Markers,
    //    Map::Markers::All)
    //  * installs SKSE event sinks (cell load, menu open/close, quest stage,
    //    load game) that bump the version of every registered key.
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
}
