#include "QuestReader.h"
#include "PlayerReader.h"
#include "QuestText.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <format>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace logger = SKSE::log;

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

        struct LocalizedStringFile
        {
            bool                                         loaded = false;
            std::unordered_map<std::uint32_t, std::string> entries;
        };

        struct StringDirectoryEntry
        {
            std::uint32_t id = 0;
            std::uint32_t offset = 0;
        };

        std::unordered_map<std::string, LocalizedStringFile> g_localizedStringFiles;

        std::string ToLowerAscii(std::string value)
        {
            for (char& ch : value)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return value;
        }

        std::string StringFileStem(std::string_view filename)
        {
            const auto slash = filename.find_last_of("/\\");
            if (slash != std::string_view::npos)
                filename.remove_prefix(slash + 1);

            const auto dot = filename.find_last_of('.');
            if (dot != std::string_view::npos)
                filename = filename.substr(0, dot);

            return std::string(filename);
        }

        std::string CurrentLanguage()
        {
            auto languageJson = PlayerReader::ReadLanguage();
            if (!languageJson.is_string())
                return "english";
            auto language = languageJson.get<std::string>();
            if (language.empty())
                return "english";
            return ToLowerAscii(std::move(language));
        }

        template <class T>
        bool ReadValue(RE::NiBinaryStream& stream, T& value)
        {
            return stream.read(reinterpret_cast<char*>(&value), sizeof(T));
        }

        std::string ReadSizedString(const std::vector<char>& data, std::uint32_t offset)
        {
            if (offset >= data.size())
                return {};

            if (offset + sizeof(std::uint32_t) <= data.size()) {
                std::uint32_t length = 0;
                std::memcpy(&length, data.data() + offset, sizeof(length));
                const auto textOffset = offset + static_cast<std::uint32_t>(sizeof(length));
                if (length > 0 && textOffset <= data.size() && length <= data.size() - textOffset) {
                    std::string value(data.data() + textOffset, data.data() + textOffset + length);
                    while (!value.empty() && value.back() == '\0')
                        value.pop_back();
                    return value;
                }
            }

            const char* begin = data.data() + offset;
            const char* end = data.data() + data.size();
            const char* nul = std::find(begin, end, '\0');
            return std::string(begin, nul);
        }

        LocalizedStringFile LoadLocalizedStringFile(std::string_view path)
        {
            LocalizedStringFile result;

            RE::BSResourceNiBinaryStream stream{ std::string(path) };
            if (!stream.good()) {
                logger::debug("[Player::Quests] localized string file not found: {}", path);
                result.loaded = true;
                return result;
            }

            std::uint32_t count = 0;
            std::uint32_t dataSize = 0;
            if (!ReadValue(stream, count) || !ReadValue(stream, dataSize)) {
                logger::debug("[Player::Quests] failed to read localized string header: {}", path);
                result.loaded = true;
                return result;
            }

            static constexpr std::uint32_t kMaxStringCount = 1'000'000;
            static constexpr std::uint32_t kMaxDataSize = 256 * 1024 * 1024;
            if (count > kMaxStringCount || dataSize > kMaxDataSize) {
                logger::debug("[Player::Quests] localized string file rejected: {} count={} dataSize={}",
                              path, count, dataSize);
                result.loaded = true;
                return result;
            }

            std::vector<StringDirectoryEntry> directory(count);
            for (auto& entry : directory) {
                if (!ReadValue(stream, entry.id) || !ReadValue(stream, entry.offset)) {
                    logger::debug("[Player::Quests] failed to read localized string directory: {}", path);
                    result.loaded = true;
                    return result;
                }
            }

            std::vector<char> data(dataSize);
            if (dataSize > 0 && !stream.read(data.data(), dataSize)) {
                logger::debug("[Player::Quests] failed to read localized string data: {}", path);
                result.loaded = true;
                return result;
            }

            result.entries.reserve(directory.size());
            for (const auto& entry : directory) {
                if (entry.offset >= data.size())
                    continue;
                auto value = ReadSizedString(data, entry.offset);
                if (!value.empty())
                    result.entries.emplace(entry.id, std::move(value));
            }

            logger::debug("[Player::Quests] loaded {} localized journal strings from {}",
                          result.entries.size(), path);
            result.loaded = true;
            return result;
        }

        const std::unordered_map<std::uint32_t, std::string>& LocalizedStringsFor(const RE::TESFile* file)
        {
            static const std::unordered_map<std::uint32_t, std::string> kEmpty;
            if (!file)
                return kEmpty;

            const auto stem = StringFileStem(file->GetFilename());
            if (stem.empty())
                return kEmpty;

            const std::string path = std::format("Strings/{}_{}.DLSTRINGS", stem, CurrentLanguage());
            auto& cached = g_localizedStringFiles[path];
            if (!cached.loaded)
                cached = LoadLocalizedStringFile(path);
            return cached.entries;
        }

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

            auto* file = item->owner ? item->owner->GetDescriptionOwnerFile() : nullptr;
            if (!file && item->owner)
                file = item->owner->GetFile();

            const auto& strings = LocalizedStringsFor(file);
            if (auto found = strings.find(item->logEntry.id); found != strings.end())
                return found->second;

            logger::debug("[Player::Quests] unresolved journal log string id=0x{:08X} quest={} file={}",
                          item->logEntry.id,
                          item->owner && item->owner->GetFormEditorID() ? item->owner->GetFormEditorID() : "",
                          file ? file->GetFilename() : "");
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
                                      const std::vector<QuestLogEntry>& logEntries,
                                      const nlohmann::json& miscMarkerVisibility)
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
                out["miscMarkersVisible"] = miscMarkerVisibility.value("visible", true);
                out["miscMarkersVisibilityKnown"] = miscMarkerVisibility.value("known", false);
                out["miscMarkersVisibilitySource"] = miscMarkerVisibility.value("source", std::string("default-visible"));
            }

            return out;
        }

        bool ShouldIncludeQuest(const nlohmann::json& questJson)
        {
            if (!questJson.value("isRunning", false))
                return false;
            
            // Filter out quests without meaningful content
            const bool hasSteps = questJson.contains("steps") && questJson["steps"].is_array() && !questJson["steps"].empty();
            const bool hasDescription = !questJson.value("description", std::string()).empty();
            const bool hasName = !questJson.value("name", std::string()).empty();
            const bool isActive = questJson.value("isActive", false);
            
            // Include quest if:
            // 1. It has steps OR
            // 2. It has description AND is active OR
            // 3. It is active and has a name
            if (hasSteps)
                return true;
            if ((hasDescription || hasName) && isActive)
                return true;
            
            return false;
        }
    }

    nlohmann::json BuildQuestJson(RE::TESQuest* quest)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        return BuildQuestJson(quest,
                              BuildRuntimeObjectiveMap(player),
                              GetQuestLogEntries(quest),
                              PlayerReader::ReadMiscQuestMarkerVisibility());
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
        const auto miscMarkerVisibility = PlayerReader::ReadMiscQuestMarkerVisibility();
        for (auto* quest : dataHandler->GetFormArray<RE::TESQuest>()) {
            if (!quest || !quest->IsRunning())
                continue;

            const auto logs = [&]() -> std::vector<QuestLogEntry> {
                if (auto found = questLog.find(quest); found != questLog.end())
                    return found->second;
                return {};
            }();

            auto entry = BuildQuestJson(quest, runtimeInfo, logs, miscMarkerVisibility);
            if (ShouldIncludeQuest(entry)) {
                out.push_back(std::move(entry));
            } else {
                // Debug: log why this quest was excluded
                const auto questId = std::format("0x{:08X}", quest->GetFormID());
                const auto questEditorId = quest->GetFormEditorID() ? quest->GetFormEditorID() : "<none>";
                const bool hasSteps = entry.contains("steps") && entry["steps"].is_array() && !entry["steps"].empty();
                const bool hasDescription = !entry.value("description", std::string()).empty();
                const bool hasName = !entry.value("name", std::string()).empty();
                const bool isActive = entry.value("isActive", false);
                logger::debug("[Player::Quests] Excluded quest {} ({}) - isActive={}, hasSteps={}, hasDescription={}, hasName={}, isRunning={}, isCompleted={}",
                             questId, questEditorId, isActive, hasSteps, hasDescription, hasName,
                             quest->IsRunning(), quest->IsCompleted());
            }
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