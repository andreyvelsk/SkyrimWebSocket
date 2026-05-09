#include "QuestReader.h"
#include "PlayerReader.h"
#include "QuestText.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <unordered_map>
#include <utility>
#include <vector>

namespace QuestReader
{
    namespace
    {
        struct ObjectiveRuntimeInfo
        {
            RE::QUEST_OBJECTIVE_STATE state = RE::QUEST_OBJECTIVE_STATE::kDormant;
            std::uint32_t             instanceID = 0;
            std::size_t               order = 0;
        };

        struct QuestLogEntry
        {
            std::uint16_t stage = 0;
            std::int8_t   itemIndex = 0;
            std::uint32_t instanceID = 0;
            std::string   textRaw;
            std::string   text;
        };

        std::size_t RuntimeObjectiveOffset()
        {
            return REL::Module::IsAE() ? 0x588 : 0x580;
        }

        std::size_t QuestLogOffset()
        {
            return REL::Module::IsAE() ? 0x578 : 0x570;
        }

        std::unordered_map<RE::BGSQuestObjective*, ObjectiveRuntimeInfo>
        BuildRuntimeObjectiveMap(RE::PlayerCharacter* player)
        {
            std::unordered_map<RE::BGSQuestObjective*, ObjectiveRuntimeInfo> result;
            if (!player || REL::Module::IsVR())
                return result;

            const auto base = reinterpret_cast<std::uintptr_t>(player);
            const auto& instances =
                *reinterpret_cast<const RE::BSTArray<RE::BGSInstancedQuestObjective>*>(base + RuntimeObjectiveOffset());

            std::size_t order = 0;
            for (const auto& inst : instances) {
                if (!inst.Objective)
                    continue;

                auto& info = result[inst.Objective];
                if (info.instanceID == 0 || inst.InstanceState == RE::QUEST_OBJECTIVE_STATE::kDisplayed) {
                    info.state = inst.InstanceState;
                    info.instanceID = inst.instanceID;
                    info.order = order;
                }
                ++order;
            }
            return result;
        }

        std::uint32_t FindStageItemInstanceID(RE::TESQuest* quest, RE::TESQuestStageItem* item)
        {
            if (!quest || !item)
                return 0;

            const std::uint16_t stage = item->owningStage ? item->owningStage->data.index : 0;
            for (auto* data : quest->instanceData) {
                if (!data)
                    continue;
                if (data->journalStage == stage && data->journalStageItem == item->index)
                    return data->id;
            }
            return quest->currentInstanceID;
        }

        std::string ReadStageLogText(RE::TESQuestStageItem* item)
        {
            if (!item || !item->hasLogEntry || item->logEntry.id == 0)
                return {};

            return {};
        }

        std::unordered_map<RE::TESQuest*, std::vector<QuestLogEntry>>
        BuildQuestLogMap(RE::PlayerCharacter* player)
        {
            std::unordered_map<RE::TESQuest*, std::vector<QuestLogEntry>> result;
            if (!player || REL::Module::IsVR())
                return result;

            const auto base = reinterpret_cast<std::uintptr_t>(player);
            auto& questLog =
                *reinterpret_cast<RE::BSSimpleList<RE::TESQuestStageItem*>*>(base + QuestLogOffset());

            for (auto* item : questLog) {
                if (!item || !item->owner)
                    continue;

                const auto raw = ReadStageLogText(item);
                if (raw.empty())
                    continue;

                const auto instanceID = FindStageItemInstanceID(item->owner, item);
                QuestLogEntry entry;
                entry.stage = item->owningStage ? item->owningStage->data.index : 0;
                entry.itemIndex = item->index;
                entry.instanceID = instanceID;
                entry.textRaw = raw;
                entry.text = QuestText::ResolveText(item->owner, raw, instanceID);
                result[item->owner].push_back(std::move(entry));
            }
            return result;
        }

        std::vector<QuestLogEntry> GetQuestLogEntries(RE::TESQuest* quest)
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto logMap = BuildQuestLogMap(player);
            if (auto it = logMap.find(quest); it != logMap.end())
                return it->second;
            return {};
        }

        nlohmann::json BuildQuestJson(RE::TESQuest* quest,
                                      const std::unordered_map<RE::BGSQuestObjective*, ObjectiveRuntimeInfo>& runtimeInfo,
                                      const std::vector<QuestLogEntry>& logEntries)
        {
            nlohmann::json out = nlohmann::json::object();
            if (!quest)
                return out;

            const bool isMisc = quest->GetType() == RE::QUEST_DATA::Type::kMiscellaneous;
            const char* rawNamePtr = quest->GetFullName();
            const std::string nameRaw = rawNamePtr ? rawNamePtr : "";
            std::string name = QuestText::ResolveQuestName(quest, quest->currentInstanceID);

            nlohmann::json steps = nlohmann::json::array();
            for (auto* objective : quest->objectives) {
                if (!objective)
                    continue;

                ObjectiveRuntimeInfo info;
                const auto staticState = static_cast<RE::QUEST_OBJECTIVE_STATE>(objective->state.underlying());
                if (auto found = runtimeInfo.find(objective); found != runtimeInfo.end()) {
                    info = found->second;
                    if (!QuestText::IsObjectiveVisibleInJournal(info.state) &&
                        QuestText::IsObjectiveVisibleInJournal(staticState)) {
                        info.state = staticState;
                    }
                } else {
                    info.state = staticState;
                    info.instanceID = QuestText::FindObjectiveInstanceID(RE::PlayerCharacter::GetSingleton(), objective);
                }

                if (!QuestText::IsObjectiveVisibleInJournal(info.state))
                    continue;

                const std::string textRaw = objective->displayText.c_str()
                                                ? objective->displayText.c_str()
                                                : "";
                const std::string text = QuestText::ResolveText(quest, textRaw, info.instanceID);

                nlohmann::json step;
                step["index"] = objective->index;
                step["text"] = text;
                step["textRaw"] = textRaw;
                step["completed"] = QuestText::IsObjectiveCompleted(info.state);
                step["failed"] = QuestText::IsObjectiveFailed(info.state);
                step["state"] = QuestText::ObjectiveStateName(info.state);
                step["stateRaw"] = static_cast<std::uint8_t>(info.state);
                step["instanceId"] = info.instanceID;
                step["runtimeOrder"] = info.order;
                steps.push_back(std::move(step));

                if (name.empty() && isMisc && !text.empty())
                    name = text;
            }

            std::sort(steps.begin(), steps.end(), [](const nlohmann::json& lhs, const nlohmann::json& rhs) {
                const auto leftIndex = lhs.value("index", 0);
                const auto rightIndex = rhs.value("index", 0);
                if (leftIndex != rightIndex)
                    return leftIndex < rightIndex;
                return lhs.value("runtimeOrder", std::size_t{0}) < rhs.value("runtimeOrder", std::size_t{0});
            });
            for (auto& step : steps)
                step.erase("runtimeOrder");

            std::string description;
            std::string descriptionRaw;
            std::uint16_t descriptionStage = 0;
            if (!logEntries.empty()) {
                const auto latestIt = std::max_element(
                    logEntries.begin(),
                    logEntries.end(),
                    [](const QuestLogEntry& lhs, const QuestLogEntry& rhs) {
                        if (lhs.stage != rhs.stage)
                            return lhs.stage < rhs.stage;
                        return lhs.itemIndex < rhs.itemIndex;
                    });
                const auto& latest = *latestIt;
                description = latest.text;
                descriptionRaw = latest.textRaw;
                descriptionStage = latest.stage;
            }

            out["questFormId"] = std::format("0x{:08X}", quest->GetFormID());
            out["questEditorId"] = quest->GetFormEditorID() ? quest->GetFormEditorID() : "";
            out["name"] = name;
            out["nameRaw"] = nameRaw;
            out["description"] = description;
            out["descriptionRaw"] = descriptionRaw;
            out["descriptionStage"] = descriptionStage;
            out["type"] = QuestText::QuestTypeName(quest->GetType());
            out["questType"] = QuestText::QuestTypeName(quest->GetType());
            out["isMisc"] = isMisc;
            out["isActive"] = quest->IsActive();
            out["isRunning"] = quest->IsRunning();
            out["isCompleted"] = quest->IsCompleted();
            out["currentStage"] = quest->GetCurrentStageID();
            out["currentInstanceId"] = quest->currentInstanceID;
            out["steps"] = std::move(steps);

            if (isMisc) {
                const auto miscVisibility = PlayerReader::ReadMiscQuestMarkerVisibility();
                out["miscMarkersVisible"] = miscVisibility.value("visible", true);
                out["miscMarkersVisibilityKnown"] = miscVisibility.value("known", false);
                out["miscMarkersVisibilitySource"] = miscVisibility.value("source", "defaultVisible");
            }

            return out;
        }

        bool ShouldIncludeQuest(const nlohmann::json& questJson)
        {
            if (!questJson.value("isRunning", false) || questJson.value("isCompleted", false))
                return false;
            if (questJson.contains("steps") && questJson["steps"].is_array() && !questJson["steps"].empty())
                return true;
            if (!questJson.value("description", std::string()).empty())
                return true;
            if (questJson.value("isActive", false) && !questJson.value("name", std::string()).empty())
                return true;
            return false;
        }
    }

    nlohmann::json BuildQuestJson(RE::TESQuest* quest)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        return BuildQuestJson(quest,
                              BuildRuntimeObjectiveMap(player),
                              GetQuestLogEntries(quest));
    }

    nlohmann::json ReadQuests()
    {
        nlohmann::json out = nlohmann::json::array();

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler)
            return out;

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto runtimeInfo = BuildRuntimeObjectiveMap(player);
        auto questLog = BuildQuestLogMap(player);

        for (auto* quest : dataHandler->GetFormArray<RE::TESQuest>()) {
            if (!quest || !quest->IsRunning() || quest->IsCompleted())
                continue;

            const auto logs = [&]() -> std::vector<QuestLogEntry> {
                if (auto found = questLog.find(quest); found != questLog.end())
                    return found->second;
                return {};
            }();

            auto entry = BuildQuestJson(quest, runtimeInfo, logs);
            if (ShouldIncludeQuest(entry))
                out.push_back(std::move(entry));
        }

        std::sort(out.begin(), out.end(), [](const nlohmann::json& lhs, const nlohmann::json& rhs) {
            const bool leftMisc = lhs.value("isMisc", false);
            const bool rightMisc = rhs.value("isMisc", false);
            if (leftMisc != rightMisc)
                return !leftMisc;

            const auto leftName = lhs.value("name", std::string());
            const auto rightName = rhs.value("name", std::string());
            if (leftName != rightName)
                return leftName < rightName;
            return lhs.value("questFormId", std::string()) < rhs.value("questFormId", std::string());
        });

        return out;
    }
}