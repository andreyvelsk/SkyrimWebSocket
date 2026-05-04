#include "PlayerReader.h"

#include "../../logger.h"

#include <cctype>
#include <unordered_set>

namespace PlayerReader
{
    nlohmann::json ReadLevel()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return 0;
        return static_cast<int>(player->GetLevel());
    }

    nlohmann::json ReadXPCurrent()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return 0.0f;
        auto& info = player->GetInfoRuntimeData();
        if (!info.skills || !info.skills->data)
            return 0.0f;
        return info.skills->data->xp;
    }

    nlohmann::json ReadXPNext()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return 0.0f;
        auto& info = player->GetInfoRuntimeData();
        if (!info.skills || !info.skills->data)
            return 0.0f;
        return info.skills->data->levelThreshold;
    }

    nlohmann::json ReadXPLevelStart()
    {
        return 0.0f;
    }

    nlohmann::json ReadInventoryWeight()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return 0.0f;
        return player->GetWeightInContainer();
    }

    nlohmann::json ReadCarryWeight()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return 0.0f;
        auto* avo = player->AsActorValueOwner();
        if (!avo)
            return 0.0f;
        return avo->GetActorValue(RE::ActorValue::kCarryWeight);
    }

    nlohmann::json ReadLanguage()
    {
        static constexpr const char* kDefaultLanguage = "english";
        auto* settings = RE::INISettingCollection::GetSingleton();
        if (!settings)
            return kDefaultLanguage;
        auto* setting = settings->GetSetting("sLanguage:General");
        if (!setting)
            return kDefaultLanguage;
        const char* str = setting->GetString();
        return str ? std::string(str) : kDefaultLanguage;
    }

    nlohmann::json ReadPosition()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::object();

        nlohmann::json pos;
        pos["x"]     = player->GetPositionX();
        pos["y"]     = player->GetPositionY();
        pos["z"]     = player->GetPositionZ();
        pos["angle"] = player->GetAngleZ();

        auto formIdStr = [](RE::FormID id) {
            return std::format("0x{:08X}", id);
        };

        auto* world = player->GetWorldspace();
        if (world) {
            const char* edid = world->GetFormEditorID();
            pos["worldspace"]       = edid ? std::string(edid) : std::string();
            pos["worldspaceFormId"] = formIdStr(world->GetFormID());

            auto* root = world;
            while (root->parentWorld)
                root = root->parentWorld;
            const char* rootEdid = root->GetFormEditorID();
            pos["parentWorldspace"]       = rootEdid ? std::string(rootEdid) : std::string();
            pos["parentWorldspaceFormId"] = formIdStr(root->GetFormID());
        } else {
            pos["worldspace"]             = nullptr;
            pos["worldspaceFormId"]       = nullptr;
            pos["parentWorldspace"]       = nullptr;
            pos["parentWorldspaceFormId"] = nullptr;
        }

        auto* cell = player->GetParentCell();
        if (cell) {
            const char* cedid = cell->GetFormEditorID();
            pos["cell"]       = cedid ? std::string(cedid) : std::string();
            pos["cellFormId"] = formIdStr(cell->GetFormID());
            pos["isInterior"] = cell->IsInteriorCell();
        } else {
            pos["cell"]       = nullptr;
            pos["cellFormId"] = nullptr;
            pos["isInterior"] = false;
        }

        return pos;
    }

    nlohmann::json ReadExteriorPosition()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::object();

        // PLAYER_RUNTIME_DATA fields are not exposed as struct members in
        // multi-targeting builds (HAS_SKYRIM_MULTI_TARGETING=1), so we resolve
        // them by absolute offsets from the PlayerCharacter base. Offsets are
        // taken from CommonLibSSE-NG's PlayerCharacter.h:
        //   cachedWorldSpace : SE 0x628, AE 0x630, VR 0xC18
        //   exteriorPosition : SE 0x630, AE 0x638, VR 0xC20
        std::size_t worldOff = 0;
        std::size_t posOff   = 0;
        if (REL::Module::IsVR()) {
            worldOff = 0xC18;
            posOff   = 0xC20;
        } else if (REL::Module::IsAE()) {
            worldOff = 0x630;
            posOff   = 0x638;
        } else {  // SE
            worldOff = 0x628;
            posOff   = 0x630;
        }

        const auto base    = reinterpret_cast<std::uintptr_t>(player);
        auto*       world  = *reinterpret_cast<RE::TESWorldSpace**>(base + worldOff);
        const auto& extPos = *reinterpret_cast<const RE::NiPoint3*>(base + posOff);

        nlohmann::json out;
        out["x"] = extPos.x;
        out["y"] = extPos.y;
        out["z"] = extPos.z;

        if (world) {
            const char* edid = world->GetFormEditorID();
            out["worldspace"]       = edid ? std::string(edid) : std::string();
            out["worldspaceFormId"] = std::format("0x{:08X}", world->GetFormID());

            auto* root = world;
            while (root->parentWorld)
                root = root->parentWorld;
            const char* rootEdid = root->GetFormEditorID();
            out["parentWorldspace"]       = rootEdid ? std::string(rootEdid) : std::string();
            out["parentWorldspaceFormId"] = std::format("0x{:08X}", root->GetFormID());
        } else {
            out["worldspace"]             = nullptr;
            out["worldspaceFormId"]       = nullptr;
            out["parentWorldspace"]       = nullptr;
            out["parentWorldspaceFormId"] = nullptr;
        }

        return out;
    }

    RE::TESObjectREFR* GetPlayerMarkerRef()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nullptr;

        // INFO_RUNTIME_DATA is exposed via the public GetInfoRuntimeData()
        // accessor in every targeting mode (SE/AE/VR), so we don't need
        // offset hacks here.
        auto& info = player->GetInfoRuntimeData();
        return info.playerMapMarker.get().get();
    }

    nlohmann::json BuildPlayerMarkerJson(RE::TESObjectREFR* ref)
    {
        nlohmann::json out;

        const auto setNullSpatial = [&]() {
            out["x"]                      = nullptr;
            out["y"]                      = nullptr;
            out["z"]                      = nullptr;
            out["worldspace"]             = nullptr;
            out["worldspaceFormId"]       = nullptr;
            out["parentWorldspace"]       = nullptr;
            out["parentWorldspaceFormId"] = nullptr;
        };

        if (!ref) {
            out["isSet"] = false;
            setNullSpatial();
            return out;
        }

        // The marker ref always exists once the engine has touched the map
        // menu; the *visibility* of its ExtraMapMarker is what tells whether
        // the player has a marker placed right now.
        auto*      extra     = ref->extraList.GetByType<RE::ExtraMapMarker>();
        const bool hasData   = extra && extra->mapData;
        const bool isVisible = hasData && extra->mapData->flags.any(RE::MapMarkerData::Flag::kVisible);

        out["isSet"] = isVisible;
        out["x"]     = ref->GetPositionX();
        out["y"]     = ref->GetPositionY();
        out["z"]     = ref->GetPositionZ();

        const auto formIdStr = [](RE::FormID id) {
            return std::format("0x{:08X}", id);
        };

        if (auto* world = ref->GetWorldspace()) {
            const char* edid = world->GetFormEditorID();
            out["worldspace"]       = edid ? std::string(edid) : std::string();
            out["worldspaceFormId"] = formIdStr(world->GetFormID());

            auto* root = world;
            while (root->parentWorld)
                root = root->parentWorld;
            const char* rootEdid = root->GetFormEditorID();
            out["parentWorldspace"]       = rootEdid ? std::string(rootEdid) : std::string();
            out["parentWorldspaceFormId"] = formIdStr(root->GetFormID());
        } else {
            out["worldspace"]             = nullptr;
            out["worldspaceFormId"]       = nullptr;
            out["parentWorldspace"]       = nullptr;
            out["parentWorldspaceFormId"] = nullptr;
        }

        return out;
    }

    nlohmann::json ReadPlayerMarker()
    {
        return BuildPlayerMarkerJson(GetPlayerMarkerRef());
    }

    static nlohmann::json ReadMapMarkersImpl(bool visibleOnly)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        // clang-format off
        static constexpr std::array<std::string_view, 66> kTypeNames = {
            "None",               //  0
            "City",               //  1
            "Town",               //  2
            "Settlement",         //  3
            "Cave",               //  4
            "Camp",               //  5
            "Fort",               //  6
            "NordicRuin",         //  7
            "DwemerRuin",         //  8
            "Shipwreck",          //  9
            "Grove",              // 10
            "Landmark",           // 11
            "DragonLair",         // 12
            "Farm",               // 13
            "WoodMill",           // 14
            "Mine",               // 15
            "ImperialCamp",       // 16
            "StormcloakCamp",     // 17
            "Doomstone",          // 18
            "WheatMill",          // 19
            "Smelter",            // 20
            "Stable",             // 21
            "ImperialTower",      // 22
            "Clearing",           // 23
            "Pass",               // 24
            "Altar",              // 25
            "Rock",               // 26
            "Lighthouse",         // 27
            "OrcStronghold",      // 28
            "GiantCamp",          // 29
            "Shack",              // 30
            "NordicTower",        // 31
            "NordicDwelling",     // 32
            "Docks",              // 33
            "Shrine",             // 34
            "RiftenCastle",       // 35
            "RiftenCapitol",      // 36
            "WindhelmCastle",     // 37
            "WindhelmCapitol",    // 38
            "WhiterunCastle",     // 39
            "WhiterunCapitol",    // 40
            "SolitudeCastle",     // 41
            "SolitudeCapitol",    // 42
            "MarkarthCastle",     // 43
            "MarkarthCapitol",    // 44
            "WinterholdCastle",   // 45
            "WinterholdCapitol",  // 46
            "MorthalCastle",      // 47
            "MorthalCapitol",     // 48
            "FalkreathCastle",    // 49
            "FalkreathCapitol",   // 50
            "DawnstarCastle",     // 51
            "DawnstarCapitol",    // 52
            "DLC02MiraakTemple",  // 53
            "DLC02RavenRock",     // 54
            "DLC02BeastStone",    // 55
            "DLC02TelMithryn",    // 56
            "DLC02ToSkyrim",      // 57
            "DLC02StalhrimSource",// 58
            "DLC02CastleKarstaag",// 59
            "Unknown",            // 60  (kTotalLocationTypes sentinel)
            "Door",               // 61
            "QuestTarget",        // 62
            "Unknown",            // 63
            "PlayerSet",          // 64
            "YouAreHere",         // 65
        };
        // clang-format on

        nlohmann::json result = nlohmann::json::array();

        // Map markers are persistent refs attached to each worldspace, NOT
        // members of TESDataHandler::GetFormArray<TESObjectREFR>() (that array
        // does not store object references). We iterate every worldspace and
        // walk its persistentCell via TESObjectCELL::ForEachReference, looking
        // for refs that carry an ExtraMapMarker.
        //
        // NOTE: this walks thousands of refs across all worldspaces.  Keep
        // this hot path silent (no per-ref logging); the per-flush-on-trace
        // fsync inside spdlog turns each log line into a multi-millisecond
        // disk write that visibly freezes the renderer.

        auto* handler = RE::TESDataHandler::GetSingleton();
        if (!handler) {
            logger::warn("[Map::Markers::Locations] no TESDataHandler");
            return result;
        }

        std::unordered_set<RE::FormID> seen;

        auto emit = [&](RE::TESObjectREFR* form) {
            if (!form)
                return;
            const auto formId = form->GetFormID();

            auto* extra = form->extraList.GetByType<RE::ExtraMapMarker>();
            if (!extra || !extra->mapData)
                return;

            if (!seen.insert(formId).second)
                return;

            auto* data = extra->mapData;

            using Flag = RE::MapMarkerData::Flag;
            const bool isVisible     = data->flags.any(Flag::kVisible);
            const bool canFastTravel = data->flags.any(Flag::kCanTravelTo);

            if (visibleOnly && !isVisible)
                return;

            // When visibleOnly is requested we want exactly what MapMenu would
            // render: skip disabled / deleted refs and nameless markers (the
            // engine itself filters those out before drawing).
            if (visibleOnly && (form->IsDisabled() || form->IsDeleted()))
                return;

            const auto typeId   = static_cast<uint32_t>(data->type.underlying());
            const auto typeName = typeId < kTypeNames.size()
                                      ? std::string(kTypeNames[typeId])
                                      : "Unknown";

            const char* fullName = data->locationName.GetFullName();
            std::string name     = fullName ? fullName : "";

            if (visibleOnly && name.empty())
                return;

            const float x = form->GetPositionX();
            const float y = form->GetPositionY();

            nlohmann::json entry;
            entry["refId"]         = std::format("0x{:08X}", formId);
            entry["name"]          = name;
            entry["type"]          = typeName;
            entry["typeId"]        = typeId;
            entry["x"]             = x;
            entry["y"]             = y;
            entry["isVisible"]     = isVisible;
            entry["canFastTravel"] = canFastTravel;

            result.push_back(std::move(entry));
        };

        const auto& worlds = handler->GetFormArray<RE::TESWorldSpace>();

        // We avoid touching world->fixedPersistentRefMap / mobilePersistentRefs
        // directly: in multi-targeting builds the BSTHashMap layout mismatches
        // the header, which produces garbage iterators and crashes. Instead we
        // walk every worldspace's persistent cell using the public, safe API
        // TESObjectCELL::ForEachReference. Map markers live in the persistent
        // cell of each worldspace.
        std::size_t totalRefs = 0;
        for (auto* world : worlds) {
            if (!world)
                continue;
            auto* persist = world->persistentCell;
            if (!persist)
                continue;

            persist->ForEachReference([&](RE::TESObjectREFR& ref) {
                ++totalRefs;
                emit(&ref);
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }

        logger::debug("[Map::Markers::Locations] visibleOnly={} worlds={} refs_visited={} markers={}",
                      visibleOnly, worlds.size(), totalRefs, result.size());
        return result;
    }

    nlohmann::json ReadMapMarkers()
    {
        return ReadMapMarkersImpl(/*visibleOnly=*/true);
    }

    nlohmann::json ReadMapMarkersAll()
    {
        return ReadMapMarkersImpl(/*visibleOnly=*/false);
    }

    namespace
    {
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

        // Find the BGSRefAlias in `quest` whose aliasID matches the
        // TESQuestTarget::alias byte. Returns nullptr when nothing matches
        // (e.g. data alias, location alias, or unfilled).
        RE::BGSRefAlias* FindRefAlias(RE::TESQuest* quest, std::uint32_t aliasID)
        {
            if (!quest)
                return nullptr;
            for (auto* alias : quest->aliases) {
                if (!alias)
                    continue;
                if (alias->aliasID != aliasID)
                    continue;
                // Identify a ref alias by its scripting VM type tag rather
                // than RTTI so we don't depend on dynamic_cast working
                // across module boundaries (BGSRefAlias::VMTYPEID == 140).
                if (alias->GetVMTypeID() == RE::BGSRefAlias::VMTYPEID)
                    return static_cast<RE::BGSRefAlias*>(alias);
                return nullptr;
            }
            return nullptr;
        }

        std::string_view TrimAscii(std::string_view value)
        {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
                value.remove_prefix(1);
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
                value.remove_suffix(1);
            return value;
        }

        bool EqualAsciiIgnoreCase(std::string_view lhs, std::string_view rhs)
        {
            if (lhs.size() != rhs.size())
                return false;
            for (std::size_t i = 0; i < lhs.size(); ++i) {
                const auto l = static_cast<unsigned char>(lhs[i]);
                const auto r = static_cast<unsigned char>(rhs[i]);
                if (std::tolower(l) != std::tolower(r))
                    return false;
            }
            return true;
        }

        bool IsAliasTokenHead(std::string_view head)
        {
            head = TrimAscii(head);
            return head.size() >= 5
                && EqualAsciiIgnoreCase(head.substr(0, 5), "Alias")
                && (head.size() == 5 || head[5] == '.');
        }

        // Lookup an alias by its (case-insensitive) editor name on a quest.
        // Used to resolve <Alias=Foo> tokens in objective text.
        RE::BGSBaseAlias* FindAliasByName(RE::TESQuest* quest, std::string_view name)
        {
            name = TrimAscii(name);
            if (!quest || name.empty())
                return nullptr;
            for (auto* alias : quest->aliases) {
                if (!alias)
                    continue;
                const char* aname = alias->aliasName.c_str();
                if (!aname)
                    continue;
                if (EqualAsciiIgnoreCase(aname, name))
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

        // Best-effort display string for a quest alias. For radiant quests,
        // the static alias usually has only a template name (BanditCamp),
        // while the concrete value lives in TESQuest::instanceData as
        // BGSQuestInstanceText::StringData(aliasID -> fullNameFormID).
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

        // Replace every <Alias[.<sub>]=<Name>> token in `raw` with the
        // resolved alias name from `quest`. Tokens we can't resolve are
        // left in place verbatim so callers can still see what's missing.
        // We do not try to handle <Global=...>, <Spouse>, <Faction=...>,
        // etc. here - those would need additional context (textGlobals,
        // player relationships) and are out of scope for now.
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
                // Token without the angle brackets:
                //   "Alias=BanditCamp"  or  "Alias.ShortName=BanditLeader"
                std::string_view token(&raw[i + 1], end - i - 1);

                bool         matched = false;
                const auto eq = token.find('=');
                if (eq != std::string_view::npos) {
                    const auto head = TrimAscii(token.substr(0, eq));
                    const auto name = TrimAscii(token.substr(eq + 1));

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
                    // Leave the original "<...>" untouched.
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
    }  // namespace

    nlohmann::json ReadQuestMarkers()
    {
        nlohmann::json result = nlohmann::json::array();

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            logger::warn("[Map::Markers::Quests] no PlayerCharacter");
            return result;
        }

        const auto formIdStr = [](RE::FormID id) {
            return std::format("0x{:08X}", id);
        };

        // Dedup key: quest formId + objective index + alias id.
        std::unordered_set<std::uint64_t> seen;
        const auto makeKey = [](RE::FormID qid, std::uint16_t obj, std::uint32_t alias) {
            return (static_cast<std::uint64_t>(qid) << 32)
                 ^ (static_cast<std::uint64_t>(obj) << 16)
                 ^  static_cast<std::uint64_t>(alias);
        };

        // Build one JSON entry from a BGSQuestObjective + a TESQuestTarget +
        // resolved ref. Returns false when the target couldn't be resolved
        // and nothing was emitted.
        const auto emitTargetEntry = [&](RE::TESQuest*          quest,
                                         RE::BGSQuestObjective* objective,
                                         RE::TESQuestTarget*    target,
                                         std::uint32_t          instanceID) -> bool {
            if (!quest || !objective || !target)
                return false;

            // Skyrim's world map / compass only render quest-target arrows
            // for the *active* (tracked) quest in the journal. The kActive
            // flag is what the journal toggles when the player taps the
            // "Active" arrow next to a quest. Filtering by it gives us the
            // same visual set the player sees on the map.
            if (!quest->IsActive())
                return false;
            if (!quest->IsRunning() || quest->IsCompleted())
                return false;

            const std::uint32_t aliasID = target->alias;
            const auto          dedup  = makeKey(quest->GetFormID(), objective->index, aliasID);
            if (!seen.insert(dedup).second)
                return false;

            auto* refAlias = FindRefAlias(quest, aliasID);
            if (!refAlias)
                return false;

            auto* ref = refAlias->GetReference();
            if (!ref || ref->IsDeleted())
                return false;

            const char* questEditorIdC = quest->GetFormEditorID();
            const char* questNameC     = quest->GetFullName();
            const std::string questEditorId = questEditorIdC ? questEditorIdC : "";
            const std::string questName     = questNameC ? questNameC : "";
            const std::string objectiveText = objective->displayText.c_str()
                                                  ? objective->displayText.c_str()
                                                  : "";
            const std::string objectiveTextResolved =
                ResolveQuestObjectiveText(quest, objectiveText, instanceID);

            nlohmann::json entry;
            entry["questFormId"]    = formIdStr(quest->GetFormID());
            entry["questEditorId"]  = questEditorId;
            entry["questName"]      = questName;
            entry["questType"]      = std::string(QuestTypeName(quest->GetType()));
            entry["isActive"]       = true;
            entry["objectiveIndex"] = objective->index;
            entry["objectiveText"]  = objectiveText;
            entry["objectiveTextResolved"] = objectiveTextResolved;
            entry["aliasId"]        = aliasID;
            entry["refId"]          = formIdStr(ref->GetFormID());

            const char* refNameC = ref->GetDisplayFullName();
            if (!refNameC || !*refNameC)
                refNameC = ref->GetName();
            entry["name"] = refNameC ? std::string(refNameC) : std::string();

            entry["x"] = ref->GetPositionX();
            entry["y"] = ref->GetPositionY();
            entry["z"] = ref->GetPositionZ();

            if (auto* world = ref->GetWorldspace()) {
                const char* edid = world->GetFormEditorID();
                entry["worldspace"]       = edid ? std::string(edid) : std::string();
                entry["worldspaceFormId"] = formIdStr(world->GetFormID());

                auto* root = world;
                while (root->parentWorld)
                    root = root->parentWorld;
                const char* rootEdid = root->GetFormEditorID();
                entry["parentWorldspace"]       = rootEdid ? std::string(rootEdid) : std::string();
                entry["parentWorldspaceFormId"] = formIdStr(root->GetFormID());
            } else {
                entry["worldspace"]             = nullptr;
                entry["worldspaceFormId"]       = nullptr;
                entry["parentWorldspace"]       = nullptr;
                entry["parentWorldspaceFormId"] = nullptr;
            }

            if (auto* cell = ref->GetParentCell()) {
                const char* cedid = cell->GetFormEditorID();
                entry["cell"]       = cedid ? std::string(cedid) : std::string();
                entry["cellFormId"] = formIdStr(cell->GetFormID());
                entry["isInterior"] = cell->IsInteriorCell();
            } else {
                entry["cell"]       = nullptr;
                entry["cellFormId"] = nullptr;
                entry["isInterior"] = false;
            }

            logger::debug("[Map::Markers::Quests] emit quest='{}' (type={}, formId={}) obj#{} alias={} ref={} '{}'",
                          questEditorId, std::string(QuestTypeName(quest->GetType())),
                          formIdStr(quest->GetFormID()), objective->index, aliasID,
                          formIdStr(ref->GetFormID()), entry["name"].get<std::string>());

            result.push_back(std::move(entry));
            return true;
        };

        // We feed `result` from up to three sources and dedup by
        // (quest, objectiveIndex, aliasId):
        //
        //  1. PLAYER_RUNTIME_DATA::questTargets — a runtime BSTHashMap keyed
        //     by TESQuest*; the values are BSTArrays of TESQuestTarget* that
        //     the engine itself uses to draw compass arrows / quest-target
        //     icons. This is the source we trust most.
        //
        //  2. PLAYER_RUNTIME_DATA::objectives — a BSTArray of
        //     BGSInstancedQuestObjective. Tells us which objectives are in
        //     the kDisplayed runtime state for the current playthrough. We
        //     cross every displayed objective's `targets[]` through the
        //     dedup set so they land in the result if (1) missed them.
        //
        //  3. Static fallback: TESQuest::objectives walked with the
        //     non-instanced state field. Required for radiant / Misc quests
        //     and for VR (which has a different runtime data layout we
        //     don't translate yet).
        //
        // PLAYER_RUNTIME_DATA fields are not exposed as struct members in
        // multi-targeting builds; we resolve by absolute offsets.
        //   objectives    : SE 0x580, AE 1.6.629+ 0x588
        //   questTargets  : SE 0x598, AE 1.6.629+ 0x5A0

        std::size_t fromQuestTargets   = 0;
        std::size_t fromInstanced      = 0;
        std::size_t fromStaticFallback = 0;

        if (!REL::Module::IsVR()) {
            const auto base = reinterpret_cast<std::uintptr_t>(player);

            // ── (1) questTargets ─────────────────────────────────────────
            {
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

                        // We need an objective for the entry's
                        // index/displayText, but questTargets has only
                        // (quest, target) and target carries no objective
                        // back-pointer. Walk the quest's objectives and
                        // pick the one whose `targets[]` contains this
                        // target pointer. Cheap (objectives count is small
                        // per quest) and gives accurate metadata.
                        RE::BGSQuestObjective* matchedObj = nullptr;
                        for (auto* obj : quest->objectives) {
                            if (!obj || !obj->targets)
                                continue;
                            for (std::uint32_t i = 0; i < obj->numTargets; ++i) {
                                if (obj->targets[i] == target) {
                                    matchedObj = obj;
                                    break;
                                }
                            }
                            if (matchedObj)
                                break;
                        }
                        if (!matchedObj)
                            continue;

                        const auto instanceID = FindDisplayedObjectiveInstanceID(player, matchedObj);
                        if (emitTargetEntry(quest, matchedObj, target, instanceID))
                            ++fromQuestTargets;
                    }
                }
            }

            // ── (2) instanced objectives ─────────────────────────────────
            {
                const std::size_t off = REL::Module::IsAE() ? 0x588 : 0x580;
                const auto&       instances =
                    *reinterpret_cast<const RE::BSTArray<RE::BGSInstancedQuestObjective>*>(base + off);

                for (const auto& inst : instances) {
                    if (inst.InstanceState != RE::QUEST_OBJECTIVE_STATE::kDisplayed)
                        continue;

                    auto* objective = inst.Objective;
                    if (!objective)
                        continue;
                    auto* quest = objective->ownerQuest;
                    if (!quest)
                        continue;

                    const auto numTargets = objective->numTargets;
                    for (std::uint32_t i = 0; i < numTargets; ++i) {
                        auto* target = objective->targets ? objective->targets[i] : nullptr;
                        if (!target)
                            continue;
                        if (emitTargetEntry(quest, objective, target, inst.instanceID))
                            ++fromInstanced;
                    }
                }
            }
        }

        // ── (3) static-state fallback over every TESQuest ────────────────
        if (auto* handler = RE::TESDataHandler::GetSingleton()) {
            const auto& quests = handler->GetFormArray<RE::TESQuest>();
            for (auto* quest : quests) {
                if (!quest)
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

        logger::info("[Map::Markers::Quests] markers={} (questTargets={}, instanced={}, staticFallback={})",
                     result.size(), fromQuestTargets, fromInstanced, fromStaticFallback);
        return result;
    }

    nlohmann::json ReadGameStatus()
    {
        nlohmann::json out = {
            { "paused",           false },
            { "loading",          false },
            { "inMainMenu",       false },
            { "inDialogue",       false },
            { "inCombat",         false },
            { "dead",             false },
            { "controlsEnabled",  true  },
            { "canAct",           false },
        };

        bool paused     = false;
        bool loading    = false;
        bool inMainMenu = false;
        bool inDialogue = false;

        if (auto* ui = RE::UI::GetSingleton())
        {
            paused     = ui->GameIsPaused();
            // NOTE: official menu names contain a space:
            // LoadingMenu::MENU_NAME = "Loading Menu", MainMenu::MENU_NAME = "Main Menu",
            // DialogueMenu::MENU_NAME = "Dialogue Menu".
            loading    = ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
            inMainMenu = ui->IsMenuOpen(RE::MainMenu::MENU_NAME);
            inDialogue = ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);

            // Quick interior door transitions don't open the full LoadingMenu,
            // they fade through FaderMenu instead. Treat that as loading too.
            if (!loading && ui->IsMenuOpen("Fader Menu"))
                loading = true;
        }

        // MenuTopicManager.speaker / lastSpeaker remain set while a conversation
        // is active even after the dialogue menu has been torn down (e.g. the
        // NPC is still speaking the last line). Treat any valid speaker handle
        // as "in dialogue".
        if (auto* mtm = RE::MenuTopicManager::GetSingleton())
        {
            if (mtm->speaker || mtm->lastSpeaker)
                inDialogue = true;
        }

        bool inCombat        = false;
        bool dead            = false;
        bool inKillMove      = false;
        bool inFurniture     = false;
        bool inForcedAnim    = false;  // ragdoll, knock-down, sit/sleep, mount, getting on/off mount

        if (auto* player = RE::PlayerCharacter::GetSingleton())
        {
            inCombat = player->IsInCombat();

            // Use ActorState life-state so we catch the entire death sequence
            // (dying animation -> ragdoll -> "you died" load screen), not just
            // the final state.
            switch (player->AsActorState()->GetLifeState())
            {
            case RE::ACTOR_LIFE_STATE::kAlive:
                dead = false;
                break;
            default:
                // kDying, kDead, kUnconcious, kReanimate, kRecycle,
                // kRestrained, kEssentialDown, kBleedout
                dead = true;
                break;
            }

            inKillMove = player->IsInKillMove();

            // Crafting (forge, workbench, alchemy, enchanting, cooking),
            // sitting, sleeping, wait-menu — all set an occupied-furniture
            // reference on the actor.
            if (player->GetOccupiedFurniture())
                inFurniture = true;

            // Sit / sleep / mount transitions disable normal controls even
            // while no UI menu is open (animation in progress).
            const auto sitSleep = player->AsActorState()->GetSitSleepState();
            if (sitSleep != RE::SIT_SLEEP_STATE::kNormal)
                inForcedAnim = true;

            // Knockdown / stagger / ragdoll
            if (player->AsActorState()->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal)
                inForcedAnim = true;
        }

        // PlayerControls::blockPlayerInput is set in cinematics / scripted
        // sequences / cart intro / forced first-person scenes. Combined with
        // the actor-state checks above, this gives a reliable
        // "the engine has taken control away from the player" signal.
        bool inputBlocked = false;
        if (auto* pc = RE::PlayerControls::GetSingleton())
        {
            inputBlocked = pc->blockPlayerInput;
        }

        bool controlsEnabled =
            !inputBlocked && !inKillMove && !inFurniture && !inForcedAnim && !dead;

        out["paused"]          = paused;
        out["loading"]         = loading;
        out["inMainMenu"]      = inMainMenu;
        out["inDialogue"]      = inDialogue;
        out["inCombat"]        = inCombat;
        out["dead"]            = dead;
        out["controlsEnabled"] = controlsEnabled;
        out["canAct"]          = !paused && !loading && !inMainMenu &&
                                 !inDialogue && !dead && controlsEnabled;

        return out;
    }
}
