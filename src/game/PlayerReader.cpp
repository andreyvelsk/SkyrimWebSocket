#include "PlayerReader.h"

#include "../../logger.h"

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

    nlohmann::json ReadMapMarkers()
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
        // walk its fixedPersistentRefMap + mobilePersistentRefs, looking for
        // refs that carry an ExtraMapMarker.
        logger::info("[Map::Markers] start");

        auto* handler = RE::TESDataHandler::GetSingleton();
        if (!handler) {
            logger::info("[Map::Markers] no TESDataHandler");
            return result;
        }
        logger::info("[Map::Markers] handler ok");

        std::unordered_set<RE::FormID> seen;

        auto emit = [&](RE::TESObjectREFR* form) {
            if (!form) {
                logger::info("[Map::Markers]   emit: form=null, skip");
                return;
            }
            const auto formId = form->GetFormID();
            logger::info("[Map::Markers]   emit: form=0x{:08X}", formId);

            logger::info("[Map::Markers]     - get extraList ExtraMapMarker");
            auto* extra = form->extraList.GetByType<RE::ExtraMapMarker>();
            if (!extra) {
                logger::info("[Map::Markers]     -> no ExtraMapMarker, skip");
                return;
            }
            logger::info("[Map::Markers]     - extra ok, mapData={}",
                         static_cast<const void*>(extra->mapData));
            if (!extra->mapData) {
                logger::info("[Map::Markers]     -> mapData null, skip");
                return;
            }

            if (!seen.insert(formId).second) {
                logger::info("[Map::Markers]     -> dup, skip");
                return;
            }

            auto* data = extra->mapData;

            using Flag = RE::MapMarkerData::Flag;
            logger::info("[Map::Markers]     - read flags");
            const bool isVisible     = data->flags.any(Flag::kVisible);
            const bool canFastTravel = data->flags.any(Flag::kCanTravelTo);

            logger::info("[Map::Markers]     - read type");
            const auto typeId   = static_cast<uint32_t>(data->type.underlying());
            const auto typeName = typeId < kTypeNames.size()
                                      ? std::string(kTypeNames[typeId])
                                      : "Unknown";

            logger::info("[Map::Markers]     - read locationName.GetFullName");
            const char* fullName = data->locationName.GetFullName();
            std::string name     = fullName ? fullName : "";
            logger::info("[Map::Markers]     - name='{}' typeId={} vis={} ft={}",
                         name, typeId, isVisible, canFastTravel);

            logger::info("[Map::Markers]     - read position");
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
            logger::info("[Map::Markers]     -> pushed");
        };

        logger::info("[Map::Markers] get worldspace form array");
        const auto& worlds = handler->GetFormArray<RE::TESWorldSpace>();
        logger::info("[Map::Markers] worldspace count = {}", worlds.size());

        std::size_t worldIdx = 0;
        for (auto* world : worlds) {
            const auto idx = worldIdx++;
            if (!world) {
                logger::info("[Map::Markers] world #{}: null, skip", idx);
                continue;
            }
            const auto wid = world->GetFormID();
            const char* wedid = world->GetFormEditorID();
            logger::info("[Map::Markers] world #{}: 0x{:08X} edid='{}'",
                         idx, wid, wedid ? wedid : "(null)");

            logger::info("[Map::Markers]   - access fixedPersistentRefMap");
            try {
                const auto& fixedMap = world->fixedPersistentRefMap;
                logger::info("[Map::Markers]   - fixedPersistentRefMap.size={}",
                             fixedMap.size());

                std::size_t bucketIdx = 0;
                for (auto& kv : fixedMap) {
                    const auto bi = bucketIdx++;
                    auto& refs = kv.second;
                    logger::info("[Map::Markers]   - fixed bucket #{} key=0x{:08X} refs.size={}",
                                 bi, kv.first, refs.size());
                    std::size_t ri = 0;
                    for (auto& ref : refs) {
                        logger::info("[Map::Markers]     - ref #{} ptr={}",
                                     ri++, static_cast<const void*>(ref.get()));
                        emit(ref.get());
                    }
                }
            } catch (const std::exception& e) {
                logger::info("[Map::Markers]   ! exception in fixed: {}", e.what());
            } catch (...) {
                logger::info("[Map::Markers]   ! unknown exception in fixed");
            }

            logger::info("[Map::Markers]   - access mobilePersistentRefs");
            try {
                auto& mobile = world->mobilePersistentRefs;
                logger::info("[Map::Markers]   - mobilePersistentRefs.size={}",
                             mobile.size());
                std::size_t mi = 0;
                for (auto& ref : mobile) {
                    logger::info("[Map::Markers]     - mobile ref #{} ptr={}",
                                 mi++, static_cast<const void*>(ref.get()));
                    emit(ref.get());
                }
            } catch (const std::exception& e) {
                logger::info("[Map::Markers]   ! exception in mobile: {}", e.what());
            } catch (...) {
                logger::info("[Map::Markers]   ! unknown exception in mobile");
            }

            logger::info("[Map::Markers] world #{} done", idx);
        }

        logger::info("[Map::Markers] finished, total markers = {}", result.size());
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
