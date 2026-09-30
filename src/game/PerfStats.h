#pragma once

#include <nlohmann/json.hpp>
#include <string>

// Time spent on the game thread by field resolvers and subscription pushes.
// Every resolve runs inside a frame, so a slow one shows as a short hitch
// that frame-time averages hide. This makes those costs visible.
namespace PerfStats
{
    // Record one measurement. Keys: registry key ("Player::Quests") or
    // "sub:<subscription id>" for a whole push. Thread-safe.
    void Record(const std::string& a_key, double a_ms);

    // Debug::FieldTimings. Sorted by max time, slowest first:
    //   { "sinceSeconds", "entries": [ { "key", "calls", "avgMs", "maxMs",
    //     "lastMs", "slowCalls" } ] }
    // slowCalls counts calls over 4 ms (a quarter of a 60 FPS frame).
    nlohmann::json Read();

    // Clear all counters.
    void Reset();
}
