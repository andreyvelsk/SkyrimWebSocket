#include "QuestMarkers.h"
#include "Common.h"
#include "PlayerPosition.h"
#include "QuestText.h"

#include "../../logger.h"

#include <array>
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace QuestMarkers
{
    namespace
    {
        // Miscellaneous-objectives master toggle (journal UI state).
        struct MiscObjectivesVisibility
        {
            bool visible = true;
            bool known = false;
            const char* source = "default-visible";
            bool journalOpen = false;
            bool cachedKnown = false;
            bool cachedVisible = true;
            bool nativeKnown = false;
            bool nativeVisible = true;
        };

        nlohmann::json MiscObjectivesVisibilityJson(const MiscObjectivesVisibility& state)
        {
            return {
                { "visible", state.visible },
                { "known", state.known },
                { "source", state.source },
                { "journalOpen", state.journalOpen },
                { "cachedKnown", state.cachedKnown },
                { "cachedVisible", state.cachedVisible },
                { "nativeKnown", state.nativeKnown },
                { "nativeVisible", state.nativeVisible }
            };
        }

        bool g_miscKnown = false;
        bool g_miscVisible = true;
        const char* g_miscSource = "cached misc objectives visibility";

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

        MiscObjectivesVisibility GetMiscObjectivesVisibility()
        {
            MiscObjectivesVisibility state;
            state.cachedKnown = g_miscKnown;
            state.cachedVisible = g_miscVisible;

            // A command override wins until the next save load.
            if (g_miscKnown && std::string_view(g_miscSource) == "command") {
                state.visible = g_miscVisible;
                state.known = true;
                state.source = g_miscSource;
                return state;
            }

            if (auto* ui = RE::UI::GetSingleton()) {
                if (auto journal = ui->GetMenu<RE::JournalMenu>()) {
                    state.journalOpen = true;
                    // Native master toggle on the journal's quests tab.
                    const bool native = journal->GetRuntimeData().questsTab.unk30;
                    state.nativeKnown = true;
                    state.nativeVisible = native;
                    g_miscKnown = true;
                    g_miscVisible = native;
                    g_miscSource = "Journal_QuestsTab::unk30";
                }
            }

            if (g_miscKnown) {
                state.visible = g_miscVisible;
                state.known = true;
                state.source = g_miscSource;
            }
            return state;
        }

        // ---------------------------------------------------------------------------
        // Small helpers
        // ---------------------------------------------------------------------------
        std::string PtrString(const void* ptr)
        {
            return std::format("0x{:016X}", reinterpret_cast<std::uintptr_t>(ptr));
        }

        std::string RefDisplayName(RE::TESObjectREFR* ref)
        {
            if (!ref)
                return {};
            if (const char* full = ref->GetDisplayFullName(); full && *full)
                return full;
            if (const char* name = ref->GetName(); name && *name)
                return name;
            if (auto* base = ref->GetBaseObject(); base && base->GetName() && *base->GetName())
                return base->GetName();
            return {};
        }

        RE::TESWorldSpace* RootWorldspace(RE::TESWorldSpace* world)
        {
            auto* root = world;
            while (root && root->parentWorld)
                root = root->parentWorld;
            return root;
        }

        bool IsMapMarkerRef(RE::TESObjectREFR* ref)
        {
            auto* extra = ref ? ref->extraList.GetByType<RE::ExtraMapMarker>() : nullptr;
            return extra && extra->mapData;
        }

        // True when the reference lives in a top-level (parentless) exterior
        // worldspace — i.e. its coordinates are directly usable on the
        // global map.
        bool HasGlobalMapCoordinates(RE::TESObjectREFR* ref)
        {
            if (!ref || !ref->GetFormID())
                return false;
            if (auto* cell = ref->GetParentCell(); cell && cell->IsInteriorCell())
                return false;
            auto* world = ref->GetWorldspace();
            return world && !world->parentWorld;
        }

        RE::BGSRefAlias* FindRefAlias(RE::TESQuest* quest, std::uint32_t aliasID)
        {
            if (!quest)
                return nullptr;
            for (auto* alias : quest->aliases) {
                if (alias && alias->aliasID == aliasID) {
                    if (alias->GetVMTypeID() == RE::BGSRefAlias::VMTYPEID)
                        return static_cast<RE::BGSRefAlias*>(alias);
                    return nullptr;
                }
            }
            return nullptr;
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

        // ---------------------------------------------------------------------------
        // Native coordinate resolution
        //
        // Priority:
        //   1. target already in a top-level worldspace
        //   2. TESQuestTarget::GetTrackingRef — the ref the engine itself tracks
        //   3. TESQuestTarget::teleportPath — the engine's own door chain; when
        //      either end lies in a top-level worldspace its endpoint (start/end)
        //      is a ready-to-use global position
        //   4. nearest ancestor BGSLocation::worldLocMarker
        // ---------------------------------------------------------------------------
        struct TargetCoordinates
        {
            RE::TESObjectREFR* ref = nullptr;
            const char* source = "unresolved:noGlobalCoordinates";
            bool hasOverride = false;
            RE::NiPoint3 overridePos{};
        };

        bool NodeEndsAtTopLevelWorld(const RE::TeleportPath::ParentSpaceNode& node)
        {
            if (!node.isWorldspace || !node.worldspace || !node.worldspace->GetFormID())
                return false;
            return RootWorldspace(node.worldspace) == node.worldspace;
        }

        TargetCoordinates ResolveTargetCoordinates(RE::TESQuest* quest,
                                                   RE::TESQuestTarget* target,
                                                   RE::TESObjectREFR* targetRef)
        {
            TargetCoordinates out;

            // 1. Target already map-facing.
            if (HasGlobalMapCoordinates(targetRef)) {
                out.ref = targetRef;
                out.source = "targetRef:global";
                return out;
            }

            if (!quest || !target || !quest->IsRunning())
                return out;

            // 2. The engine's own tracking ref.
            RE::ObjectRefHandle trackingHandle;
            target->GetTrackingRef(trackingHandle, quest);
            if (auto* trackingRef = trackingHandle.get().get();
                trackingRef && HasGlobalMapCoordinates(trackingRef)) {
                out.ref = trackingRef;
                out.source = "nativeTrackingRef";
                return out;
            }

            // 3. The engine's native door-chain path. Only trusted for
            // actively running quests where the engine maintains it.
            const auto& path = target->teleportPath;
            if (!path.spaces.empty() && path.spaces.size() <= 16 &&
                path.teleportRefs.size() <= 16) {
                if (NodeEndsAtTopLevelWorld(path.spaces.front())) {
                    out.hasOverride = true;
                    out.overridePos = path.start;
                    out.source = "teleportPath.start";
                    if (!path.teleportRefs.empty())
                        out.ref = path.teleportRefs.front().ref;
                    return out;
                }
                if (NodeEndsAtTopLevelWorld(path.spaces.back())) {
                    out.hasOverride = true;
                    out.overridePos = path.end;
                    out.source = "teleportPath.end";
                    if (!path.teleportRefs.empty())
                        out.ref = path.teleportRefs.back().ref;
                    return out;
                }
            }

            // 4. Nearest ancestor location with a map-facing marker.
            if (targetRef) {
                std::array<RE::BGSLocation*, 3> candidates{
                    targetRef->GetCurrentLocation(),
                    targetRef->GetEditorLocation(),
                    targetRef->GetParentCell() ? targetRef->GetParentCell()->GetLocation()
                                               : nullptr
                };
                std::unordered_set<RE::FormID> seen;
                for (auto* location : candidates) {
                    for (auto* candidate = location; candidate; candidate = candidate->parentLoc) {
                        if (!candidate || !seen.insert(candidate->GetFormID()).second)
                            continue;
                        auto marker = candidate->worldLocMarker.get();
                        if (marker && HasGlobalMapCoordinates(marker.get())) {
                            out.ref = marker.get();
                            out.source = "BGSLocation::worldLocMarker";
                            return out;
                        }
                    }
                }

                // 5. Exit doors of the interior cell (single hop into a
                // top-level worldspace).
                if (auto* cell = targetRef->GetParentCell(); cell && cell->IsInteriorCell()) {
                    RE::TESObjectREFR* exitRef = nullptr;
                    cell->ForEachReference([&](RE::TESObjectREFR* candidate) {
                        if (exitRef || !candidate)
                            return RE::BSContainer::ForEachResult::kContinue;
                        auto* teleport = candidate->extraList.GetByType<RE::ExtraTeleport>();
                        const auto* tpData = teleport ? teleport->teleportData : nullptr;
                        if (!tpData)
                            return RE::BSContainer::ForEachResult::kContinue;
                        auto linked = tpData->linkedDoor.get();
                        if (linked && HasGlobalMapCoordinates(linked.get())) {
                            exitRef = linked.get();
                            return RE::BSContainer::ForEachResult::kStop;
                        }
                        return RE::BSContainer::ForEachResult::kContinue;
                    });
                    if (exitRef) {
                        out.ref = exitRef;
                        out.source = "linkedTeleportDoor.exit";
                        return out;
                    }
                }
            }

            return out;
        }

        // ---------------------------------------------------------------------------
        // JSON writers
        // ---------------------------------------------------------------------------
        constexpr std::array SpatialKeys{
            "x", "y", "z",
            "worldspace", "worldspaceFormId",
            "parentWorldspace", "parentWorldspaceFormId",
            "cell", "cellFormId", "isInterior"
        };
        constexpr std::array LocalSpatialKeys{
            "localX", "localY", "localZ",
            "localWorldspace", "localWorldspaceFormId",
            "localParentWorldspace", "localParentWorldspaceFormId",
            "localCell", "localCellFormId", "localIsInterior"
        };

        void WriteSpatialJson(nlohmann::json& out,
                              const std::array<const char*, 10>& keys,
                              RE::TESObjectREFR* ref)
        {
            if (!ref) {
                for (std::size_t i = 0; i < 9; ++i)
                    out[keys[i]] = nullptr;
                out[keys[9]] = false;
                return;
            }
            out[keys[0]] = ref->GetPositionX();
            out[keys[1]] = ref->GetPositionY();
            out[keys[2]] = ref->GetPositionZ();
            Common::BuildWorldspaceFields(out, ref->GetWorldspace());
            if (auto* cell = ref->GetParentCell()) {
                const char* edid = cell->GetFormEditorID();
                out[keys[7]] = edid ? std::string(edid) : std::string();
                out[keys[8]] = Common::FormIdToString(cell->GetFormID());
                out[keys[9]] = cell->IsInteriorCell();
            } else {
                out[keys[7]] = nullptr;
                out[keys[8]] = nullptr;
                out[keys[9]] = false;
            }
        }

        void WriteCoordinatesJson(nlohmann::json& out, const TargetCoordinates& coords)
        {
            out["coordinateSource"] = coords.source;
            out["coordinateRefId"] = coords.ref
                                         ? nlohmann::json(Common::FormIdToString(coords.ref->GetFormID()))
                                         : nlohmann::json(nullptr);
            out["coordinateRefName"] = RefDisplayName(coords.ref);
            if (coords.hasOverride) {
                out["x"] = coords.overridePos.x;
                out["y"] = coords.overridePos.y;
                out["z"] = coords.overridePos.z;
                auto* world = coords.ref ? coords.ref->GetWorldspace() : nullptr;
                Common::BuildWorldspaceFields(out, RootWorldspace(world));
                out["cell"] = nullptr;
                out["cellFormId"] = nullptr;
                out["isInterior"] = false;
            } else {
                WriteSpatialJson(out, SpatialKeys, coords.ref);
            }
        }

        // Guarded dump of the engine's native TeleportPath. The engine only
        // populates it for actively tracked targets; never dereference data
        // that looks uninitialised.
        nlohmann::json TeleportPathJson(RE::TESQuest* quest, RE::TESQuestTarget* target)
        {
            if (!target)
                return nullptr;

            const auto& path = target->teleportPath;
            nlohmann::json j;
            j["spaceCount"] = path.spaces.size();
            j["teleportRefCount"] = path.teleportRefs.size();

            if (!quest || !quest->IsRunning() ||
                path.spaces.empty() || path.spaces.size() > 16 ||
                path.teleportRefs.size() > 16) {
                j["skipped"] = true;
                return j;
            }

            j["spaces"] = nlohmann::json::array();
            for (const auto& space : path.spaces) {
                nlohmann::json sj;
                sj["isWorldspace"] = space.isWorldspace;
                if (space.isWorldspace && space.worldspace && space.worldspace->GetFormID()) {
                    const char* edid = space.worldspace->GetFormEditorID();
                    sj["worldspace"] = edid ? std::string(edid) : std::string();
                    sj["worldspaceFormId"] = Common::FormIdToString(space.worldspace->GetFormID());
                    sj["isTopLevelWorld"] = !space.worldspace->parentWorld;
                } else if (!space.isWorldspace && space.interiorCell &&
                           space.interiorCell->GetFormID()) {
                    const char* cedid = space.interiorCell->GetFormEditorID();
                    sj["interiorCell"] = cedid ? std::string(cedid) : std::string();
                    sj["interiorCellFormId"] = Common::FormIdToString(space.interiorCell->GetFormID());
                }
                j["spaces"].push_back(std::move(sj));
            }

            j["teleportRefs"] = nlohmann::json::array();
            for (const auto& link : path.teleportRefs) {
                nlohmann::json lj;
                if (link.ref && link.ref->GetFormID()) {
                    lj["refId"] = Common::FormIdToString(link.ref->GetFormID());
                    lj["refName"] = RefDisplayName(link.ref);
                    lj["isTopLevelWorld"] = HasGlobalMapCoordinates(link.ref);
                } else {
                    lj["refId"] = nullptr;
                }
                lj["teleportLocation"] = {
                    { "x", link.teleportLocation.x }, { "y", link.teleportLocation.y },
                    { "z", link.teleportLocation.z }
                };
                j["teleportRefs"].push_back(std::move(lj));
            }

            j["start"] = { { "x", path.start.x }, { "y", path.start.y }, { "z", path.start.z } };
            j["end"] = { { "x", path.end.x }, { "y", path.end.y }, { "z", path.end.z } };
            return j;
        }

        nlohmann::json QuestDebugJson(RE::TESQuest* quest)
        {
            nlohmann::json out;
            if (!quest)
                return out;
            const char* editorID = quest->GetFormEditorID();
            const char* name = quest->GetFullName();
            out["formId"] = Common::FormIdToString(quest->GetFormID());
            out["editorId"] = editorID ? std::string(editorID) : std::string();
            out["name"] = name ? std::string(name) : std::string();
            out["type"] = std::string(QuestTypeName(quest->GetType()));
            out["isRunning"] = quest->IsRunning();
            out["isActiveFlag"] = quest->IsActive();
            return out;
        }

        nlohmann::json ObjectiveDebugJson(RE::BGSQuestObjective* objective, std::uint32_t instanceID)
        {
            nlohmann::json out;
            if (!objective)
                return out;
            out["index"] = objective->index;
            out["numTargets"] = objective->numTargets;
            out["stateRaw"] = static_cast<std::uint8_t>(objective->state.underlying());
            out["text"] = objective->displayText.c_str() ? objective->displayText.c_str() : "";
            out["instanceID"] = instanceID;
            return out;
        }

        nlohmann::json QuestTargetDebugJson(RE::TESQuest* quest,
                                            RE::BGSQuestObjective* objective,
                                            RE::TESQuestTarget* target,
                                            std::uint32_t instanceID)
        {
            nlohmann::json out;
            if (!target)
                return out;

            out["ptr"] = PtrString(target);
            out["aliasId"] = target->alias;
            out["objective"] = ObjectiveDebugJson(objective, instanceID);
            out["teleportPath"] = TeleportPathJson(quest, target);

            auto* refAlias = FindRefAlias(quest, target->alias);
            out["refAliasFound"] = refAlias != nullptr;
            RE::TESObjectREFR* ref = refAlias ? refAlias->GetReference() : nullptr;
            if (ref) {
                out["refId"] = Common::FormIdToString(ref->GetFormID());
                out["name"] = RefDisplayName(ref);
                WriteSpatialJson(out, LocalSpatialKeys, ref);
                const auto coords = ResolveTargetCoordinates(quest, target, ref);
                WriteCoordinatesJson(out, coords);
            } else {
                out["refId"] = nullptr;
            }
            return out;
        }
    }  // namespace

    void CaptureQuestJournalState()
    {
        // Live state is picked up by GetMiscObjectivesVisibility whenever the
        // journal menu is open; nothing extra to capture here.
    }

    void ResetQuestJournalState()
    {
        g_miscKnown = false;
        g_miscVisible = true;
        g_miscSource = "cached misc objectives visibility";
    }

    void SetMiscObjectivesVisible(bool visible)
    {
        g_miscKnown = true;
        g_miscVisible = visible;
        g_miscSource = "command";
        // Apply to the live journal menu when it is open.
        //
        // Limitation: this writes the native master-toggle field directly.
        // The engine's ToggleShowMiscObjectives is a Scaleform-registered
        // callback and cannot be invoked from SKSE, so the journal's
        // TitleList checkbox does not visually refresh until the journal is
        // reopened. The filter itself takes effect immediately.
        if (auto* ui = RE::UI::GetSingleton()) {
            if (auto journal = ui->GetMenu<RE::JournalMenu>())
                journal->GetRuntimeData().questsTab.unk30 = visible;
        }
        logger::info("[Map::Markers::Quests] misc objectives visibility set to {} by command", visible);
    }

    nlohmann::json ReadQuestMarkers()
    {
        nlohmann::json result = nlohmann::json::array();

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            logger::warn("[Map::Markers::Quests] no PlayerCharacter");
            return result;
        }

        const auto miscVisibility = GetMiscObjectivesVisibility();

        struct DestKey
        {
            RE::FormID quest;
            std::uint16_t objective;
            float x, y;
            bool operator==(const DestKey&) const = default;
        };
        struct DestKeyHash
        {
            std::size_t operator()(const DestKey& k) const noexcept
            {
                std::size_t seed = 0;
                auto combine = [&seed](auto v) {
                    seed ^= std::hash<decltype(v)>{}(v) + 0x9E3779B97F4A7C15ull + (seed << 6) + (seed >> 2);
                };
                combine(k.quest);
                combine(k.objective);
                combine(k.x);
                combine(k.y);
                return seed;
            }
        };
        std::unordered_set<DestKey, DestKeyHash> seenDestinations;

        const auto emitTarget = [&](RE::TESQuest* quest,
                                    RE::BGSQuestObjective* objective,
                                    RE::TESQuestTarget* target,
                                    std::uint32_t instanceID) -> bool {
            if (!quest || !objective || !target)
                return false;
            if (!quest->IsRunning() || quest->IsCompleted() || !quest->IsActive())
                return false;

            const bool isMisc = quest->GetType() == RE::QUEST_DATA::Type::kMiscellaneous;
            if (isMisc && !miscVisibility.visible)
                return false;

            auto* refAlias = FindRefAlias(quest, target->alias);
            if (!refAlias)
                return false;
            auto* ref = refAlias->GetReference();
            if (!ref)
                return false;

            const auto coords = ResolveTargetCoordinates(quest, target, ref);
            if (!coords.ref && !coords.hasOverride)
                return false;

            const float px = coords.hasOverride ? coords.overridePos.x : coords.ref->GetPositionX();
            const float py = coords.hasOverride ? coords.overridePos.y : coords.ref->GetPositionY();
            if (!seenDestinations.insert(DestKey{ quest->GetFormID(), objective->index, px, py }).second)
                return false;

            const char* editorIdC = quest->GetFormEditorID();
            const char* questNameC = quest->GetFullName();

            nlohmann::json entry;
            entry["questFormId"] = Common::FormIdToString(quest->GetFormID());
            entry["questEditorId"] = editorIdC ? std::string(editorIdC) : std::string();
            entry["questName"] = questNameC ? std::string(questNameC) : std::string();
            entry["questType"] = std::string(QuestTypeName(quest->GetType()));
            entry["isActive"] = true;
            entry["isMiscellaneous"] = isMisc;
            if (isMisc) {
                entry["miscObjectivesVisible"] = miscVisibility.visible;
                entry["miscObjectivesVisibilityKnown"] = miscVisibility.known;
                entry["miscObjectivesVisibilitySource"] = miscVisibility.source;
            }
            entry["objectiveIndex"] = objective->index;
            const std::string objectiveText =
                objective->displayText.c_str() ? objective->displayText.c_str() : "";
            entry["objectiveText"] = objectiveText;
            entry["objectiveTextResolved"] = QuestText::ResolveText(quest, objectiveText, instanceID);
            entry["aliasId"] = target->alias;
            entry["refId"] = Common::FormIdToString(ref->GetFormID());
            entry["isDeleted"] = ref->IsDeleted();
            entry["name"] = RefDisplayName(ref);

            WriteSpatialJson(entry, LocalSpatialKeys, ref);
            WriteCoordinatesJson(entry, coords);

            result.push_back(std::move(entry));
            return true;
        };

        std::size_t emitted = 0;

        if (!REL::Module::IsVR()) {
            const auto base = reinterpret_cast<std::uintptr_t>(player);
            const std::size_t off = REL::Module::IsAE() ? 0x5A0 : 0x598;
            const auto& map =
                *reinterpret_cast<const RE::BSTHashMap<RE::TESQuest*, RE::BSTArray<RE::TESQuestTarget*>*>*>(
                    base + off);

            for (const auto& kv : map) {
                auto* quest = kv.first;
                auto* targets = kv.second;
                if (!quest || !targets)
                    continue;

                for (auto* target : *targets) {
                    if (!target)
                        continue;
                    auto* objective = FindObjectiveForTarget(quest, target);
                    if (!objective)
                        continue;
                    const auto instanceID = QuestText::FindObjectiveInstanceID(player, objective);
                    if (emitTarget(quest, objective, target, instanceID))
                        ++emitted;
                }
            }
        } else if (auto* handler = RE::TESDataHandler::GetSingleton()) {
            // VR fallback: static displayed objectives.
            for (auto* quest : handler->GetFormArray<RE::TESQuest>()) {
                if (!quest || !quest->IsActive() || !quest->IsRunning() || quest->IsCompleted())
                    continue;
                for (auto* objective : quest->objectives) {
                    if (!objective || objective->state != RE::QUEST_OBJECTIVE_STATE::kDisplayed)
                        continue;
                    for (std::uint32_t i = 0; objective->targets && i < objective->numTargets; ++i) {
                        if (emitTarget(quest, objective, objective->targets[i],
                                       quest->currentInstanceID))
                            ++emitted;
                    }
                }
            }
        }

        logger::info("[Map::Markers::Quests] markers={}", emitted);
        return result;
    }

    nlohmann::json ReadQuestMarkersDebug()
    {
        nlohmann::json out;
        out["module"] = {
            { "isAE", REL::Module::IsAE() },
            { "isVR", REL::Module::IsVR() }
        };
        out["miscObjectivesVisibility"] = MiscObjectivesVisibilityJson(GetMiscObjectivesVisibility());

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            out["error"] = "no PlayerCharacter";
            return out;
        }

        out["trackedTargets"] = nlohmann::json::array();
        if (!REL::Module::IsVR()) {
            const auto base = reinterpret_cast<std::uintptr_t>(player);
            const std::size_t off = REL::Module::IsAE() ? 0x5A0 : 0x598;
            const auto& map =
                *reinterpret_cast<const RE::BSTHashMap<RE::TESQuest*, RE::BSTArray<RE::TESQuestTarget*>*>*>(
                    base + off);

            for (const auto& kv : map) {
                auto* quest = kv.first;
                auto* targets = kv.second;
                if (!quest || !targets)
                    continue;
                for (auto* target : *targets) {
                    if (!target)
                        continue;
                    auto* objective = FindObjectiveForTarget(quest, target);
                    out["trackedTargets"].push_back(
                        QuestTargetDebugJson(quest, objective, target,
                                             objective ? QuestText::FindObjectiveInstanceID(player, objective) : 0));
                }
            }
        }
        out["markers"] = ReadQuestMarkers();
        return out;
    }

    nlohmann::json DebugQuestMarker(RE::FormID questFormId)
    {
        nlohmann::json out;
        auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(questFormId);
        if (!quest) {
            out["error"] = std::format("No TESQuest with formId 0x{:08X}", questFormId);
            return out;
        }

        out["quest"] = QuestDebugJson(quest);

        out["objectives"] = nlohmann::json::array();
        for (auto* objective : quest->objectives) {
            if (!objective)
                continue;
            nlohmann::json oj = ObjectiveDebugJson(objective, quest->currentInstanceID);
            oj["targets"] = nlohmann::json::array();
            for (std::uint32_t i = 0; objective->targets && i < objective->numTargets; ++i) {
                oj["targets"].push_back(
                    QuestTargetDebugJson(quest, objective, objective->targets[i],
                                         quest->currentInstanceID));
            }
            out["objectives"].push_back(std::move(oj));
        }

        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            out["player"]["x"] = player->GetPositionX();
            out["player"]["y"] = player->GetPositionY();
            out["player"]["z"] = player->GetPositionZ();
            auto& rt = player->GetPlayerRuntimeData();
            if (rt.cachedWorldSpace) {
                const char* edid = rt.cachedWorldSpace->GetFormEditorID();
                out["player"]["cachedWorldspace"] = edid ? std::string(edid) : std::string();
                out["player"]["exteriorPosition"] = {
                    { "x", rt.exteriorPosition.x }, { "y", rt.exteriorPosition.y },
                    { "z", rt.exteriorPosition.z }
                };
            }
        }
        return out;
    }
}
