#include "MapMarkers.h"
#include "Common.h"
#include "PlayerPosition.h"

#include "../../logger.h"

#include <array>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace MapMarkers
{
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
            "DLC02ToSolstheim",   // 58
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

        // Step 1 — determine the player's root (top-level) worldspace.
        auto* playerWorld = PlayerPosition::ResolvePlayerWorldspace();
        if (!playerWorld) {
            logger::debug("[Map::Markers::Locations] cannot determine player's worldspace, returning empty");
            return result;
        }

        auto* rootWorld = Common::ResolveWorldspaceRoot(playerWorld);

        // Step 2 — collect all worldspaces in the root's hierarchy.
        std::vector<RE::TESWorldSpace*> hierarchyWorlds;
        hierarchyWorlds.push_back(rootWorld);

        if (auto* handler = RE::TESDataHandler::GetSingleton()) {
            for (auto* w : handler->GetFormArray<RE::TESWorldSpace>()) {
                if (!w || w == rootWorld)
                    continue;
                for (auto* ancestor = w->parentWorld; ancestor;
                     ancestor = ancestor->parentWorld) {
                    if (ancestor == rootWorld) {
                        hierarchyWorlds.push_back(w);
                        break;
                    }
                }
            }
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

            const auto typeId   = static_cast<uint32_t>(data->type.underlying());
            const auto typeName = typeId < kTypeNames.size()
                                      ? std::string(kTypeNames[typeId])
                                      : "Unknown";

            const bool isCity = (typeId == 1);

            using Flag = RE::MapMarkerData::Flag;
            const bool isVisible     = data->flags.any(Flag::kVisible);
            const bool canFastTravel = data->flags.any(Flag::kCanTravelTo);

            if (visibleOnly && !isVisible && !isCity)
                return;

            if (visibleOnly && (form->IsDisabled() || form->IsDeleted()) && !isCity)
                return;

            const char* fullName = data->locationName.GetFullName();
            std::string name     = fullName ? fullName : "";

            if (visibleOnly && name.empty() && !isCity)
                return;

            const float x = form->GetPositionX();
            const float y = form->GetPositionY();

            nlohmann::json entry;
            entry["refId"]         = Common::FormIdToString(formId);
            entry["name"]          = name;
            entry["type"]          = typeName;
            entry["typeId"]        = typeId;
            entry["x"]             = x;
            entry["y"]             = y;
            entry["isVisible"]     = isVisible;
            entry["canFastTravel"] = canFastTravel;

            result.push_back(std::move(entry));
        };

        // Step 3 — walk every worldspace in the hierarchy, collecting
        // map-marker refs from each persistent cell.
        std::size_t totalRefs = 0;
        for (auto* ws : hierarchyWorlds) {
            auto* persist = ws->persistentCell;
            if (!persist)
                continue;

            persist->ForEachReference([&](RE::TESObjectREFR* ref) {
                ++totalRefs;
                emit(ref);
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }

        logger::debug("[Map::Markers::Locations] visibleOnly={} rootWorld='{}' worlds_visited={} refs_visited={} markers={}",
                      visibleOnly,
                      rootWorld->GetFormEditorID() ? rootWorld->GetFormEditorID() : "?",
                      hierarchyWorlds.size(), totalRefs, result.size());
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
}