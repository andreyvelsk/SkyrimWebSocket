#pragma once

#include <nlohmann/json.hpp>

namespace QuestMarkers
{
    // Returns an array of active quest-marker destinations — the markers the
    // engine renders as floating quest arrows / quest-target icons.
    // Must be called on the game thread.
    nlohmann::json ReadQuestMarkers();

    // Debug snapshot for diagnosing quest marker sources.
    // Must be called on the game thread.
    nlohmann::json ReadQuestMarkersDebug();

    // Captures live quest-journal UI state that is not otherwise exposed in
    // quest/runtime data (currently the master Miscellaneous objective toggle).
    // Safe to call from menu/event callbacks on the game thread.
    void CaptureQuestJournalState();

    // Clears cached quest-journal UI state after loading a different save.
    // Must be called on the game thread.
    void ResetQuestJournalState();
}