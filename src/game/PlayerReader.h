#pragma once

#include <nlohmann/json.hpp>

namespace PlayerReader
{
    // Returns the player's current character level as an integer.
    // Must be called on the game thread.
    nlohmann::json ReadLevel();

    // Returns the player's current XP within the current level as a float.
    // Must be called on the game thread.
    nlohmann::json ReadXPCurrent();

    // Returns the XP threshold required to reach the next level as a float.
    // Must be called on the game thread.
    nlohmann::json ReadXPNext();

    // Returns the XP value at the start of the current level as a float (always 0.0).
    // Provided for progress-bar calculations alongside XPCurrent and XPNext.
    // Must be called on the game thread.
    nlohmann::json ReadXPLevelStart();

    // Returns the total weight of all items currently in the player's inventory as a float.
    // Must be called on the game thread.
    nlohmann::json ReadInventoryWeight();

    // Returns the player's maximum carry weight as a float.
    // Must be called on the game thread.
    nlohmann::json ReadCarryWeight();

    // Returns the current game language as a lowercase string (e.g. "english", "russian").
    // Reads sLanguage:General from the INI setting collection.
    // Must be called on the game thread.
    nlohmann::json ReadLanguage();

    // Returns the player's current world position and heading as a JSON object:
    // { "x", "y", "z", "angle",
    //   "worldspace", "worldspaceFormId",
    //   "parentWorldspace", "parentWorldspaceFormId",
    //   "cell", "cellFormId", "isInterior" }
    // x/y/z are local to the current worldspace (or interior cell); angle is the
    // Z-axis rotation (yaw) in radians. worldspace fields are null in interiors.
    // parentWorldspace walks up TESWorldSpace::parentWorld to the root (e.g. Tamriel
    // for city sub-worlds); equals worldspace for top-level worlds (Tamriel,
    // Solstheim).
    // Must be called on the game thread.
    nlohmann::json ReadPosition();

    // Returns the map-position to use when rendering the player on a global world map.
    //
    // - If the player is in a top-level exterior worldspace (Tamriel, Solstheim, etc.),
    //   returns the live player coordinates and that worldspace.
    // - If the player is inside an interior cell or a city sub-worldspace, resolves
    //   the BGSLocation::worldLocMarker reference (walking up the location hierarchy
    //   if needed) and returns the marker's coordinates in the exterior worldspace.
    //   This is the fixed entrance point visible on the world map (e.g. the cave
    //   door on Tamriel, or the city gate).  No engine cache is involved, so values
    //   are always correct — including during and immediately after fast travel.
    //
    // Shape: { "x", "y", "z",
    //          "worldspace", "worldspaceFormId",
    //          "parentWorldspace", "parentWorldspaceFormId" }
    // x/y/z and worldspace fields are null when no BGSLocation marker can be found
    // (rare: small hand-placed cells with no location assignment).
    // Must be called on the game thread.
    nlohmann::json ReadExteriorPosition();

    // Returns an array of map markers currently shown on the player's world
    // map (i.e. markers whose MapMarkerData::Flag::kVisible bit is set —
    // either pre-set in the ESM for cities/major locations, or toggled on by
    // the engine when the player discovers them, or by quest scripts).
    // Each entry: { "refId", "name", "type", "typeId", "x", "y", "isVisible", "canFastTravel" }
    // Must be called on the game thread.
    nlohmann::json ReadMapMarkers();

    // Same as ReadMapMarkers but returns ALL map markers in every loaded
    // worldspace, including undiscovered/hidden ones. Useful for tooling /
    // map editors.
    // Must be called on the game thread.
    nlohmann::json ReadMapMarkersAll();

    // Returns an array of active quest-marker destinations — the markers the
    // engine renders as floating quest arrows / quest-target icons. On SE/AE,
    // this uses PlayerCharacter::questTargets as the runtime target-candidate
    // source, then filters by the journal tracking flags. Miscellaneous quest
    // targets also honor the journal's master "Miscellaneous" toggle when its
    // state has been observed. VR uses a best-effort static fallback. Multiple
    // target aliases that resolve to the same map-facing destination are
    // collapsed. When a target belongs to a BGSLocation, x/y/z are resolved
    // through a map-facing location marker: BGSLocation::worldLocMarker when
    // usable, otherwise location specialRefs / persistent-cell ExtraMapMarker
    // refs matching Map::Markers::Locations. localX/localY/localZ keep the raw
    // target reference coordinates for clients that need them. If an interior
    // or child-worldspace target has no global marker, x/y/z are null rather
    // than local cell/world coords.
    // Each entry: { questFormId, questEditorId, questName, questType,
    //               isActive, isMiscellaneous,
    //               objectiveIndex, objectiveText, objectiveTextResolved, aliasId,
    //               refId, isDeleted, name,
    //               coordinateSource, coordinateRefId, coordinateRefName,
    //               locationFormId, locationEditorId, locationName,
    //               localX, localY, localZ,
    //               localWorldspace, localWorldspaceFormId,
    //               localParentWorldspace, localParentWorldspaceFormId,
    //               localCell, localCellFormId, localIsInterior,
    //               x, y, z,
    //               worldspace, worldspaceFormId,
    //               parentWorldspace, parentWorldspaceFormId,
    //               cell, cellFormId, isInterior }.
    // Must be called on the game thread.
    nlohmann::json ReadQuestMarkers();

    // Captures live quest-journal UI state that is not otherwise exposed in
    // quest/runtime data (currently the master Miscellaneous objective toggle).
    // Safe to call from menu/event callbacks on the game thread.
    void CaptureQuestJournalState();

    // Clears cached quest-journal UI state after loading a different save.
    // Must be called on the game thread.
    void ResetQuestJournalState();

    // Debug snapshot for diagnosing quest marker sources. Returns raw
    // PlayerCharacter questTargets/objectives summaries plus current
    // ReadQuestMarkers output. Intended for troubleshooting only.
    // Must be called on the game thread.
    nlohmann::json ReadQuestMarkersDebug();

    // Returns and updates the cached master Miscellaneous quest-marker
    // visibility used by Map::Markers::Quests. Updating this value changes
    // the shared Misc map-marker filter only; it does not toggle individual
    // quest active flags.
    // Must be called on the game thread.
    nlohmann::json ReadMiscQuestMarkerVisibility();
    bool GetMiscQuestMarkerVisibility();
    void SetMiscQuestMarkerVisibility(bool visible);

    // Returns a JSON object describing the player-placed custom map marker
    // (the marker the player can drop on the world map by clicking on it):
    // { "isSet", "x", "y", "z",
    //   "worldspace", "worldspaceFormId",
    //   "parentWorldspace", "parentWorldspaceFormId" }
    // When the marker is not set (or never placed in this save), `isSet` is
    // false and the coordinate / worldspace fields are null.
    // Must be called on the game thread.
    nlohmann::json ReadPlayerMarker();

    // Returns the live TESObjectREFR* for the player-placed custom map
    // marker, or nullptr if the player has never opened the map menu in this
    // save (the engine creates the ref on demand).
    // Used by both PlayerReader::ReadPlayerMarker and the GameWriter set/clear
    // commands so they share a single resolution path.
    // Must be called on the game thread.
    RE::TESObjectREFR* GetPlayerMarkerRef();

    // Builds the same JSON payload as ReadPlayerMarker but for an explicit
    // ref pointer. Pass nullptr to get the "not set" payload. Used by the
    // GameWriter set/clear commands so their result shape matches the reader.
    nlohmann::json BuildPlayerMarkerJson(RE::TESObjectREFR* ref);

    // Returns a JSON object describing the current game / player state.
    // Lets clients tell whether the player can act right now (paused, loading,
    // dialogue, combat, controls disabled, etc.).
    // Must be called on the game thread.
    nlohmann::json ReadGameStatus();
}
