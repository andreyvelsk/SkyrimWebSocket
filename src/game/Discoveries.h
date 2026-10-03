#pragma once

#include <nlohmann/json.hpp>

// Locations the player discovers ("Discovered: Bleak Falls Barrow").
// The engine's LocationDiscovery event feeds a small ring buffer that clients
// poll; each entry has a sequence number so a client can tell new entries
// from ones it has seen.
namespace Discoveries
{
    // Game thread (called from the LocationDiscovery sink).
    void Record(const RE::LocationDiscovery::Event& a_event);

    // Player::Discoveries:
    //   { "seq": <last sequence number, 0 = none yet>,
    //     "recent": [ { "seq", "name", "type", "worldspace", "x", "y" } ] }
    // Newest last; at most 20 entries. x/y are the player's position when the
    // discovery fired (the player is next to the marker at that moment).
    nlohmann::json Read();
}
