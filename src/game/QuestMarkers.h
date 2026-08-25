#pragma once

#include <RE/Skyrim.h>
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

    // Per-quest diagnostics for a single quest form ID (objectives, targets,
    // native TeleportPath per target, coordinate resolution, player context).
    // Must be called on the game thread.
    nlohmann::json DebugQuestMarker(RE::FormID questFormId);

    // Captures live quest-journal UI state that is not otherwise exposed in
    // quest/runtime data (currently the master Miscellaneous objective toggle).
    // Safe to call from menu/event callbacks on the game thread.
    void CaptureQuestJournalState();

    // Clears cached quest-journal UI state after loading a different save.
    // Must be called on the game thread.
    void ResetQuestJournalState();

    // Overrides the journal's master Miscellaneous-objectives toggle until
    // the next save load. Also applies to the live journal menu when open.
    // Must be called on the game thread.
    void SetMiscObjectivesVisible(bool visible);
}