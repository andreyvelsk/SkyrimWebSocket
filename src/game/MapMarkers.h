#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string_view>

namespace MapMarkers
{
    // Stable name for a MapMarkerData type id ("Cave", "City", ...).
    std::string_view TypeName(std::uint16_t a_typeId);

    // Returns an array of map markers currently shown on the player's world
    // map (i.e. markers whose MapMarkerData::Flag::kVisible bit is set).
    // Each entry: { "refId", "name", "type", "typeId", "x", "y", "isVisible", "canFastTravel" }
    // Must be called on the game thread.
    nlohmann::json ReadMapMarkers();

    // Same as ReadMapMarkers but returns ALL map markers in every loaded
    // worldspace, including undiscovered/hidden ones.
    // Must be called on the game thread.
    nlohmann::json ReadMapMarkersAll();
}