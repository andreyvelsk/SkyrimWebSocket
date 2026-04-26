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

    nlohmann::json ReadGameStatus()
    {
        nlohmann::json out = {
            { "paused",           false },
            { "loading",          false },
            { "inMainMenu",       false },
            { "inDialogue",       false },
            { "inCombat",         false },
            { "controlsEnabled",  true  },
            { "canAct",           false },
        };

        auto* ui = RE::UI::GetSingleton();
        if (ui)
        {
            out["paused"]     = ui->GameIsPaused();
            out["loading"]    = ui->IsMenuOpen("LoadingMenu"sv);
            out["inMainMenu"] = ui->IsMenuOpen("Main Menu"sv);
            // UI::IsInDialogue covers the dialogue menu being on the stack
            out["inDialogue"] = ui->IsMenuOpen("Dialogue Menu"sv);
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (player)
        {
            // Player flag is the most reliable signal for an active conversation
            // (covers cases where the dialogue menu is being torn down but the
            // engine still considers the player busy).
            if (player->IsInDialogue())
                out["inDialogue"] = true;
            out["inCombat"] = player->IsInCombat();
        }

        bool controlsEnabled = true;
        if (auto* cm = RE::ControlMap::GetSingleton())
        {
            // Treat controls as "enabled" only when the player can both move and
            // fight. This matches what callers usually mean by "can act":
            // cinematics / forced sequences disable movement, menus disable
            // fighting, etc.
            controlsEnabled = cm->IsMovementControlsEnabled() &&
                              cm->IsFightingControlsEnabled();
        }
        out["controlsEnabled"] = controlsEnabled;

        out["canAct"] = !out["paused"].get<bool>() &&
                        !out["loading"].get<bool>() &&
                        !out["inMainMenu"].get<bool>() &&
                        !out["inDialogue"].get<bool>() &&
                        controlsEnabled;

        return out;
    }
}
