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

    // Returns the last known exterior position cached by the game (used by the
    // compass / world map). Useful for placing the player on the global map while
    // they are inside an interior cell or a city sub-worldspace.
    // Shape: { "x", "y", "z",
    //          "worldspace", "worldspaceFormId",
    //          "parentWorldspace", "parentWorldspaceFormId" }
    // worldspace fields are null until the game has cached an exterior location.
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
    // this uses PlayerCharacter::questTargets, the same runtime map the engine
    // uses for tracked markers. VR uses a best-effort static fallback. Multiple
    // target aliases that resolve to the same marker destination are collapsed.
    // Each entry: { questFormId, questEditorId, questName, questType,
    //               objectiveIndex, objectiveText, objectiveTextResolved, aliasId,
    //               refId, isDeleted, name, x, y, z,
    //               worldspace, worldspaceFormId,
    //               parentWorldspace, parentWorldspaceFormId,
    //               cell, cellFormId, isInterior }.
    // Must be called on the game thread.
    nlohmann::json ReadQuestMarkers();

    // Debug snapshot for diagnosing quest marker sources. Returns raw
    // PlayerCharacter questTargets/objectives summaries plus current
    // ReadQuestMarkers output. Intended for troubleshooting only.
    // Must be called on the game thread.
    nlohmann::json ReadQuestMarkersDebug();

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
