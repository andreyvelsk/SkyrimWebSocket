#include "QuestReader.h"
#include "../Utils.h"

#include <format>
#include <string>

namespace QuestReader
{
    // ─── Helpers ──────────────────────────────────────────────────────────

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

    // Miscellaneous quests use the same kDisplayedInHUD flag the journal sets
    // for all visible quests.  The Misc tab is just a UI grouping by questType.
    static bool IsRelevantMiscQuest(const RE::TESQuest* quest)
    {
        if (!quest)
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

    static const char* ObjectiveStateName(RE::QUEST_OBJECTIVE_STATE s)
    {
        using S = RE::QUEST_OBJECTIVE_STATE;
        switch (s) {
            case S::kDormant:            return "kDormant";
            case S::kDisplayed:          return "kDisplayed";
            case S::kCompleted:          return "kCompleted";
            case S::kCompletedDisplayed: return "kCompletedDisplayed";
            case S::kFailed:             return "kFailed";
            case S::kFailedDisplayed:    return "kFailedDisplayed";
            default:                     return "unknown";
        }
    }

    static const char* QuestTypeName(RE::QUEST_DATA::Type t)
    {
        using T = RE::QUEST_DATA::Type;
        switch (t) {
            case T::kNone:            return "None";
            case T::kMainQuest:       return "MainQuest";
            case T::kMagesGuild:      return "MagesGuild";
            case T::kThievesGuild:    return "ThievesGuild";
            case T::kDarkBrotherhood: return "DarkBrotherhood";
            case T::kCompanionsQuest: return "CompanionsQuest";
            case T::kMiscellaneous:   return "Miscellaneous";
            case T::kDaedric:         return "Daedric";
            case T::kSideQuest:       return "SideQuest";
            case T::kCivilWar:        return "CivilWar";
            case T::kDLC01_Vampire:   return "DLC01_Vampire";
            case T::kDLC02_Dragonborn:return "DLC02_Dragonborn";
            default:                  return "unknown";
        }
    }

    static nlohmann::json BuildTaskJson(const RE::BGSQuestObjective* obj)
    {
        nlohmann::json j;
        const char* text = obj->displayText.c_str();
        j["index"]       = obj->index;
        j["name"]        = text ? text : "";
        j["state"]       = ObjectiveStateName(obj->state.get());
        j["isCompleted"] = IsObjectiveCompleted(obj->state);
        j["isVisible"]   = IsObjectiveVisible(obj->state);
        return j;
    }

    static nlohmann::json BuildQuestJson(RE::TESQuest* quest)
    {
        nlohmann::json j;
        j["questId"]      = std::format("0x{:08X}", quest->GetFormID());
        j["editorId"]     = quest->GetFormEditorID() ? quest->GetFormEditorID() : "";
        j["name"]         = quest->GetName();
        j["type"]         = static_cast<int>(quest->GetType());
        j["typeName"]     = QuestTypeName(quest->GetType());
        j["flags"]        = std::format("0x{:04X}", quest->data.flags.underlying());
        j["isOther"]      = IsMiscellaneous(quest);
        j["isActive"]     = quest->IsActive();
        j["isEnabled"]    = quest->IsEnabled();
        j["isCompleted"]  = quest->IsCompleted();
        j["isRunning"]    = quest->IsRunning();
        j["isStopped"]    = quest->IsStopped();
        j["currentStage"] = quest->GetCurrentStageID();

        nlohmann::json objectives = nlohmann::json::array();
        for (auto* obj : quest->objectives) {
            if (!obj)
                continue;
            objectives.push_back(BuildTaskJson(obj));
        }
        j["objectives"] = std::move(objectives);

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
            if (!quest)
                continue;
            const char* name = quest->GetName();
            if (!name || !*name)
                continue;
            // Include any quest the engine considers journal-visible (kDisplayedInHUD),
            // regardless of type.  isOther=true marks Miscellaneous ones.
            if (!quest->data.flags.all(RE::QuestFlag::kDisplayedInHUD))
                continue;
            out.push_back(BuildQuestJson(quest));
        }
        return out;
    }

    nlohmann::json ReadOthers()
    {
        // DEBUG DUMP — no filtering, every TESQuest with every available field.
        // Use this to find which flag/field distinguishes Miscellaneous quests.
        nlohmann::json out = nlohmann::json::array();
        auto* handler = RE::TESDataHandler::GetSingleton();
        if (!handler)
            return out;

        for (auto* quest : handler->GetFormArray<RE::TESQuest>()) {
            if (!quest)
                continue;

            nlohmann::json j;

            // ── Identity ──────────────────────────────────────────────────
            j["questId"]  = std::format("0x{:08X}", quest->GetFormID());
            j["editorId"] = quest->GetFormEditorID() ? quest->GetFormEditorID() : "";
            const char* name = quest->GetName();
            j["name"]    = name ? name : "";
            j["hasName"] = name && *name;

            // ── Type ──────────────────────────────────────────────────────
            j["type"]     = static_cast<int>(quest->GetType());
            j["typeName"] = QuestTypeName(quest->GetType());

            // ── Quest data flags (raw + individual bits) ──────────────────
            const auto qf = quest->data.flags.underlying();
            j["questFlags"] = std::format("0x{:04X}", qf);
            // bit 0x0010 — quest starts enabled at game start
            j["flag_kStartsEnabled"] =
                quest->data.flags.all(RE::QuestFlag::kStartsEnabled);
            // bit that IsActive() tests
            j["flag_kActive"] =
                quest->data.flags.all(RE::QuestFlag::kActive);
            // the flag we use for Quests::Items
            j["flag_kDisplayedInHUD"] =
                quest->data.flags.all(RE::QuestFlag::kDisplayedInHUD);
            // run-once quests never restart
            j["flag_kRunOnce"] =
                quest->data.flags.all(RE::QuestFlag::kRunOnce);

            // ── TESForm base flags (deleted, ignored, …) ──────────────────
            j["formFlags"] = std::format("0x{:08X}", quest->formFlags);

            // ── Priority (0-255, higher = more important) ─────────────────
            j["priority"] = static_cast<int>(quest->data.priority);

            // ── Runtime state ─────────────────────────────────────────────
            j["isActive"]     = quest->IsActive();
            j["isEnabled"]    = quest->IsEnabled();
            j["isCompleted"]  = quest->IsCompleted();
            j["isRunning"]    = quest->IsRunning();
            j["isStopped"]    = quest->IsStopped();
            j["currentStage"] = quest->GetCurrentStageID();

            // ── Objectives summary ────────────────────────────────────────
            int totalObj = 0, visibleObj = 0;
            for (auto* obj : quest->objectives) {
                if (!obj)
                    continue;
                ++totalObj;
                if (IsObjectiveVisible(obj->state))
                    ++visibleObj;
            }
            j["objectivesTotal"]   = totalObj;
            j["objectivesVisible"] = visibleObj;

            out.push_back(std::move(j));
        }
        return out;
    }
}
