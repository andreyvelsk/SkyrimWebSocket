#include "QuestCommands.h"
#include "Common.h"
#include "QuestMarkers.h"
#include "QuestReader.h"
#include "../Utils.h"

#include <format>

namespace logger = SKSE::log;

namespace QuestCommands
{
    CommandResult SetQuestActive(RE::FormID formId, bool active)
    {
        auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(formId);
        if (!quest)
            return {false, std::format("Form 0x{:08X} is not a quest", formId)};

        if (active && !quest->IsRunning())
            return {false, std::format("Quest 0x{:08X} is not running", formId)};
        if (active && quest->IsCompleted())
            return {false, std::format("Quest 0x{:08X} is already completed", formId)};

        const bool dispatched = Common::DispatchFormMethod(quest, "Quest", "SetActive", active);
        if (!dispatched)
            return {false, "Failed to dispatch Quest.SetActive"};

        if (active)
            quest->data.flags.set(RE::QuestFlag::kActive);
        else
            quest->data.flags.reset(RE::QuestFlag::kActive);

        CommandResult result;
        result.success = true;
        result.data = QuestReader::BuildQuestJson(quest);

        const char* name = quest->GetFullName();
        PrintConsole(std::format("[WS] Quest 0x{:08X} {} ({})",
                                 formId,
                                 active ? "tracked" : "untracked",
                                 name && *name ? name : "unnamed"));
        return result;
    }

    CommandResult SetMiscObjectivesVisible(bool visible)
    {
        QuestMarkers::SetMiscObjectivesVisible(visible);
        CommandResult result;
        result.success = true;
        result.data = { { "miscObjectivesVisible", visible } };
        PrintConsole(std::format("[WS] Misc objectives {}",
                                 visible ? "shown" : "hidden"));
        return result;
    }
}