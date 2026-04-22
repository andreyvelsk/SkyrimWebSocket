#include "PlayerReader.h"

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
        return pos;
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

        auto& runtimeData   = player->GetPlayerRuntimeData();
        auto& markerHandles = runtimeData.currentMapMarkers;

        for (auto& handle : markerHandles) {
            auto ref = handle.get();
            if (!ref)
                continue;

            auto* extra = ref->extraList.GetByType<RE::ExtraMapMarker>();
            if (!extra || !extra->mapData)
                continue;

            auto* data = extra->mapData;

            using Flag = RE::MapMarkerData::Flag;
            const bool isVisible     = data->flags.any(Flag::kVisible);
            const bool canFastTravel = data->flags.any(Flag::kCanTravelTo);

            const auto   typeId   = static_cast<uint32_t>(data->type.underlying());
            const auto   typeName = typeId < kTypeNames.size()
                                        ? std::string(kTypeNames[typeId])
                                        : "Unknown";

            const char* fullName = data->locationName.GetFullName();
            std::string name     = fullName ? fullName : "";

            nlohmann::json entry;
            entry["refId"]        = std::format("{:#010X}", ref->GetFormID());
            entry["name"]         = name;
            entry["type"]         = typeName;
            entry["typeId"]       = typeId;
            entry["x"]            = ref->GetPositionX();
            entry["y"]            = ref->GetPositionY();
            entry["isVisible"]    = isVisible;
            entry["canFastTravel"] = canFastTravel;

            result.push_back(std::move(entry));
        }

        return result;
    }
}
