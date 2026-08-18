#pragma once

#include "Common.h"

namespace MapCommands
{
    using Common::CommandResult;

    // ─── Player-placed map marker ─────────────────────────────────────────
    //
    // The "player marker" is the marker the player can drop on the world map
    // by clicking on it (also known as the custom map marker). The engine
    // keeps a single dedicated TESObjectREFR for it (PlayerCharacter::
    // GetInfoRuntimeData().playerMapMarker), and toggles its
    // ExtraMapMarker visibility instead of creating new refs.
    //
    // Both commands return `data` set to the same JSON shape as the
    // `Player::Marker` field — the marker's current state AFTER the
    // operation, so clients can confirm the new coordinates / cleared
    // status in a single round-trip.

    // Place or move the player's map marker to (a_x, a_y, a_z) in the
    // marker's current worldspace (which is always the global world map —
    // typically Tamriel — so callers normally pass parent-worldspace
    // coordinates). The marker is automatically made visible.
    // Fails when the marker ref does not yet exist (engine creates it on
    // first map-menu open in a save).
    // Must be called on the game thread.
    CommandResult SetPlayerMarker(float a_x, float a_y, float a_z);

    // Hide / clear the player's map marker. The underlying ref is preserved
    // (the engine reuses it next time the player places one), only the
    // ExtraMapMarker visibility flag is cleared.
    // Must be called on the game thread.
    CommandResult ClearPlayerMarker();

    // ─── Fast travel ──────────────────────────────────────────────

    // Trigger a real fast travel to the map-marker reference identified by
    // formId.  Internally dispatches the Papyrus static `Game.FastTravel(ref)`,
    // which routes through the engine's full fast-travel pipeline (fade
    // animation, in-game time advancement, random-encounter rolls, weather
    // reset, autosave, follower transfer, `PlayerFlags::fastTraveling`).
    //
    // Mirrors the in-game fast-travel pre-flight checks before dispatching:
    //   * the form must exist and be a TESObjectREFR
    //   * the ref must carry an ExtraMapMarker with MapMarkerData
    //   * the marker must have MapMarkerData::Flag::kVisible (i.e. discovered)
    //   * the marker must have MapMarkerData::Flag::kCanTravelTo set
    //   * the marker reference must not be disabled or deleted
    //   * the marker's parent worldspace must not have kCantFastTravel
    //   * the player must not be in combat
    //   * if the player is in an interior cell, kCanTravelFromHere must be set
    //   * if the player is in an exterior cell, the worldspace must not have
    //     kCantFastTravel (e.g. Enderal)
    //   * on failure, a native HUD notification is shown ("Cannot fast travel
    //     from this location") matching vanilla behaviour
    //
    // Returns the same JSON shape used by Map::Markers::Locations entries on success
    // (refId, name, type, typeId, x, y, isVisible, canFastTravel) so callers
    // can confirm the destination in a single round-trip.  The success
    // response is sent immediately after the VM dispatch is queued; actual
    // arrival happens asynchronously inside the engine.
    // Must be called on the game thread.
    CommandResult FastTravelToMarker(RE::FormID formId);
}