#pragma once

#include <nlohmann/json.hpp>

namespace QuestReader
{
    // Returns an array of regular (non-miscellaneous) active/known quests with
    // their objectives ("tasks").  Quest name and task text come from the game
    // engine already localized to the current game language (sLanguage:General).
    //
    // Shape: [
    //   {
    //     "questId":     "0x000NNNNN",
    //     "name":        "Quest name",
    //     "description": "",               // See Quests.md for why this is empty.
    //     "isActive":    bool,             // HUD/compass-tracked quest
    //     "isCompleted": bool,
    //     "tasks": [
    //       { "name": "Objective text", "isCompleted": bool }
    //     ]
    //   }, ...
    // ]
    //
    // Must be called on the game thread.
    nlohmann::json ReadAll();

    // Returns an array of Miscellaneous-type quests ("Разное" in Russian).
    // Miscellaneous quests have no objective list in the journal — each quest
    // is one-shot, so the shape is flatter:
    //
    // [
    //   {
    //     "isOther":     true,
    //     "questId":     "0x000NNNNN",
    //     "name":        "Quest name",
    //     "isActive":    bool,
    //     "isCompleted": bool
    //   }, ...
    // ]
    //
    // Must be called on the game thread.
    nlohmann::json ReadOthers();
}
