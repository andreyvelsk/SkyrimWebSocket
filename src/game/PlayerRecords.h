#pragma once

#include <nlohmann/json.hpp>

// General stats (the journal's "General Stats" page) and active magic effects.
namespace PlayerRecords
{
    // Game thread. Values of the journal's general stats, grouped the way the
    // journal shows them:
    //   { "General": [ { "name", "value" }, ... ], "Quest": [...], "Combat": [...],
    //     "Magic": [...], "Crafting": [...], "Crime": [...] }
    // Values come from Papyrus Game.QueryStat, which answers asynchronously:
    // each read returns the latest cached values and refreshes the cache in
    // the background (at most every few seconds). The first read after load
    // may therefore be empty or partial.
    nlohmann::json ReadMiscStats();

    // Game thread. Active magic effects on the player, like the Active
    // Effects menu: [ { "name", "source", "magnitude", "duration", "elapsed",
    // "remaining", "detrimental" } ]. Hidden and inactive effects are skipped.
    // duration 0 = permanent (abilities, worn enchantments).
    nlohmann::json ReadActiveEffects();
}
