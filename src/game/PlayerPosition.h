#pragma once

#include <RE/Skyrim.h>
#include <nlohmann/json.hpp>

namespace PlayerPosition
{
    // Resolve the worldspace the player is currently "in" for map purposes.
    // Returns nullptr when every fallback fails.
    // Must be called on the game thread.
    RE::TESWorldSpace* ResolvePlayerWorldspace();

    // Returns the player's current world position and heading as a JSON object:
    // { "x", "y", "z", "angle",
    //   "worldspace", "worldspaceFormId",
    //   "parentWorldspace", "parentWorldspaceFormId",
    //   "cell", "cellFormId", "isInterior" }
    // Must be called on the game thread.
    nlohmann::json ReadPosition();

    // Returns the map-position to use when rendering the player on a global world map.
    // Must be called on the game thread.
    nlohmann::json ReadExteriorPosition();

    // Returns the live TESObjectREFR* for the player-placed custom map
    // marker, or nullptr if the player has never opened the map menu in this
    // save (the engine creates the ref on demand).
    // Must be called on the game thread.
    RE::TESObjectREFR* GetPlayerMarkerRef();

    // Builds the same JSON payload as ReadPlayerMarker but for an explicit
    // ref pointer. Pass nullptr to get the "not set" payload.
    nlohmann::json BuildPlayerMarkerJson(RE::TESObjectREFR* ref);

    // Returns a JSON object describing the player-placed custom map marker:
    // { "isSet", "x", "y", "z",
    //   "worldspace", "worldspaceFormId",
    //   "parentWorldspace", "parentWorldspaceFormId" }
    // Must be called on the game thread.
    nlohmann::json ReadPlayerMarker();
}