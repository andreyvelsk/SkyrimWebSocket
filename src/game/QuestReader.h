#pragma once

#include <nlohmann/json.hpp>

namespace QuestReader
{
    nlohmann::json ReadQuests();
    nlohmann::json BuildQuestJson(RE::TESQuest* quest);
}