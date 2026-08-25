#pragma once

#include "Common.h"

namespace QuestCommands
{
    using Common::CommandResult;

    // Sets or clears the active/tracked state of a running quest. Dispatches
    // Papyrus Quest.SetActive(active) and updates the QuestFlag::kActive bit
    // so Player::Quests and Map::Markers::Quests reflect the new state in
    // the same command response.
    // Must be called on the game thread.
    CommandResult SetQuestActive(RE::FormID formId, bool active);

    // Overrides the journal's master Miscellaneous-objectives toggle until
    // the next save load (delegates the state change to QuestMarkers).
    // Must be called on the game thread.
    CommandResult SetMiscObjectivesVisible(bool visible);
}
