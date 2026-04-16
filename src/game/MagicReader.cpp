#include "MagicReader.h"

#include <array>
#include <format>
#include <unordered_map>

namespace MagicReader
{
    // ─── Helpers ──────────────────────────────────────────────────────────

    // Looks up a GameSetting string by key (e.g. "sSkillAlteration").
    // Returns an empty string when the key does not exist or is not a string setting.
    static std::string GetGMSTString(const char* key)
    {
        auto* gmst = RE::GameSettingCollection::GetSingleton();
        if (!gmst)
            return "";
        auto* setting = gmst->GetSetting(key);
        if (!setting)
            return "";
        const char* str = setting->GetString();
        return str ? str : "";
    }

    // Replace all occurrences of `from` with `to` inside `str`.
    static void ReplaceAll(std::string& str, const std::string_view from, const std::string& to)
    {
        for (std::size_t pos = 0; (pos = str.find(from, pos)) != std::string::npos; pos += to.size())
            str.replace(pos, from.size(), to);
    }

    // Format a float: show as integer when there is no fractional part, otherwise
    // keep one decimal place (matches vanilla inventory display convention).
    static std::string FormatMagnitude(float v)
    {
        float intpart;
        if (std::modf(v, &intpart) == 0.f)
            return std::to_string(static_cast<int>(intpart));
        return std::format("{:.1f}", v);
    }

    static nlohmann::json BuildEffectJson(const RE::Effect* eff)
    {
        nlohmann::json j;
        if (!eff || !eff->baseEffect) {
            j["name"]                = "";
            j["magnitude"]           = 0.f;
            j["duration"]            = 0u;
            j["descriptionTemplate"] = "";
            j["description"]         = "";
            return j;
        }
        j["name"]      = eff->baseEffect->GetName();
        j["magnitude"] = eff->effectItem.magnitude;
        j["duration"]  = eff->effectItem.duration;

        const auto& desc = eff->baseEffect->magicItemDescription;
        std::string tmpl = desc.empty() ? "" : std::string(desc.c_str());
        j["descriptionTemplate"] = tmpl;

        // Build the ready-to-display description by substituting <mag> and <dur>.
        std::string resolved = tmpl;
        ReplaceAll(resolved, "<mag>", FormatMagnitude(eff->effectItem.magnitude));
        ReplaceAll(resolved, "<dur>", std::to_string(eff->effectItem.duration));
        j["description"] = std::move(resolved);

        return j;
    }

    // Returns a JSON array of effect objects for a spell.
    static nlohmann::json BuildSpellEffectsArray(const RE::SpellItem* spell)
    {
        nlohmann::json effects = nlohmann::json::array();
        if (spell) {
            for (const auto* eff : spell->effects) {
                if (!eff || !eff->baseEffect)
                    continue;
                effects.push_back(BuildEffectJson(eff));
            }
        }
        return effects;
    }

    // Maps magic school to category ID
    static std::string MagicSchoolToCategory(RE::MagicSystem::SpellType spellType)
    {
        switch (spellType) {
            case RE::MagicSystem::SpellType::kAlteration:  return "Alteration";
            case RE::MagicSystem::SpellType::kConjuration: return "Conjuration";
            case RE::MagicSystem::SpellType::kDestruction: return "Destruction";
            case RE::MagicSystem::SpellType::kIllusion:    return "Illusion";
            case RE::MagicSystem::SpellType::kRestoration: return "Restoration";
            default:                                       return "";
        }
    }

    // Maps category to GMST key for localized name
    static const std::unordered_map<std::string, const char*> s_categoryGMSTKeys = {
        { "Alteration",  "sSkillAlteration"  },
        { "Conjuration", "sSkillConjuration" },
        { "Destruction", "sSkillDestruction" },
        { "Illusion",    "sSkillIllusion"    },
        { "Restoration", "sSkillRestoration" },
    };

    // Helper to check if player knows a spell
    static bool PlayerKnowsSpell(RE::PlayerCharacter* player, RE::SpellItem* spell)
    {
        if (!player || !spell)
            return false;

        // Access the actor's magic data to check for known spells
        // Cast to Actor to access runtime data

        auto& actorData = player->GetActorRuntimeData();
        if (!actorData.addedSpells)
            return false;

        // Check if spell is in the added spells list
        for (auto* knownSpell : *actorData.addedSpells) {
            if (knownSpell && knownSpell->GetFormID() == spell->GetFormID()) {
                return true;
            }
        }
        
        return false;
    }

    // Determines which hand a spell is currently equipped in
    static nlohmann::json GetSpellEquippedHand(const RE::SpellItem* spell, RE::PlayerCharacter* player)
    {
        if (!spell || !player)
            return nullptr;

        // Access selected spells through runtime data

        auto& actorData = player->GetActorRuntimeData();
        
        auto* leftSpell = actorData.selectedSpells[RE::PlayerCharacter::SelectedSpells::kLeftHand];
        auto* rightSpell = actorData.selectedSpells[RE::PlayerCharacter::SelectedSpells::kRightHand];

        bool isLeft = (leftSpell && leftSpell->GetFormID() == spell->GetFormID());
        bool isRight = (rightSpell && rightSpell->GetFormID() == spell->GetFormID());

        if (isLeft && isRight)
            return "both";
        if (isLeft)
            return "left";
        if (isRight)
            return "right";
        return nullptr;
    }

    // TODO: Implement two-handed spell detection
    // In vanilla Skyrim, two-handed spells are rare. Future enhancement could
    // detect spells that require both hands by checking spell flags or equip slots.
    // For now, all spells are treated as one-handed.
    static bool IsSpellTwoHanded(const RE::SpellItem* /* spell */)
    {
        return false;
    }

    // Get spell level as string
    static std::string GetSpellLevel(const RE::SpellItem* spell)
    {
        if (!spell)
            return "Novice";

        // Spell level is determined by minimum skill level required
        // This is stored in the spell data
        auto minSkill = spell->GetMinimumSkillLevel();
        
        if (minSkill >= 75)
            return "Master";
        else if (minSkill >= 50)
            return "Expert";
        else if (minSkill >= 25)
            return "Adept";
        else if (minSkill > 0)
            return "Apprentice";
        else
            return "Novice";
    }

    // Builds a JSON object for a single spell
    static nlohmann::json BuildSpellEntry(const RE::SpellItem* spell, RE::PlayerCharacter* player)
    {
        if (!spell)
            return nlohmann::json();

        nlohmann::json j;
        j["name"]         = spell->GetName();
        j["formId"]       = std::format("0x{:08X}", spell->GetFormID());
        j["cost"]         = static_cast<int>(spell->CalculateMagickaCost(player));
        j["level"]        = GetSpellLevel(spell);
        j["categoryType"] = MagicSchoolToCategory(spell->GetSpellType());
        
        bool isTwoHanded = IsSpellTwoHanded(spell);
        j["isTwoHanded"]  = isTwoHanded;
        j["equippedHand"] = GetSpellEquippedHand(spell, player);
        j["isEquipped"]   = !j["equippedHand"].is_null();
        j["effects"]      = BuildSpellEffectsArray(spell);

        return j;
    }

    // Helper to read spells of a specific type
    static nlohmann::json ReadSpellsByType(RE::MagicSystem::SpellType spellType)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        nlohmann::json result = nlohmann::json::array();

        // Get all spells from the data handler
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler)
            return result;

        // Iterate through all spells and check if player knows them
        auto& spells = dataHandler->GetFormArray<RE::SpellItem>();
        for (auto* spell : spells) {
            if (!spell)
                continue;

            // Check if player knows this spell
            if (!PlayerKnowsSpell(player, spell))
                continue;

            // Skip if not the right type
            if (spell->GetSpellType() != spellType)
                continue;

            // Skip powers and abilities (we want castable spells only)
            auto castingType = spell->GetCastingType();
            if (castingType == RE::MagicSystem::CastingType::kConstantEffect)
                continue;

            result.push_back(BuildSpellEntry(spell, player));
        }

        return result;
    }

    // ─── ReadCategories ───────────────────────────────────────────────────

    nlohmann::json ReadCategories()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler)
            return nlohmann::json::array();

        // Count spells by category
        std::unordered_map<std::string, int32_t> categoryCounts;

        // Iterate through all spells and check if player knows them
        auto& spells = dataHandler->GetFormArray<RE::SpellItem>();
        for (auto* spell : spells) {
            if (!spell)
                continue;

            // Check if player knows this spell
            if (!PlayerKnowsSpell(player, spell))
                continue;

            // Skip powers and abilities
            auto castingType = spell->GetCastingType();
            if (castingType == RE::MagicSystem::CastingType::kConstantEffect)
                continue;

            auto category = MagicSchoolToCategory(spell->GetSpellType());
            if (!category.empty())
                categoryCounts[category]++;
        }

        // Build result array
        nlohmann::json result = nlohmann::json::array();
        for (auto& [catId, count] : categoryCounts) {
            // Get localized name from GMST
            std::string displayName;
            auto gmstIt = s_categoryGMSTKeys.find(catId);
            if (gmstIt != s_categoryGMSTKeys.end())
                displayName = GetGMSTString(gmstIt->second);
            if (displayName.empty())
                displayName = catId;

            result.push_back({
                { "categoryId", catId       },
                { "name",       displayName },
                { "count",      count       },
            });
        }

        return result;
    }

    // ─── Category-specific readers ────────────────────────────────────────

    nlohmann::json ReadAlteration()
    {
        return ReadSpellsByType(RE::MagicSystem::SpellType::kAlteration);
    }

    nlohmann::json ReadConjuration()
    {
        return ReadSpellsByType(RE::MagicSystem::SpellType::kConjuration);
    }

    nlohmann::json ReadDestruction()
    {
        return ReadSpellsByType(RE::MagicSystem::SpellType::kDestruction);
    }

    nlohmann::json ReadIllusion()
    {
        return ReadSpellsByType(RE::MagicSystem::SpellType::kIllusion);
    }

    nlohmann::json ReadRestoration()
    {
        return ReadSpellsByType(RE::MagicSystem::SpellType::kRestoration);
    }
}
