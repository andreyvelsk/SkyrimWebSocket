#include "MagicReader.h"

#include <format>
#include <unordered_map>

// Reuse JSON helpers consistent with InventoryReader
#include <nlohmann/json.hpp>

namespace MagicReader
{
    // Mapping ActorValue -> stable category id string
    static const std::unordered_map<RE::ActorValue, std::string> s_schoolNames = {
        { RE::ActorValue::kDestruction, "Destruction" },
        { RE::ActorValue::kAlteration,  "Alteration"  },
        { RE::ActorValue::kConjuration, "Conjuration" },
        { RE::ActorValue::kIllusion,    "Illusion"    },
        { RE::ActorValue::kRestoration, "Restoration" },
        { RE::ActorValue::kEnchanting,  "Enchanting"  },
    };

    static const std::unordered_map<RE::ActorValue, const char*> s_schoolGMSTKeys = {
        { RE::ActorValue::kDestruction, "sSkillDestruction" },
        { RE::ActorValue::kAlteration,  "sSkillAlteration"  },
        { RE::ActorValue::kConjuration, "sSkillConjuration" },
        { RE::ActorValue::kIllusion,    "sSkillIllusion"    },
        { RE::ActorValue::kRestoration, "sSkillRestoration" },
        { RE::ActorValue::kEnchanting,  "sSkillEnchanting"  },
    };

    // Small helpers copied/adapted from InventoryReader for consistent formatting
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

    static void ReplaceAll(std::string& str, const std::string_view from, const std::string& to)
    {
        for (std::size_t pos = 0; (pos = str.find(from, pos)) != std::string::npos; pos += to.size())
            str.replace(pos, from.size(), to);
    }

    static std::string FormatMagnitude(float v)
    {
        float intpart;
        if (std::modf(v, &intpart) == 0.f)
            return std::to_string(static_cast<int>(intpart));
        return std::format("{:.1f}", v);
    }

    static std::string CastingTypeToString(RE::MagicSystem::CastingType castType)
    {
        switch (castType) {
            case RE::MagicSystem::CastingType::kConstantEffect:
                return "ConstantEffect";
            case RE::MagicSystem::CastingType::kFireAndForget:
                return "FireAndForget";
            case RE::MagicSystem::CastingType::kConcentration:
                return "Concentration";
            case RE::MagicSystem::CastingType::kScroll:
                return "Scroll";
            default:
                return "Unknown";
        }
    }

    static std::string DeliveryToString(RE::MagicSystem::Delivery delivery)
    {
        switch (delivery) {
            case RE::MagicSystem::Delivery::kSelf: return "Self";
            case RE::MagicSystem::Delivery::kTouch: return "Touch";
            case RE::MagicSystem::Delivery::kAimed: return "Aimed";
            case RE::MagicSystem::Delivery::kTargetActor: return "TargetActor";
            case RE::MagicSystem::Delivery::kTargetLocation: return "TargetLocation";
            default: return "Unknown";
        }
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

        std::string resolved = tmpl;
        ReplaceAll(resolved, "<mag>", FormatMagnitude(eff->effectItem.magnitude));
        ReplaceAll(resolved, "<dur>", std::to_string(eff->effectItem.duration));
        j["description"] = std::move(resolved);

        return j;
    }

    static nlohmann::json BuildMagicEffectsArray(const RE::MagicItem* magic)
    {
        nlohmann::json effects = nlohmann::json::array();
        if (magic) {
            for (const auto* eff : magic->effects) {
                if (!eff || !eff->baseEffect)
                    continue;
                effects.push_back(BuildEffectJson(eff));
            }
        }
        return effects;
    }

    // Generic spell JSON builder (minimal, safe to call across runtimes).
    static nlohmann::json BuildSpellEntry(const RE::SpellItem* spell)
    {
        nlohmann::json j;
        if (!spell)
            return j;

        j["name"]       = spell->GetName();
        j["formId"]     = std::format("0x{:08X}", spell->GetFormID());

        // categoryType -> resolve by associated skill if available
        std::string categoryType = "Unknown";
        if (auto av = spell->GetAssociatedSkill()) {
            auto it = s_schoolNames.find(av);
            if (it != s_schoolNames.end())
                categoryType = it->second;
        }
        j["categoryType"] = categoryType;

        // Effects (reuse MagicItem effect representation)
        j["effects"] = BuildMagicEffectsArray(spell);

        // Attempt to populate additional spell metadata when available.
        // Cost resolution: left as 0 for now (engine-perk adjustments require further work).
        j["cost"] = 0;
        j["castingType"] = CastingTypeToString(spell->GetCastingType());
        j["delivery"]    = DeliveryToString(spell->GetDelivery());
        j["range"]       = spell->GetRange();
        j["chargeTime"]  = spell->GetChargeTime();

        j["isEquipped"] = false;
        j["equippedHand"] = nullptr;
        j["hotkeys"] = nlohmann::json::array();
        j["isActive"] = false;

        return j;
    }

    // Reads categories: counts spells per school and returns non-empty categories
    nlohmann::json ReadCategories()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto* spells = player->GetSpellList();
        if (!spells)
            return nlohmann::json::array();

        std::unordered_map<RE::ActorValue, int32_t> counts;
        for (std::uint32_t i = 0; i < spells->numSpells; ++i) {
            const auto* spell = spells->spells[i];
            if (!spell)
                continue;
            auto av = spell->GetAssociatedSkill();
            if (s_schoolNames.find(av) != s_schoolNames.end())
                counts[av]++;
        }

        nlohmann::json result = nlohmann::json::array();
        for (const auto& [av, name] : s_schoolNames) {
            auto it = counts.find(av);
            if (it == counts.end() || it->second == 0)
                continue;

            std::string displayName;
            auto gmstIt = s_schoolGMSTKeys.find(av);
            if (gmstIt != s_schoolGMSTKeys.end() && gmstIt->second[0] != '\0')
                displayName = GetGMSTString(gmstIt->second);
            if (displayName.empty())
                displayName = name;

            result.push_back({
                { "categoryId", name },
                { "name",       displayName },
                { "count",      it->second },
            });
        }
        return result;
    }

    // Generic reader for spells filtered by ActorValue school
    static nlohmann::json ReadSpellsForSchool(RE::ActorValue school)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto* spells = player->GetSpellList();
        if (!spells)
            return nlohmann::json::array();

        nlohmann::json result = nlohmann::json::array();
        for (std::uint32_t i = 0; i < spells->numSpells; ++i) {
            const auto* spell = spells->spells[i];
            if (!spell)
                continue;

            auto av = spell->GetAssociatedSkill();
            if (av != school)
                continue;

            auto j = BuildSpellEntry(spell);
            result.push_back(std::move(j));
        }
        return result;
    }

    nlohmann::json ReadDestruction()  { return ReadSpellsForSchool(RE::ActorValue::kDestruction); }
    nlohmann::json ReadAlteration()   { return ReadSpellsForSchool(RE::ActorValue::kAlteration); }
    nlohmann::json ReadConjuration()  { return ReadSpellsForSchool(RE::ActorValue::kConjuration); }
    nlohmann::json ReadIllusion()     { return ReadSpellsForSchool(RE::ActorValue::kIllusion); }
    nlohmann::json ReadRestoration()  { return ReadSpellsForSchool(RE::ActorValue::kRestoration); }
    nlohmann::json ReadEnchanting()   { return ReadSpellsForSchool(RE::ActorValue::kEnchanting); }
}
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
