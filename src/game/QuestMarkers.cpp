#include "QuestMarkers.h"
#include "Common.h"
#include "PlayerPosition.h"
#include "QuestText.h"

#include "../../logger.h"

#include <RE/Skyrim.h>

#include <array>
#include <cctype>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace QuestMarkers
{
    namespace
    {
        bool g_miscObjectivesVisibilityKnown = false;
        bool g_miscObjectivesVisible         = true;
        const char* g_miscObjectivesVisibilitySource = "cached misc objectives visibility";

        // ---------------------------------------------------------------------------
        // Persistent-ref cache
        // ---------------------------------------------------------------------------
        struct PersistentRefCache
        {
            std::unordered_map<RE::FormID, RE::TESObjectREFR*> byFormId;
            std::unordered_map<RE::FormID, RE::TESObjectREFR*> markerByLocationId;
            std::unordered_map<std::string, RE::TESObjectREFR*> markerByName;
            bool built = false;
        };
        static PersistentRefCache s_persistentCache;

        struct MiscObjectivesVisibility
        {
            bool        visible       = true;
            bool        known         = false;
            const char* source        = "default-visible";
            bool        cachedKnown   = false;
            bool        cachedVisible = true;
            bool        journalOpen   = false;
            bool        scaleformKnown = false;
            bool        scaleformVisible = true;
            const char* scaleformSource = "none";
            bool        nativeKnown   = false;
            bool        nativeVisible = true;
        };

        struct ScaleformMiscVisibilityRead
        {
            bool        visible = true;
            const char* source  = "unknown Scaleform source";
        };

        struct MiscObjectivesVisibilityRead
        {
            bool        visible = true;
            const char* source  = "unknown";
            bool        scaleformKnown = false;
            bool        scaleformVisible = true;
            const char* scaleformSource = "none";
            bool        nativeKnown = false;
            bool        nativeVisible = true;
        };

        std::string_view QuestTypeName(RE::QUEST_DATA::Type t)
        {
            using T = RE::QUEST_DATA::Type;
            switch (t) {
            case T::kNone:             return "None";
            case T::kMainQuest:        return "MainQuest";
            case T::kMagesGuild:       return "MagesGuild";
            case T::kThievesGuild:     return "ThievesGuild";
            case T::kDarkBrotherhood:  return "DarkBrotherhood";
            case T::kCompanionsQuest:  return "Companions";
            case T::kMiscellaneous:    return "Miscellaneous";
            case T::kDaedric:          return "Daedric";
            case T::kSideQuest:        return "SideQuest";
            case T::kCivilWar:         return "CivilWar";
            case T::kDLC01_Vampire:    return "DLC01_Vampire";
            case T::kDLC02_Dragonborn: return "DLC02_Dragonborn";
            default:                   return "Unknown";
            }
        }

        std::optional<bool> ReadBoolLike(const RE::GFxValue& value)
        {
            if (value.IsBool())
                return value.GetBool();
            if (value.IsNumber())
                return value.GetNumber() != 0.0;
            return std::nullopt;
        }

        bool IsZeroNumber(const RE::GFxValue& value)
        {
            return value.IsNumber() && value.GetNumber() == 0.0;
        }

        std::optional<bool> ReadMiscObjectivesVisibleFromEntry(const RE::GFxValue& entry)
        {
            if (!entry.IsObject())
                return std::nullopt;

            RE::GFxValue formID;
            if (!entry.GetMember("formID", &formID) || !IsZeroNumber(formID))
                return std::nullopt;

            RE::GFxValue active;
            if (!entry.GetMember("active", &active))
                return std::nullopt;
            return ReadBoolLike(active);
        }

        std::optional<ScaleformMiscVisibilityRead> ReadScaleformMiscObjectivesVisibleFromValue(
            const RE::GFxValue& value,
            const char*         source,
            std::uint32_t       depth = 0)
        {
            if (value.IsArray()) {
                const auto size = value.GetArraySize();
                for (std::uint32_t i = 0; i < size; ++i) {
                    RE::GFxValue entry;
                    if (!value.GetElement(i, &entry))
                        continue;
                    if (auto visible = ReadMiscObjectivesVisibleFromEntry(entry))
                        return ScaleformMiscVisibilityRead{ *visible, source };
                }
                return std::nullopt;
            }

            if (!value.IsObject())
                return std::nullopt;

            if (auto visible = ReadMiscObjectivesVisibleFromEntry(value))
                return ScaleformMiscVisibilityRead{ *visible, source };

            RE::GFxValue entries;
            if (value.GetMember("entryList", &entries)) {
                if (auto visible = ReadScaleformMiscObjectivesVisibleFromValue(entries, source, depth + 1))
                    return visible;
            }

            constexpr std::array kEntryMembers{
                "selectedEntry",
                "centeredEntry"
            };
            for (const char* member : kEntryMembers) {
                RE::GFxValue entry;
                if (!value.GetMember(member, &entry))
                    continue;
                if (auto visible = ReadMiscObjectivesVisibleFromEntry(entry))
                    return ScaleformMiscVisibilityRead{ *visible, source };
            }

            if (depth >= 4)
                return std::nullopt;

            constexpr std::array kChildMembers{
                "QuestJournalFader",
                "QuestsFader",
                "Page_mc",
                "TitleList",
                "TitleList_mc",
                "List_mc"
            };
            for (const char* member : kChildMembers) {
                RE::GFxValue child;
                if (!value.GetMember(member, &child))
                    continue;
                if (auto visible = ReadScaleformMiscObjectivesVisibleFromValue(child, source, depth + 1))
                    return visible;
            }

            return std::nullopt;
        }

        std::optional<ScaleformMiscVisibilityRead> ReadScaleformMiscObjectivesVisible(RE::JournalMenu* journal)
        {
            if (!journal)
                return std::nullopt;

            auto& questsTab = journal->GetRuntimeData().questsTab;
            if (auto visible = ReadScaleformMiscObjectivesVisibleFromValue(
                    questsTab.titleList,
                    "Journal_QuestsTab::titleList")) {
                return visible;
            }

            auto movie = questsTab.view;
            if (!movie) {
                auto* ui = RE::UI::GetSingleton();
                if (ui)
                    movie = ui->GetMovieView(RE::JournalMenu::MENU_NAME);
            }
            if (!movie)
                return std::nullopt;

            constexpr std::array kEntryListPaths{
                "QuestJournalFader.QuestsFader.Page_mc.TitleList.entryList",
                "QuestJournalFader.QuestsFader.Page_mc.TitleList_mc.List_mc.entryList",
                "QuestsFader.Page_mc.TitleList.entryList",
                "QuestsFader.Page_mc.TitleList_mc.List_mc.entryList",
                "_root.QuestJournalFader.QuestsFader.Page_mc.TitleList.entryList",
                "_root.QuestJournalFader.QuestsFader.Page_mc.TitleList_mc.List_mc.entryList",
                "_root.QuestsFader.Page_mc.TitleList.entryList",
                "_root.QuestsFader.Page_mc.TitleList_mc.List_mc.entryList"
            };

            for (const char* path : kEntryListPaths) {
                RE::GFxValue entries;
                if (!movie->GetVariable(&entries, path) || !entries.IsArray())
                    continue;
                if (auto visible = ReadScaleformMiscObjectivesVisibleFromValue(entries, path))
                    return visible;
            }

            constexpr std::array kSelectedEntryPaths{
                "QuestJournalFader.QuestsFader.Page_mc.TitleList.selectedEntry",
                "QuestJournalFader.QuestsFader.Page_mc.TitleList.centeredEntry",
                "QuestJournalFader.QuestsFader.Page_mc.TitleList_mc.List_mc.selectedEntry",
                "QuestJournalFader.QuestsFader.Page_mc.TitleList_mc.List_mc.centeredEntry",
                "QuestsFader.Page_mc.TitleList.selectedEntry",
                "QuestsFader.Page_mc.TitleList.centeredEntry",
                "QuestsFader.Page_mc.TitleList_mc.List_mc.selectedEntry",
                "QuestsFader.Page_mc.TitleList_mc.List_mc.centeredEntry",
                "_root.QuestJournalFader.QuestsFader.Page_mc.TitleList.selectedEntry",
                "_root.QuestJournalFader.QuestsFader.Page_mc.TitleList.centeredEntry",
                "_root.QuestJournalFader.QuestsFader.Page_mc.TitleList_mc.List_mc.selectedEntry",
                "_root.QuestJournalFader.QuestsFader.Page_mc.TitleList_mc.List_mc.centeredEntry",
                "_root.QuestsFader.Page_mc.TitleList.selectedEntry",
                "_root.QuestsFader.Page_mc.TitleList.centeredEntry",
                "_root.QuestsFader.Page_mc.TitleList_mc.List_mc.selectedEntry",
                "_root.QuestsFader.Page_mc.TitleList_mc.List_mc.centeredEntry"
            };

            for (const char* path : kSelectedEntryPaths) {
                RE::GFxValue entry;
                if (!movie->GetVariable(&entry, path))
                    continue;
                if (auto visible = ReadScaleformMiscObjectivesVisibleFromValue(entry, path))
                    return visible;
            }

            return std::nullopt;
        }

        std::optional<bool> ReadNativeJournalMiscObjectivesVisible(RE::JournalMenu* journal)
        {
            if (!journal)
                return std::nullopt;

            return journal->GetRuntimeData().questsTab.unk30;
        }

        std::optional<MiscObjectivesVisibilityRead> ReadJournalMiscObjectivesVisible()
        {
            auto* ui = RE::UI::GetSingleton();
            if (!ui)
                return std::nullopt;

            auto journal = ui->GetMenu<RE::JournalMenu>();
            if (!journal)
                return std::nullopt;

            MiscObjectivesVisibilityRead result;
            if (auto native = ReadNativeJournalMiscObjectivesVisible(journal.get())) {
                result.nativeKnown   = true;
                result.nativeVisible = *native;
            }

            if (auto scaleform = ReadScaleformMiscObjectivesVisible(journal.get())) {
                result.visible          = scaleform->visible;
                result.source           = scaleform->source;
                result.scaleformKnown   = true;
                result.scaleformVisible = scaleform->visible;
                result.scaleformSource  = scaleform->source;
                return result;
            }

            if (result.nativeKnown) {
                result.visible = result.nativeVisible;
                result.source  = "Journal_QuestsTab::unk30";
                return result;
            }

            return std::nullopt;
        }

        void StoreMiscObjectivesVisible(bool visible,
                                        const char* source = "cached misc objectives visibility")
        {
            const bool        prevKnown   = g_miscObjectivesVisibilityKnown;
            const bool        prevVisible = g_miscObjectivesVisible;
            const char* const prevSource  = g_miscObjectivesVisibilitySource;
            g_miscObjectivesVisibilityKnown = true;
            g_miscObjectivesVisible         = visible;
            g_miscObjectivesVisibilitySource = source;

            logger::trace("[MiscObjectivesVisibility] update known {}->{} visible {}->{} source '{}'->'{}'",
                          prevKnown, g_miscObjectivesVisibilityKnown,
                          prevVisible, g_miscObjectivesVisible,
                          prevSource ? prevSource : "",
                          g_miscObjectivesVisibilitySource ? g_miscObjectivesVisibilitySource : "");
        }

        MiscObjectivesVisibility GetMiscObjectivesVisibility()
        {
            MiscObjectivesVisibility state;
            state.cachedKnown   = g_miscObjectivesVisibilityKnown;
            state.cachedVisible = g_miscObjectivesVisible;

            if (g_miscObjectivesVisibilityKnown &&
                std::string_view(g_miscObjectivesVisibilitySource) == "command") {
                state.visible = g_miscObjectivesVisible;
                state.known   = true;
                state.source  = g_miscObjectivesVisibilitySource;
                return state;
            }

            if (auto live = ReadJournalMiscObjectivesVisible()) {
                StoreMiscObjectivesVisible(live->visible, live->source);
                state.visible          = live->visible;
                state.known            = true;
                state.source           = live->source;
                state.cachedKnown      = true;
                state.cachedVisible    = live->visible;
                state.journalOpen      = true;
                state.scaleformKnown   = live->scaleformKnown;
                state.scaleformVisible = live->scaleformVisible;
                state.scaleformSource  = live->scaleformSource;
                state.nativeKnown      = live->nativeKnown;
                state.nativeVisible    = live->nativeVisible;
                return state;
            }

            if (g_miscObjectivesVisibilityKnown) {
                state.visible = g_miscObjectivesVisible;
                state.known   = true;
                state.source  = g_miscObjectivesVisibilitySource;
                return state;
            }

            return state;
        }

        nlohmann::json MiscObjectivesVisibilityJson(const MiscObjectivesVisibility& state)
        {
            return {
                { "visible", state.visible },
                { "known", state.known },
                { "source", state.source },
                { "journalOpen", state.journalOpen },
                { "cachedKnown", state.cachedKnown },
                { "cachedVisible", state.cachedVisible },
                { "scaleformKnown", state.scaleformKnown },
                { "scaleformVisible", state.scaleformVisible },
                { "scaleformSource", state.scaleformSource },
                { "nativeKnown", state.nativeKnown },
                { "nativeVisible", state.nativeVisible }
            };
        }

        RE::BGSRefAlias* FindRefAlias(RE::TESQuest* quest, std::uint32_t aliasID)
        {
            if (!quest)
                return nullptr;
            for (auto* alias : quest->aliases) {
                if (!alias)
                    continue;
                if (alias->aliasID != aliasID)
                    continue;
                if (alias->GetVMTypeID() == RE::BGSRefAlias::VMTYPEID)
                    return static_cast<RE::BGSRefAlias*>(alias);
                return nullptr;
            }
            return nullptr;
        }

        bool IsAliasTokenHead(std::string_view head)
        {
            head = Common::TrimAscii(head);
            return head.size() >= 5
                && Common::EqualAsciiIgnoreCase(head.substr(0, 5), "Alias")
                && (head.size() == 5 || head[5] == '.');
        }

        RE::BGSBaseAlias* FindAliasByName(RE::TESQuest* quest, std::string_view name)
        {
            name = Common::TrimAscii(name);
            if (!quest || name.empty())
                return nullptr;
            for (auto* alias : quest->aliases) {
                if (!alias)
                    continue;
                const char* aname = alias->aliasName.c_str();
                if (!aname)
                    continue;
                if (Common::EqualAsciiIgnoreCase(aname, name))
                    return alias;
            }
            return nullptr;
        }

        std::string RefDisplayName(RE::TESObjectREFR* ref)
        {
            if (!ref)
                return {};
            const char* full = ref->GetDisplayFullName();
            if (full && *full)
                return full;
            const char* name = ref->GetName();
            if (name && *name)
                return name;
            if (auto* base = ref->GetBaseObject()) {
                const char* baseName = base->GetName();
                if (baseName && *baseName)
                    return baseName;
            }
            return {};
        }

        std::string FormDisplayName(RE::TESForm* form)
        {
            if (!form)
                return {};
            if (auto* ref = form->AsReference()) {
                if (auto name = RefDisplayName(ref); !name.empty())
                    return name;
            }
            const char* name = form->GetName();
            if (name && *name)
                return name;
            const char* editorId = form->GetFormEditorID();
            if (editorId && *editorId)
                return editorId;
            return {};
        }

        std::string PtrString(const void* ptr)
        {
            return std::format("0x{:016X}", reinterpret_cast<std::uintptr_t>(ptr));
        }

        const char* ObjectiveStateName(RE::QUEST_OBJECTIVE_STATE state)
        {
            switch (state) {
            case RE::QUEST_OBJECTIVE_STATE::kDormant:            return "Dormant";
            case RE::QUEST_OBJECTIVE_STATE::kDisplayed:          return "Displayed";
            case RE::QUEST_OBJECTIVE_STATE::kCompleted:          return "Completed";
            case RE::QUEST_OBJECTIVE_STATE::kCompletedDisplayed: return "CompletedDisplayed";
            case RE::QUEST_OBJECTIVE_STATE::kFailed:             return "Failed";
            case RE::QUEST_OBJECTIVE_STATE::kFailedDisplayed:    return "FailedDisplayed";
            default:                                             return "Unknown";
            }
        }

        bool QuestFlagSet(RE::TESQuest* quest, RE::QuestFlag flag)
        {
            return quest && quest->data.flags.all(flag);
        }

        nlohmann::json QuestDebugJson(RE::TESQuest* quest)
        {
            nlohmann::json out = nlohmann::json::object();
            if (!quest)
                return out;

            const char* editorID = quest->GetFormEditorID();
            const char* name     = quest->GetFullName();
            out["ptr"]            = PtrString(quest);
            out["formId"]         = Common::FormIdToString(quest->GetFormID());
            out["editorId"]       = editorID ? std::string(editorID) : std::string();
            out["name"]           = name ? std::string(name) : std::string();
            out["type"]           = std::string(QuestTypeName(quest->GetType()));
            out["flagsRaw"]       = static_cast<std::uint16_t>(quest->data.flags.underlying());
            out["isActiveFlag"]   = quest->IsActive();
            out["isDisplayedHUD"] = QuestFlagSet(quest, RE::QuestFlag::kDisplayedInHUD);
            out["isEnabled"]      = quest->IsEnabled();
            out["isRunning"]      = quest->IsRunning();
            out["isCompleted"]    = quest->IsCompleted();
            out["currentStage"]   = quest->GetCurrentStageID();
            out["currentInstanceID"] = quest->currentInstanceID;
            return out;
        }

        nlohmann::json ObjectiveDebugJson(RE::BGSQuestObjective* objective,
                                          std::uint32_t instanceID = 0)
        {
            nlohmann::json out = nlohmann::json::object();
            if (!objective)
                return out;

            const std::string text = objective->displayText.c_str()
                                         ? objective->displayText.c_str()
                                         : "";
            out["ptr"]          = PtrString(objective);
            out["index"]        = objective->index;
            out["state"]        = ObjectiveStateName(static_cast<RE::QUEST_OBJECTIVE_STATE>(objective->state.underlying()));
            out["stateRaw"]     = static_cast<std::uint8_t>(objective->state.underlying());
            out["flagsRaw"]     = objective->flags.underlying();
            out["numTargets"]   = objective->numTargets;
            out["text"]         = text;
            out["instanceID"]   = instanceID;
            if (auto* q = objective->ownerQuest) {
                out["questFormId"]  = Common::FormIdToString(q->GetFormID());
                out["questEditorId"] = q->GetFormEditorID() ? q->GetFormEditorID() : "";
                out["questType"]     = std::string(QuestTypeName(q->GetType()));
            }
            return out;
        }

        struct QuestMarkerCoordinates
        {
            RE::TESObjectREFR*              ref = nullptr;
            RE::BGSLocation*                location = nullptr;
            RE::NiPointer<RE::TESObjectREFR> locationMarkerRef;
            const char*                     source = "unresolved:noGlobalCoordinates";
        };

        bool IsMapMarkerRef(RE::TESObjectREFR* ref)
        {
            auto* extra = ref ? ref->extraList.GetByType<RE::ExtraMapMarker>() : nullptr;
            return extra && extra->mapData;
        }

        bool HasGlobalMapCoordinates(RE::TESObjectREFR* ref)
        {
            if (!ref)
                return false;

            if (auto* cell = ref->GetParentCell(); cell && cell->IsInteriorCell())
                return false;

            auto* world = ref->GetWorldspace();
            return world && !world->parentWorld;
        }

        bool IsMapFacingCoordinateRef(RE::TESObjectREFR* ref)
        {
            return HasGlobalMapCoordinates(ref);
        }

        // Finds a map-facing reference for an interior target by following
        // the cell's exit doors (ExtraTeleport -> linkedDoor). This projects
        // interior quest targets onto the world map even when the location
        // hierarchy has no usable map marker.
        RE::TESObjectREFR* FindMapFacingExitDoorRef(RE::TESObjectREFR* targetRef)
        {
            auto* cell = targetRef ? targetRef->GetParentCell() : nullptr;
            if (!cell || !cell->IsInteriorCell())
                return nullptr;

            RE::TESObjectREFR* found = nullptr;
            cell->ForEachReference([&](RE::TESObjectREFR* candidate) {
                if (!candidate)
                    return RE::BSContainer::ForEachResult::kContinue;

                auto* teleport = candidate->extraList.GetByType<RE::ExtraTeleport>();
                const auto* tpData = teleport ? teleport->teleportData : nullptr;
                if (!tpData)
                    return RE::BSContainer::ForEachResult::kContinue;

                auto linked = tpData->linkedDoor.get();
                auto* linkedRef = linked.get();
                if (linkedRef && HasGlobalMapCoordinates(linkedRef)) {
                    found = linkedRef;
                    return RE::BSContainer::ForEachResult::kStop;
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            return found;
        }

        static void BuildPersistentRefCache()
        {
            s_persistentCache.byFormId.clear();
            s_persistentCache.markerByLocationId.clear();
            s_persistentCache.markerByName.clear();

            auto* handler = RE::TESDataHandler::GetSingleton();
            if (!handler) {
                s_persistentCache.built = true;
                return;
            }

            for (auto* world : handler->GetFormArray<RE::TESWorldSpace>()) {
                if (!world || !world->persistentCell)
                    continue;

                world->persistentCell->ForEachReference([&](RE::TESObjectREFR* ref) {
                    const RE::FormID fid = ref->GetFormID();
                    if (fid)
                        s_persistentCache.byFormId.emplace(fid, ref);

                    if (IsMapMarkerRef(ref) && HasGlobalMapCoordinates(ref)) {
                        if (auto* xl = ref->extraList.GetByType<RE::ExtraLocation>()) {
                            if (xl->location) {
                                s_persistentCache.markerByLocationId.emplace(
                                    xl->location->GetFormID(), ref);
                            }
                        }
                        auto* extra = ref->extraList.GetByType<RE::ExtraMapMarker>();
                        const char* raw = extra && extra->mapData
                                              ? extra->mapData->locationName.GetFullName()
                                              : nullptr;
                        if (raw && *raw) {
                            std::string key(raw);
                            for (auto& c : key)
                                c = static_cast<char>(
                                    std::tolower(static_cast<unsigned char>(c)));
                            s_persistentCache.markerByName.emplace(std::move(key), ref);
                        }
                    }

                    return RE::BSContainer::ForEachResult::kContinue;
                });
            }

            s_persistentCache.built = true;
        }

        static inline void EnsurePersistentRefCache()
        {
            if (!s_persistentCache.built)
                BuildPersistentRefCache();
        }

        template <class Callback>
        void ForEachPersistentWorldspaceRef(Callback&& callback)
        {
            auto* handler = RE::TESDataHandler::GetSingleton();
            if (!handler)
                return;

            const auto& worlds = handler->GetFormArray<RE::TESWorldSpace>();
            bool stop = false;
            for (auto* world : worlds) {
                if (!world || !world->persistentCell)
                    continue;

                world->persistentCell->ForEachReference([&](RE::TESObjectREFR& ref) {
                    if (callback(ref)) {
                        stop = true;
                        return RE::BSContainer::ForEachResult::kStop;
                    }
                    return RE::BSContainer::ForEachResult::kContinue;
                });

                if (stop)
                    break;
            }
        }

        template <class Callback>
        void ForEachPersistentMapMarkerRef(Callback&& callback)
        {
            ForEachPersistentWorldspaceRef([&](RE::TESObjectREFR& ref) {
                return IsMapMarkerRef(&ref) && HasGlobalMapCoordinates(&ref) && callback(ref);
            });
        }

        RE::TESObjectREFR* FindPersistentReferenceByFormID(RE::FormID formId)
        {
            if (!formId)
                return nullptr;

            if (auto* form = RE::TESForm::LookupByID<RE::TESObjectREFR>(formId))
                return form;

            EnsurePersistentRefCache();
            const auto it = s_persistentCache.byFormId.find(formId);
            return it != s_persistentCache.byFormId.end() ? it->second : nullptr;
        }

        bool SameTextFolded(const char* lhs, const char* rhs)
        {
            if (!lhs || !rhs || !*lhs || !*rhs)
                return false;

            while (*lhs && *rhs) {
                const auto left = static_cast<unsigned char>(*lhs++);
                const auto right = static_cast<unsigned char>(*rhs++);
                if (std::tolower(left) != std::tolower(right))
                    return false;
            }

            return *lhs == '\0' && *rhs == '\0';
        }

        RE::TESObjectREFR* FindPersistentMapMarkerForLocation(RE::BGSLocation* location)
        {
            if (!location)
                return nullptr;

            EnsurePersistentRefCache();

            {
                const auto it = s_persistentCache.markerByLocationId.find(
                    location->GetFormID());
                if (it != s_persistentCache.markerByLocationId.end())
                    return it->second;
            }

            const char* locationName = location->GetFullName();
            if (locationName && *locationName) {
                std::string key(locationName);
                for (auto& c : key)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                const auto it = s_persistentCache.markerByName.find(key);
                if (it != s_persistentCache.markerByName.end())
                    return it->second;
            }

            return nullptr;
        }

        RE::TESObjectREFR* FindLocationSpecialRefMarker(RE::BGSLocation* location,
                                                        bool requireMapMarker)
        {
            if (!location)
                return nullptr;

            std::unordered_set<RE::FormID> seen;
            for (const auto& specialRef : location->specialRefs) {
                const auto refId = specialRef.refData.refID;
                if (!refId || !seen.insert(refId).second)
                    continue;

                auto* ref = FindPersistentReferenceByFormID(refId);
                if (!ref)
                    continue;

                if (requireMapMarker) {
                    if (!IsMapMarkerRef(ref) || !HasGlobalMapCoordinates(ref))
                        continue;
                } else if (!IsMapFacingCoordinateRef(ref)) {
                    continue;
                }

                return ref;
            }

            return nullptr;
        }

        QuestMarkerCoordinates CoordinatesForLocationMarker(RE::BGSLocation* location,
                                                            RE::NiPointer<RE::TESObjectREFR> markerRef,
                                                            const char* source)
        {
            QuestMarkerCoordinates out;
            out.ref               = markerRef.get();
            out.location          = location;
            out.locationMarkerRef = markerRef;
            out.source            = source;
            return out;
        }

        QuestMarkerCoordinates CoordinatesForLocationMarkerRef(RE::BGSLocation* location,
                                                               RE::TESObjectREFR* markerRef,
                                                               const char* source)
        {
            QuestMarkerCoordinates out;
            out.ref      = markerRef;
            out.location = location;
            out.source   = source;
            return out;
        }

        QuestMarkerCoordinates ResolveQuestMarkerCoordinates(RE::TESObjectREFR* targetRef)
        {
            QuestMarkerCoordinates out;

            if (!targetRef)
                return out;

            if (HasGlobalMapCoordinates(targetRef)) {
                out.ref = targetRef;
                out.source = "targetRef:global";
            }

            auto* cell = targetRef->GetParentCell();
            std::array<RE::BGSLocation*, 3> locationCandidates{
                targetRef->GetCurrentLocation(),
                targetRef->GetEditorLocation(),
                cell ? cell->GetLocation() : nullptr
            };

            std::unordered_set<RE::FormID> seenLocations;

            for (auto* location : locationCandidates) {
                if (!location)
                    continue;

                for (auto* candidate = location; candidate; candidate = candidate->parentLoc) {
                    if (!candidate || !seenLocations.insert(candidate->GetFormID()).second)
                        continue;

                    if (!out.location)
                        out.location = candidate;

                    auto handleMarker = candidate->worldLocMarker.get();
                    if (IsMapFacingCoordinateRef(handleMarker.get()))
                        return CoordinatesForLocationMarker(
                            candidate,
                            handleMarker,
                            candidate == location
                                ? "BGSLocation::worldLocMarker"
                                : "BGSLocation::parentLoc.worldLocMarker");

                    if (auto* marker = FindLocationSpecialRefMarker(candidate, /*requireMapMarker=*/true))
                        return CoordinatesForLocationMarkerRef(
                            candidate,
                            marker,
                            candidate == location
                                ? "BGSLocation::specialRefs.mapMarker"
                                : "BGSLocation::parentLoc.specialRefs.mapMarker");

                    if (auto* marker = FindPersistentMapMarkerForLocation(candidate))
                        return CoordinatesForLocationMarkerRef(
                            candidate,
                            marker,
                            candidate == location
                                ? "persistentCell.ExtraMapMarker.location"
                                : "persistentCell.parentLoc.ExtraMapMarker.location");

                    if (auto* marker = FindLocationSpecialRefMarker(candidate, /*requireMapMarker=*/false))
                        return CoordinatesForLocationMarkerRef(
                            candidate,
                            marker,
                            candidate == location
                                ? "BGSLocation::specialRefs.globalRef"
                                : "BGSLocation::parentLoc.specialRefs.globalRef");
                }
            }

            // Interior target whose location hierarchy has no usable map
            // marker — project onto the world map through the cell's exit
            // door (linked teleport destination).
            if (auto* exitRef = FindMapFacingExitDoorRef(targetRef)) {
                out.ref    = exitRef;
                out.source = "linkedTeleportDoor.exit";
                return out;
            }

            if (out.location && out.ref)
                out.source = "targetRef:global:noLocationMarker";
            else if (out.location)
                out.source = "unresolved:noGlobalLocationMarker";
            return out;
        }

        struct ReferenceSpatialJsonKeys
        {
            const char* x;
            const char* y;
            const char* z;
            const char* worldspace;
            const char* worldspaceFormId;
            const char* parentWorldspace;
            const char* parentWorldspaceFormId;
            const char* cell;
            const char* cellFormId;
            const char* isInterior;
        };

        void WriteReferenceSpatialJson(nlohmann::json& out,
                                       RE::TESObjectREFR* ref,
                                       const ReferenceSpatialJsonKeys& keys)
        {
            if (!ref) {
                out[keys.x]                      = nullptr;
                out[keys.y]                      = nullptr;
                out[keys.z]                      = nullptr;
                out[keys.worldspace]             = nullptr;
                out[keys.worldspaceFormId]       = nullptr;
                out[keys.parentWorldspace]       = nullptr;
                out[keys.parentWorldspaceFormId] = nullptr;
                out[keys.cell]                   = nullptr;
                out[keys.cellFormId]             = nullptr;
                out[keys.isInterior]             = false;
                return;
            }

            out[keys.x] = ref->GetPositionX();
            out[keys.y] = ref->GetPositionY();
            out[keys.z] = ref->GetPositionZ();

            if (auto* world = ref->GetWorldspace()) {
                Common::BuildWorldspaceFields(out, world);
            } else {
                out[keys.worldspace]             = nullptr;
                out[keys.worldspaceFormId]       = nullptr;
                out[keys.parentWorldspace]       = nullptr;
                out[keys.parentWorldspaceFormId] = nullptr;
            }

            if (auto* cell = ref->GetParentCell()) {
                const char* cedid = cell->GetFormEditorID();
                out[keys.cell]       = cedid ? std::string(cedid) : std::string();
                out[keys.cellFormId] = Common::FormIdToString(cell->GetFormID());
                out[keys.isInterior] = cell->IsInteriorCell();
            } else {
                out[keys.cell]       = nullptr;
                out[keys.cellFormId] = nullptr;
                out[keys.isInterior] = false;
            }
        }

        void WriteReferenceSpatialJson(nlohmann::json& out, RE::TESObjectREFR* ref)
        {
            static constexpr ReferenceSpatialJsonKeys keys{
                "x",
                "y",
                "z",
                "worldspace",
                "worldspaceFormId",
                "parentWorldspace",
                "parentWorldspaceFormId",
                "cell",
                "cellFormId",
                "isInterior"
            };
            WriteReferenceSpatialJson(out, ref, keys);
        }

        void WriteReferenceLocalSpatialJson(nlohmann::json& out, RE::TESObjectREFR* ref)
        {
            static constexpr ReferenceSpatialJsonKeys keys{
                "localX",
                "localY",
                "localZ",
                "localWorldspace",
                "localWorldspaceFormId",
                "localParentWorldspace",
                "localParentWorldspaceFormId",
                "localCell",
                "localCellFormId",
                "localIsInterior"
            };
            WriteReferenceSpatialJson(out, ref, keys);
        }

        nlohmann::json QuestMarkerCoordinatesJson(const QuestMarkerCoordinates& coordinates)
        {
            nlohmann::json out = nlohmann::json::object();
            out["source"] = coordinates.source;
            if (coordinates.ref) {
                out["refId"] = Common::FormIdToString(coordinates.ref->GetFormID());
                out["name"]  = RefDisplayName(coordinates.ref);
            } else {
                out["refId"] = nullptr;
                out["name"]  = std::string();
            }

            if (coordinates.location) {
                const char* locationEditorId = coordinates.location->GetFormEditorID();
                const char* locationName = coordinates.location->GetFullName();
                out["locationFormId"]  = Common::FormIdToString(coordinates.location->GetFormID());
                out["locationEditorId"] = locationEditorId ? std::string(locationEditorId) : std::string();
                out["locationName"]     = locationName ? std::string(locationName) : std::string();
            } else {
                out["locationFormId"]  = nullptr;
                out["locationEditorId"] = nullptr;
                out["locationName"]     = nullptr;
            }

            WriteReferenceSpatialJson(out, coordinates.ref);
            return out;
        }

        nlohmann::json LocationDebugJson(RE::BGSLocation* location)
        {
            if (!location)
                return nullptr;

            const char* editorId = location->GetFormEditorID();
            const char* name     = location->GetFullName();

            nlohmann::json out;
            out["formId"]            = Common::FormIdToString(location->GetFormID());
            out["editorId"]          = editorId ? std::string(editorId) : std::string();
            out["name"]              = name ? std::string(name) : std::string();
            out["worldLocRadius"]    = location->worldLocRadius;
            out["worldLocMarker"]    = QuestMarkerCoordinatesJson(
                CoordinatesForLocationMarker(location, location->worldLocMarker.get(), "BGSLocation::worldLocMarker"));
            out["horseLocMarker"]    = QuestMarkerCoordinatesJson(
                CoordinatesForLocationMarker(location, location->horseLocMarker.get(), "BGSLocation::horseLocMarker"));
            out["specialRefsMapMarker"] = QuestMarkerCoordinatesJson(
                CoordinatesForLocationMarkerRef(location,
                                                FindLocationSpecialRefMarker(location, /*requireMapMarker=*/true),
                                                "BGSLocation::specialRefs.mapMarker"));
            out["persistentMapMarker"] = QuestMarkerCoordinatesJson(
                CoordinatesForLocationMarkerRef(location,
                                                FindPersistentMapMarkerForLocation(location),
                                                "persistentCell.ExtraMapMarker.location"));
            out["specialRefsGlobalRef"] = QuestMarkerCoordinatesJson(
                CoordinatesForLocationMarkerRef(location,
                                                FindLocationSpecialRefMarker(location, /*requireMapMarker=*/false),
                                                "BGSLocation::specialRefs.globalRef"));
            out["specialRefsCount"]  = location->specialRefs.size();
            out["specialRefsSample"] = nlohmann::json::array();

            std::size_t emitted = 0;
            for (const auto& specialRef : location->specialRefs) {
                if (emitted >= 12)
                    break;

                nlohmann::json item;
                if (specialRef.type) {
                    const char* typeEditorId = specialRef.type->GetFormEditorID();
                    item["typeFormId"]  = Common::FormIdToString(specialRef.type->GetFormID());
                    item["typeEditorId"] = typeEditorId ? std::string(typeEditorId) : std::string();
                } else {
                    item["typeFormId"]  = nullptr;
                    item["typeEditorId"] = nullptr;
                }
                item["refId"]         = Common::FormIdToString(specialRef.refData.refID);
                item["parentSpaceId"] = Common::FormIdToString(specialRef.refData.parentSpaceID);
                item["cellKeyRaw"]    = specialRef.refData.cellKey.raw;
                if (auto* ref = FindPersistentReferenceByFormID(specialRef.refData.refID)) {
                    nlohmann::json resolved;
                    resolved["ptr"] = PtrString(ref);
                    resolved["refId"] = Common::FormIdToString(ref->GetFormID());
                    resolved["name"] = RefDisplayName(ref);
                    resolved["hasMapMarker"] = IsMapMarkerRef(ref);
                    resolved["hasGlobalMapCoordinates"] = HasGlobalMapCoordinates(ref);
                    WriteReferenceSpatialJson(resolved, ref);
                    item["resolvedRef"] = std::move(resolved);
                } else {
                    item["resolvedRef"] = nullptr;
                }
                out["specialRefsSample"].push_back(std::move(item));
                ++emitted;
            }

            return out;
        }

        nlohmann::json ReferenceCoordinateCandidateJson(const char* source, RE::TESObjectREFR* ref)
        {
            nlohmann::json out;
            out["source"]    = source;
            out["available"] = ref != nullptr;
            if (!ref)
                return out;

            out["ptr"]       = PtrString(ref);
            out["refId"]     = Common::FormIdToString(ref->GetFormID());
            out["name"]      = RefDisplayName(ref);
            out["isDeleted"] = ref->IsDeleted();
            out["isDisabled"] = ref->IsDisabled();

            if (auto* base = ref->GetBaseObject()) {
                const char* baseName = base->GetName();
                out["baseFormId"] = Common::FormIdToString(base->GetFormID());
                out["baseName"]   = baseName ? std::string(baseName) : std::string();
            } else {
                out["baseFormId"] = nullptr;
                out["baseName"]   = nullptr;
            }

            WriteReferenceSpatialJson(out, ref);

            if (auto* extra = ref->extraList.GetByType<RE::ExtraMapMarker>(); extra && extra->mapData) {
                using Flag = RE::MapMarkerData::Flag;
                auto* data = extra->mapData;
                const char* markerName = data->locationName.GetFullName();
                out["mapMarker"] = {
                    { "name", markerName ? std::string(markerName) : std::string() },
                    { "typeId", static_cast<std::uint32_t>(data->type.underlying()) },
                    { "flagsRaw", static_cast<std::uint8_t>(data->flags.underlying()) },
                    { "isVisible", data->flags.any(Flag::kVisible) },
                    { "canFastTravel", data->flags.any(Flag::kCanTravelTo) }
                };
            } else {
                out["mapMarker"] = nullptr;
            }

            auto* currentLocation = ref->GetCurrentLocation();
            auto* editorLocation  = ref->GetEditorLocation();
            auto* cellLocation    = ref->GetParentCell() ? ref->GetParentCell()->GetLocation() : nullptr;
            out["currentLocation"] = LocationDebugJson(currentLocation);
            out["editorLocation"]  = LocationDebugJson(editorLocation);
            out["cellLocation"]    = LocationDebugJson(cellLocation);

            auto linkedDoor = ref->extraList.GetTeleportLinkedDoor().get();
            out["teleportLinkedDoorRefId"] = linkedDoor ? nlohmann::json(Common::FormIdToString(linkedDoor->GetFormID())) : nullptr;

            if (auto* randomMarker = ref->extraList.GetByType<RE::ExtraRandomTeleportMarker>(); randomMarker && randomMarker->marker)
                out["randomTeleportMarkerRefId"] = Common::FormIdToString(randomMarker->marker->GetFormID());
            else
                out["randomTeleportMarkerRefId"] = nullptr;

            return out;
        }

        nlohmann::json EditorLocationCandidateJson(RE::TESObjectREFR* ref)
        {
            nlohmann::json out;
            out["source"] = "TESObjectREFR::GetEditorLocation(out)";
            if (!ref) {
                out["available"] = false;
                return out;
            }

            RE::NiPoint3 pos;
            RE::NiPoint3 rot;
            RE::TESForm* worldOrCell = nullptr;
            auto* fallbackCell = ref->GetParentCell();
            const bool available = ref->GetEditorLocation(pos, rot, worldOrCell, fallbackCell);
            out["available"] = available;
            if (!available)
                return out;

            out["x"] = pos.x;
            out["y"] = pos.y;
            out["z"] = pos.z;
            out["rotationX"] = rot.x;
            out["rotationY"] = rot.y;
            out["rotationZ"] = rot.z;

            if (worldOrCell) {
                out["worldOrCellFormId"] = Common::FormIdToString(worldOrCell->GetFormID());
                if (auto* world = worldOrCell->As<RE::TESWorldSpace>()) {
                    const char* edid = world->GetFormEditorID();
                    out["worldOrCellType"] = "TESWorldSpace";
                    out["worldOrCellEditorId"] = edid ? std::string(edid) : std::string();
                } else if (auto* cell = worldOrCell->As<RE::TESObjectCELL>()) {
                    const char* edid = cell->GetFormEditorID();
                    out["worldOrCellType"] = cell->IsInteriorCell() ? "TESObjectCELL:Interior" : "TESObjectCELL:Exterior";
                    out["worldOrCellEditorId"] = edid ? std::string(edid) : std::string();
                    out["cellLocation"] = LocationDebugJson(cell->GetLocation());
                } else {
                    out["worldOrCellType"] = "TESForm";
                    out["worldOrCellEditorId"] = worldOrCell->GetFormEditorID() ? worldOrCell->GetFormEditorID() : "";
                }
            } else {
                out["worldOrCellFormId"] = nullptr;
                out["worldOrCellType"] = nullptr;
                out["worldOrCellEditorId"] = nullptr;
            }

            return out;
        }

        nlohmann::json QuestCoordinateDiagnosticsJson(RE::TESObjectREFR* targetRef,
                                                      const QuestMarkerCoordinates& selected)
        {
            nlohmann::json out;
            out["selected"] = QuestMarkerCoordinatesJson(selected);
            out["resolvedLocation"] = LocationDebugJson(selected.location);
            out["candidates"] = nlohmann::json::array();
            out["candidates"].push_back(ReferenceCoordinateCandidateJson("targetRef", targetRef));
            out["candidates"].push_back(EditorLocationCandidateJson(targetRef));

            if (selected.location) {
                out["candidates"].push_back(ReferenceCoordinateCandidateJson("BGSLocation::worldLocMarker", selected.location->worldLocMarker.get().get()));
                out["candidates"].push_back(ReferenceCoordinateCandidateJson("BGSLocation::horseLocMarker", selected.location->horseLocMarker.get().get()));
            }

            if (targetRef) {
                out["candidates"].push_back(ReferenceCoordinateCandidateJson("targetRef.teleportLinkedDoor", targetRef->extraList.GetTeleportLinkedDoor().get().get()));
                if (auto* randomMarker = targetRef->extraList.GetByType<RE::ExtraRandomTeleportMarker>())
                    out["candidates"].push_back(ReferenceCoordinateCandidateJson("targetRef.randomTeleportMarker", randomMarker->marker));
            }

            return out;
        }

        nlohmann::json QuestTargetDebugJson(RE::TESQuest* quest,
                                            RE::BGSQuestObjective* objective,
                                            RE::TESQuestTarget* target,
                                            std::uint32_t instanceID,
                                            bool includeCoordinateDiagnostics = false)
        {
            nlohmann::json out = nlohmann::json::object();
            if (!target)
                return out;

            out["ptr"]           = PtrString(target);
            out["aliasId"]       = target->alias;
            out["hasConditions"] = static_cast<bool>(target->conditions);
            out["flags"]         = static_cast<std::uint8_t>(target->flags.underlying());

            if (quest)
                out["quest"] = QuestDebugJson(quest);
            if (objective)
                out["objective"] = ObjectiveDebugJson(objective, instanceID);

            if (auto* refAlias = FindRefAlias(quest, target->alias)) {
                out["refAliasFound"] = true;
                auto* ref = refAlias->GetReference();
                if (ref) {
                    out["ref"] = {
                        { "ptr", PtrString(ref) },
                        { "formId", Common::FormIdToString(ref->GetFormID()) },
                        { "name", RefDisplayName(ref) },
                        { "isDeleted", ref->IsDeleted() },
                        { "x", ref->GetPositionX() },
                        { "y", ref->GetPositionY() },
                        { "z", ref->GetPositionZ() }
                    };
                    if (auto* cell = ref->GetParentCell()) {
                        out["ref"]["cellFormId"] = Common::FormIdToString(cell->GetFormID());
                        out["ref"]["cell"] = cell->GetFormEditorID() ? cell->GetFormEditorID() : "";
                    }
                    if (auto* world = ref->GetWorldspace()) {
                        out["ref"]["worldspaceFormId"] = Common::FormIdToString(world->GetFormID());
                        out["ref"]["worldspace"] = world->GetFormEditorID() ? world->GetFormEditorID() : "";
                    }
                    const auto coordinates = ResolveQuestMarkerCoordinates(ref);
                    out["mapCoordinates"] = QuestMarkerCoordinatesJson(coordinates);
                    if (includeCoordinateDiagnostics)
                        out["coordinateDiagnostics"] = QuestCoordinateDiagnosticsJson(ref, coordinates);
                    if (target->conditions) {
                        auto* condPlayer = RE::PlayerCharacter::GetSingleton();
                        if (condPlayer)
                            out["conditionsTruePlayerRef"] = target->conditions.IsTrue(condPlayer, ref);
                        else
                            out["conditionsTruePlayerRef"] = nullptr;
                    }
                } else {
                    out["ref"] = nullptr;
                }
            } else {
                out["refAliasFound"] = false;
                out["ref"] = nullptr;
            }

            return out;
        }

        RE::BGSQuestObjective* FindObjectiveForTarget(RE::TESQuest* quest, RE::TESQuestTarget* target)
        {
            if (!quest || !target)
                return nullptr;

            for (auto* objective : quest->objectives) {
                if (!objective || !objective->targets)
                    continue;
                for (std::uint32_t i = 0; i < objective->numTargets; ++i) {
                    if (objective->targets[i] == target)
                        return objective;
                }
            }
            return nullptr;
        }

        RE::BGSQuestInstanceText* FindQuestInstanceText(RE::TESQuest* quest, std::uint32_t instanceID)
        {
            if (!quest)
                return nullptr;

            const auto findByID = [&](std::uint32_t id) -> RE::BGSQuestInstanceText* {
                if (id == 0)
                    return nullptr;
                for (auto* data : quest->instanceData) {
                    if (data && data->id == id)
                        return data;
                }
                return nullptr;
            };

            if (auto* data = findByID(instanceID))
                return data;
            if (quest->currentInstanceID != instanceID)
                return findByID(quest->currentInstanceID);
            return nullptr;
        }

        std::string ResolveAliasFromInstanceText(RE::TESQuest* quest,
                                                 std::uint32_t aliasID,
                                                 std::uint32_t instanceID)
        {
            const auto resolveFrom = [&](RE::BGSQuestInstanceText* data) -> std::string {
                if (!data)
                    return {};
                for (const auto& str : data->stringData) {
                    if (str.aliasID != aliasID || str.fullNameFormID == 0)
                        continue;
                    auto* form = RE::TESForm::LookupByID(str.fullNameFormID);
                    if (!form) {
                        logger::debug("[Map::Markers::Quests] unresolved instance text formId=0x{:08X} alias={} quest={} instance={}",
                                      str.fullNameFormID, aliasID,
                                      quest && quest->GetFormEditorID() ? quest->GetFormEditorID() : "",
                                      data->id);
                        continue;
                    }
                    if (auto name = FormDisplayName(form); !name.empty())
                        return name;
                }
                return {};
            };

            if (auto name = resolveFrom(FindQuestInstanceText(quest, instanceID)); !name.empty())
                return name;

            std::string onlyName;
            std::size_t matches = 0;
            if (quest) {
                for (auto* data : quest->instanceData) {
                    if (auto name = resolveFrom(data); !name.empty()) {
                        onlyName = std::move(name);
                        ++matches;
                    }
                }
            }
            return matches == 1 ? onlyName : std::string();
        }

        std::string ResolveAliasDisplayName(RE::TESQuest* quest,
                                            RE::BGSBaseAlias* alias,
                                            std::uint32_t instanceID)
        {
            if (!alias)
                return {};

            if (auto name = ResolveAliasFromInstanceText(quest, alias->aliasID, instanceID); !name.empty())
                return name;

            if (alias->GetVMTypeID() == RE::BGSRefAlias::VMTYPEID) {
                auto* refAlias = static_cast<RE::BGSRefAlias*>(alias);
                if (auto name = RefDisplayName(refAlias->GetReference()); !name.empty())
                    return name;
            }

            return {};
        }

        std::string ResolveQuestObjectiveText(RE::TESQuest* quest,
                                              const std::string& raw,
                                              std::uint32_t instanceID)
        {
            if (raw.empty() || !quest)
                return raw;

            std::string out;
            out.reserve(raw.size());

            std::size_t i = 0;
            while (i < raw.size()) {
                if (raw[i] != '<') {
                    out.push_back(raw[i++]);
                    continue;
                }
                const auto end = raw.find('>', i + 1);
                if (end == std::string::npos) {
                    out.append(raw, i, std::string::npos);
                    break;
                }
                std::string_view token(&raw[i + 1], end - i - 1);

                bool         matched = false;
                const auto eq = token.find('=');
                if (eq != std::string_view::npos) {
                    const auto head = Common::TrimAscii(token.substr(0, eq));
                    const auto name = Common::TrimAscii(token.substr(eq + 1));

                    if (IsAliasTokenHead(head)) {
                        if (auto* a = FindAliasByName(quest, name)) {
                            std::string repl = ResolveAliasDisplayName(quest, a, instanceID);
                            if (!repl.empty()) {
                                out.append(repl);
                                matched = true;
                            } else {
                                logger::debug("[Map::Markers::Quests] unresolved alias token '<{}>' quest={} aliasName='{}' aliasID={} instance={} currentInstance={}",
                                              std::string(token),
                                              quest->GetFormEditorID() ? quest->GetFormEditorID() : "",
                                              std::string(name), a->aliasID, instanceID,
                                              quest->currentInstanceID);
                            }
                        } else {
                            logger::debug("[Map::Markers::Quests] alias token '<{}>' did not match any quest alias quest={}",
                                          std::string(token),
                                          quest->GetFormEditorID() ? quest->GetFormEditorID() : "");
                        }
                    }
                }

                if (!matched) {
                    out.append(raw, i, end - i + 1);
                }
                i = end + 1;
            }

            return out;
        }

        std::uint32_t FindDisplayedObjectiveInstanceID(RE::PlayerCharacter* player,
                                                       RE::BGSQuestObjective* objective)
        {
            if (!player || !objective || REL::Module::IsVR())
                return 0;

            const auto base = reinterpret_cast<std::uintptr_t>(player);
            const std::size_t off = REL::Module::IsAE() ? 0x588 : 0x580;
            const auto& instances =
                *reinterpret_cast<const RE::BSTArray<RE::BGSInstancedQuestObjective>*>(base + off);

            for (const auto& inst : instances) {
                if (inst.Objective == objective &&
                    inst.InstanceState == RE::QUEST_OBJECTIVE_STATE::kDisplayed) {
                    return inst.instanceID;
                }
            }
            for (const auto& inst : instances) {
                if (inst.Objective == objective)
                    return inst.instanceID;
            }
            return 0;
        }

        void HashCombine(std::size_t& seed, std::size_t value) noexcept
        {
            seed ^= value + 0x9E3779B97F4A7C15ull + (seed << 6) + (seed >> 2);
        }

        struct QuestMarkerDestinationKey
        {
            RE::FormID    questFormID{};
            std::uint16_t objectiveIndex{};
            RE::FormID    worldspaceFormID{};
            RE::FormID    cellFormID{};
            float         x{};
            float         y{};
            float         z{};

            bool operator==(const QuestMarkerDestinationKey&) const = default;
        };

        struct QuestMarkerDestinationKeyHash
        {
            std::size_t operator()(const QuestMarkerDestinationKey& key) const noexcept
            {
                std::size_t seed = 0;
                HashCombine(seed, std::hash<RE::FormID>{}(key.questFormID));
                HashCombine(seed, std::hash<std::uint16_t>{}(key.objectiveIndex));
                HashCombine(seed, std::hash<RE::FormID>{}(key.worldspaceFormID));
                HashCombine(seed, std::hash<RE::FormID>{}(key.cellFormID));
                HashCombine(seed, std::hash<float>{}(key.x));
                HashCombine(seed, std::hash<float>{}(key.y));
                HashCombine(seed, std::hash<float>{}(key.z));
                return seed;
            }
        };
    }  // namespace

    void CaptureQuestJournalState()
    {
        if (auto visible = ReadJournalMiscObjectivesVisible()) {
            if (g_miscObjectivesVisibilityKnown &&
                std::string_view(g_miscObjectivesVisibilitySource) == "command") {
                logger::trace("[Map::Markers::Quests] observed journal misc visibility={} source={} but keeping command override={}",
                              visible->visible, visible->source, g_miscObjectivesVisible);
                return;
            }

            StoreMiscObjectivesVisible(visible->visible, visible->source);
            logger::trace("[Map::Markers::Quests] captured misc objectives visibility={} source={}",
                          visible->visible, visible->source);
        }
    }

    void ResetQuestJournalState()
    {
        g_miscObjectivesVisibilityKnown = false;
        g_miscObjectivesVisible         = true;
        g_miscObjectivesVisibilitySource = "cached misc objectives visibility";
        s_persistentCache.built = false;
        s_persistentCache.byFormId.clear();
        s_persistentCache.markerByLocationId.clear();
        s_persistentCache.markerByName.clear();
        logger::trace("[Map::Markers::Quests] reset cached quest-journal state");
    }

    nlohmann::json ReadQuestMarkers()
    {
        nlohmann::json result = nlohmann::json::array();

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            logger::warn("[Map::Markers::Quests] no PlayerCharacter");
            return result;
        }

        std::unordered_set<QuestMarkerDestinationKey, QuestMarkerDestinationKeyHash> seenDestinations;
        const auto miscObjectivesVisibility = GetMiscObjectivesVisibility();

        const auto makeDestinationKey = [](RE::TESQuest* quest,
                                           RE::BGSQuestObjective* objective,
                                           RE::TESObjectREFR* ref) {
            auto* world = ref ? ref->GetWorldspace() : nullptr;
            auto* cell  = ref ? ref->GetParentCell() : nullptr;
            return QuestMarkerDestinationKey{
                quest ? quest->GetFormID() : RE::FormID{0},
                objective ? objective->index : std::uint16_t{0},
                world ? world->GetFormID() : RE::FormID{0},
                cell ? cell->GetFormID() : RE::FormID{0},
                ref ? ref->GetPositionX() : 0.0f,
                ref ? ref->GetPositionY() : 0.0f,
                ref ? ref->GetPositionZ() : 0.0f
            };
        };

        const auto emitTargetEntry = [&](RE::TESQuest*          quest,
                                         RE::BGSQuestObjective* objective,
                                         RE::TESQuestTarget*    target,
                                         std::uint32_t          instanceID) -> bool {
            if (!quest || !objective || !target)
                return false;

            if (!quest->IsRunning() || quest->IsCompleted())
                return false;

            const bool questIsActive = quest->IsActive();
            if (!questIsActive)
                return false;

            const bool isMiscellaneousQuest = quest->GetType() == RE::QUEST_DATA::Type::kMiscellaneous;
            if (isMiscellaneousQuest && !miscObjectivesVisibility.visible)
                return false;

            const std::uint32_t aliasID = target->alias;
            auto* refAlias = FindRefAlias(quest, aliasID);
            if (!refAlias)
                return false;

            auto* ref = refAlias->GetReference();
            if (!ref)
                return false;

            const bool refIsDeleted = ref->IsDeleted();
            const auto coordinates = ResolveQuestMarkerCoordinates(ref);
            auto* coordinateRef = coordinates.ref;

            if (!seenDestinations.insert(makeDestinationKey(quest, objective, coordinateRef)).second)
                return false;

            const char* questEditorIdC = quest->GetFormEditorID();
            const char* questNameC     = quest->GetFullName();
            const std::string questEditorId = questEditorIdC ? questEditorIdC : "";
            const std::string questName     = questNameC ? questNameC : "";
            const std::string objectiveText = objective->displayText.c_str()
                                                  ? objective->displayText.c_str()
                                                  : "";
            const std::string objectiveTextResolved =
                QuestText::ResolveText(quest, objectiveText, instanceID);

            nlohmann::json entry;
            entry["questFormId"]    = Common::FormIdToString(quest->GetFormID());
            entry["questEditorId"]  = questEditorId;
            entry["questName"]      = questName;
            entry["questType"]      = std::string(QuestTypeName(quest->GetType()));
            entry["isActive"]       = questIsActive;
            entry["isMiscellaneous"] = isMiscellaneousQuest;
            if (isMiscellaneousQuest) {
                entry["miscObjectivesVisible"]          = miscObjectivesVisibility.visible;
                entry["miscObjectivesVisibilityKnown"] = miscObjectivesVisibility.known;
                entry["miscObjectivesVisibilitySource"] = miscObjectivesVisibility.source;
            }
            entry["objectiveIndex"] = objective->index;
            entry["objectiveText"]  = objectiveText;
            entry["objectiveTextResolved"] = objectiveTextResolved;
            entry["aliasId"]        = aliasID;
            entry["refId"]          = Common::FormIdToString(ref->GetFormID());
            entry["isDeleted"]      = refIsDeleted;

            const char* refNameC = ref->GetDisplayFullName();
            if (!refNameC || !*refNameC)
                refNameC = ref->GetName();
            entry["name"] = refNameC ? std::string(refNameC) : std::string();

            entry["coordinateSource"] = coordinates.source;
            entry["coordinateRefId"]  = coordinateRef ? nlohmann::json(Common::FormIdToString(coordinateRef->GetFormID())) : nullptr;
            entry["coordinateRefName"] = coordinateRef ? RefDisplayName(coordinateRef) : std::string();
            if (coordinates.location) {
                const char* locationEditorId = coordinates.location->GetFormEditorID();
                const char* locationName = coordinates.location->GetFullName();
                entry["locationFormId"]  = Common::FormIdToString(coordinates.location->GetFormID());
                entry["locationEditorId"] = locationEditorId ? std::string(locationEditorId) : std::string();
                entry["locationName"]     = locationName ? std::string(locationName) : std::string();
            } else {
                entry["locationFormId"]  = nullptr;
                entry["locationEditorId"] = nullptr;
                entry["locationName"]     = nullptr;
            }

            WriteReferenceLocalSpatialJson(entry, ref);
            WriteReferenceSpatialJson(entry, coordinateRef);

            result.push_back(std::move(entry));
            return true;
        };

        std::size_t fromQuestTargets   = 0;
        std::size_t fromStaticFallback = 0;

        if (!REL::Module::IsVR()) {
            const auto base = reinterpret_cast<std::uintptr_t>(player);

            const std::size_t off = REL::Module::IsAE() ? 0x5A0 : 0x598;
            const auto&       map =
                *reinterpret_cast<const RE::BSTHashMap<RE::TESQuest*, RE::BSTArray<RE::TESQuestTarget*>*>*>(
                    base + off);

            for (const auto& kv : map) {
                auto* quest = kv.first;
                if (!quest)
                    continue;
                auto* targetArray = kv.second;
                if (!targetArray)
                    continue;

                for (auto* target : *targetArray) {
                    if (!target)
                        continue;

                    auto* matchedObj = FindObjectiveForTarget(quest, target);
                    if (!matchedObj)
                        continue;

                    const auto instanceID = QuestText::FindObjectiveInstanceID(player, matchedObj);
                    if (emitTargetEntry(quest, matchedObj, target, instanceID))
                        ++fromQuestTargets;
                }
            }
        } else if (auto* handler = RE::TESDataHandler::GetSingleton()) {
            const auto& quests = handler->GetFormArray<RE::TESQuest>();
            for (auto* quest : quests) {
                if (!quest)
                    continue;
                if (!quest->IsActive())
                    continue;
                if (!quest->IsRunning() || quest->IsCompleted())
                    continue;

                for (auto* objective : quest->objectives) {
                    if (!objective)
                        continue;
                    if (objective->state != RE::QUEST_OBJECTIVE_STATE::kDisplayed)
                        continue;

                    const auto numTargets = objective->numTargets;
                    for (std::uint32_t i = 0; i < numTargets; ++i) {
                        auto* target = objective->targets ? objective->targets[i] : nullptr;
                        if (!target)
                            continue;
                        if (emitTargetEntry(quest, objective, target, quest->currentInstanceID))
                            ++fromStaticFallback;
                    }
                }
            }
        }

        logger::info("[Map::Markers::Quests] markers={} (questTargets={}, staticFallback={})",
                     result.size(), fromQuestTargets, fromStaticFallback);
        return result;
    }

    nlohmann::json ReadQuestMarkersDebug()
    {
        nlohmann::json out = nlohmann::json::object();

        out["module"] = {
            { "isAE", REL::Module::IsAE() },
            { "isVR", REL::Module::IsVR() }
        };
        out["module"]["questTargetsOffset"] = REL::Module::IsVR() ? nullptr : nlohmann::json(REL::Module::IsAE() ? "0x5A0" : "0x598");
        out["module"]["objectivesOffset"] = REL::Module::IsVR() ? nullptr : nlohmann::json(REL::Module::IsAE() ? "0x588" : "0x580");
        out["miscObjectivesVisibility"] = MiscObjectivesVisibilityJson(GetMiscObjectivesVisibility());
        out["notes"] = nlohmann::json::array({
            "For SE/AE, compare questTargets entries with the quest arrows visible in-game.",
            "runtimeObjectives shows displayed objectives but does not by itself mean the player tracks them.",
            "Miscellaneous markers require both the individual active quest flag and the journal's master Miscellaneous toggle.",
            "questTargets[].targets[].mapCoordinates shows the map-facing coordinate ref used by Map::Markers::Quests.",
            "questTargets[].targets[].coordinateDiagnostics lists alternative coordinate candidates (target ref, editor location, location markers, specialRefs, persistent ExtraMapMarker refs, linked doors) for mismatched map positions.",
            "staticDisplayedObjectives is intentionally broad and is useful for finding which quest/objective is missing from questTargets."
        });

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            out["error"] = "no PlayerCharacter";
            out["markers"] = nlohmann::json::array();
            return out;
        }

        std::unordered_set<RE::TESQuestTarget*> questTargetPointers;
        std::unordered_set<RE::BGSQuestObjective*> questTargetObjectives;

        out["questTargets"] = nlohmann::json::array();
        out["runtimeObjectives"] = nlohmann::json::array();
        out["staticDisplayedObjectives"] = nlohmann::json::array();

        if (!REL::Module::IsVR()) {
            const auto base = reinterpret_cast<std::uintptr_t>(player);

            const std::size_t questTargetsOff = REL::Module::IsAE() ? 0x5A0 : 0x598;
            const auto&       map =
                *reinterpret_cast<const RE::BSTHashMap<RE::TESQuest*, RE::BSTArray<RE::TESQuestTarget*>*>*>(
                    base + questTargetsOff);

            for (const auto& kv : map) {
                auto* quest = kv.first;
                auto* targetArray = kv.second;
                nlohmann::json group = nlohmann::json::object();
                group["quest"] = QuestDebugJson(quest);
                group["targetArrayPtr"] = PtrString(targetArray);
                group["targets"] = nlohmann::json::array();

                if (targetArray) {
                    for (auto* target : *targetArray) {
                        questTargetPointers.insert(target);
                        auto* objective = FindObjectiveForTarget(quest, target);
                        if (objective)
                            questTargetObjectives.insert(objective);
                        const auto instanceID = QuestText::FindObjectiveInstanceID(player, objective);
                        group["targets"].push_back(QuestTargetDebugJson(quest, objective, target, instanceID, true));
                    }
                }
                out["questTargets"].push_back(std::move(group));
            }

            const std::size_t objectivesOff = REL::Module::IsAE() ? 0x588 : 0x580;
            const auto&       instances =
                *reinterpret_cast<const RE::BSTArray<RE::BGSInstancedQuestObjective>*>(base + objectivesOff);

            nlohmann::json stateCounts = nlohmann::json::object();
            for (const auto& inst : instances) {
                const auto* stateName = ObjectiveStateName(inst.InstanceState);
                stateCounts[stateName] = stateCounts.value(stateName, 0) + 1;

                auto* objective = inst.Objective;
                if (!objective)
                    continue;

                nlohmann::json entry = ObjectiveDebugJson(objective, inst.instanceID);
                entry["instanceState"] = ObjectiveStateName(inst.InstanceState);
                entry["inQuestTargets"] = questTargetObjectives.contains(objective);
                entry["quest"] = QuestDebugJson(objective->ownerQuest);
                entry["targets"] = nlohmann::json::array();

                for (std::uint32_t i = 0; objective->targets && i < objective->numTargets; ++i) {
                    auto* target = objective->targets[i];
                    auto targetJson = QuestTargetDebugJson(objective->ownerQuest, objective, target, inst.instanceID);
                    targetJson["inQuestTargets"] = questTargetPointers.contains(target);
                    entry["targets"].push_back(std::move(targetJson));
                }

                out["runtimeObjectives"].push_back(std::move(entry));
            }
            out["runtimeObjectiveStateCounts"] = std::move(stateCounts);
        }

        if (auto* handler = RE::TESDataHandler::GetSingleton()) {
            const auto& quests = handler->GetFormArray<RE::TESQuest>();
            nlohmann::json displayedByType = nlohmann::json::object();

            for (auto* quest : quests) {
                if (!quest || !quest->IsRunning() || quest->IsCompleted())
                    continue;

                for (auto* objective : quest->objectives) {
                    if (!objective || objective->state != RE::QUEST_OBJECTIVE_STATE::kDisplayed)
                        continue;

                    const auto type = std::string(QuestTypeName(quest->GetType()));
                    displayedByType[type] = displayedByType.value(type, 0) + 1;

                    nlohmann::json entry = ObjectiveDebugJson(objective, quest->currentInstanceID);
                    entry["quest"] = QuestDebugJson(quest);
                    entry["inQuestTargets"] = questTargetObjectives.contains(objective);
                    entry["targets"] = nlohmann::json::array();
                    for (std::uint32_t i = 0; objective->targets && i < objective->numTargets; ++i) {
                        auto* target = objective->targets[i];
                        auto targetJson = QuestTargetDebugJson(quest, objective, target, quest->currentInstanceID);
                        targetJson["inQuestTargets"] = questTargetPointers.contains(target);
                        entry["targets"].push_back(std::move(targetJson));
                    }
                    out["staticDisplayedObjectives"].push_back(std::move(entry));
                }
            }

            out["staticDisplayedByType"] = std::move(displayedByType);
        }

        out["markers"] = ReadQuestMarkers();
        out["counts"] = {
            { "questTargets", out["questTargets"].size() },
            { "runtimeObjectives", out["runtimeObjectives"].size() },
            { "staticDisplayedObjectives", out["staticDisplayedObjectives"].size() },
            { "markers", out["markers"].size() }
        };
        return out;
    }
}