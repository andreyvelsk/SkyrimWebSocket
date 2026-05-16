#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace QuestText
{
    const char* QuestTypeName(RE::QUEST_DATA::Type type);
    const char* ObjectiveStateName(RE::QUEST_OBJECTIVE_STATE state);

    bool IsObjectiveCompleted(RE::QUEST_OBJECTIVE_STATE state);
    bool IsObjectiveFailed(RE::QUEST_OBJECTIVE_STATE state);
    bool IsObjectiveVisibleInJournal(RE::QUEST_OBJECTIVE_STATE state);

    std::uint32_t FindObjectiveInstanceID(RE::PlayerCharacter* player,
                                          RE::BGSQuestObjective* objective);

    std::string ResolveText(RE::TESQuest* quest,
                            std::string_view raw,
                            std::uint32_t instanceID = 0);

    std::string ResolveQuestName(RE::TESQuest* quest,
                                 std::uint32_t instanceID = 0);
}