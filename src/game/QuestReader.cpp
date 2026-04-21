#include "QuestReader.h"
#include "../Utils.h"

#include <format>
#include <string>

namespace QuestReader
{
    // ─── Helpers ──────────────────────────────────────────────────────────

    // A quest is "relevant" to surface to the client only if the engine has
    // set kDisplayedInHUD on it.  That flag is set exactly for quests that
    // appear in the player's journal — it excludes dialogue quests, creature-AI
    // quests, and other internal engine quests that are technically "enabled"
    // but never shown to the player.  The flag persists after completion, so
    // finished journal quests are included as well.
    static bool IsRelevantQuest(const RE::TESQuest* quest)
    {
        if (!quest)
            return false;
        const char* name = quest->GetName();
        if (!name || !*name)
            return false;
        return quest->data.flags.all(RE::QuestFlag::kDisplayedInHUD);
    }

    // The "Miscellaneous" category is how the vanilla journal groups simple
    // one-shot tasks (bounty letters, fetch quests, ...).  In data this is
    // QUEST_DATA::Type::kMiscellaneous.
    static bool IsMiscellaneous(const RE::TESQuest* quest)
    {
        return quest && quest->GetType() == RE::QUEST_DATA::Type::kMiscellaneous;
    }

    // True when the objective is currently visible in the journal (or was
    // visible and is now resolved).  Dormant objectives are internal stages
    // the engine hasn't surfaced to the player yet.
    static bool IsObjectiveVisible(SKSE::stl::enumeration<RE::QUEST_OBJECTIVE_STATE, uint8_t> state)
    {
        using S = RE::QUEST_OBJECTIVE_STATE;
        return state == S::kDisplayed || state == S::kCompletedDisplayed ||
               state == S::kCompleted || state == S::kFailed || state == S::kFailedDisplayed;
    }

    static bool IsObjectiveCompleted(SKSE::stl::enumeration<RE::QUEST_OBJECTIVE_STATE, uint8_t> state)
    {
        using S = RE::QUEST_OBJECTIVE_STATE;
        return state == S::kCompleted || state == S::kCompletedDisplayed;
    }

    static nlohmann::json BuildTaskJson(const RE::BGSQuestObjective* obj)
    {
        nlohmann::json j;
        // displayText is a BSFixedString populated from the NNAM record, which
        // the engine resolves against the currently-loaded STRINGS file — so
        // the text is already in the game's current language.
        const char* text = obj->displayText.c_str();
        j["name"]        = text ? text : "";
        j["isCompleted"] = IsObjectiveCompleted(obj->state);
        return j;
    }

    static nlohmann::json BuildQuestJson(RE::TESQuest* quest, bool includeTasks)
    {
        nlohmann::json j;
        // GetName() comes from TESFullName (FULL record) which is localized by
        // the engine, matching the game's sLanguage:General setting.
        j["questId"]     = std::format("0x{:08X}", quest->GetFormID());
        j["name"]        = quest->GetName();
        j["isActive"]    = quest->IsActive();
        j["isCompleted"] = quest->IsCompleted();

        if (includeTasks) {
            j["description"] = "";  // See docs/Quests.md for the rationale.
            nlohmann::json tasks = nlohmann::json::array();
            for (auto* obj : quest->objectives) {
                if (!obj)
                    continue;
                if (!IsObjectiveVisible(obj->state))
                    continue;
                tasks.push_back(BuildTaskJson(obj));
            }
            j["tasks"] = std::move(tasks);
        } else {
            j["isOther"] = true;
        }
        return j;
    }

    // ─── Public API ───────────────────────────────────────────────────────

    nlohmann::json ReadAll()
    {
        nlohmann::json out = nlohmann::json::array();
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data)
            return out;

        for (auto* quest : data->GetFormArray<RE::TESQuest>()) {
            if (!IsRelevantQuest(quest))
                continue;
            if (IsMiscellaneous(quest))
                continue;  // Miscellaneous quests are returned via ReadOthers().
            out.push_back(BuildQuestJson(quest, /*includeTasks=*/true));
        }
        return out;
    }

    nlohmann::json ReadOthers()
    {
        nlohmann::json out = nlohmann::json::array();
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data)
            return out;

        for (auto* quest : data->GetFormArray<RE::TESQuest>()) {
            if (!IsRelevantQuest(quest))
                continue;
            if (!IsMiscellaneous(quest))
                continue;
            out.push_back(BuildQuestJson(quest, /*includeTasks=*/false));
        }
        return out;
    }
}
