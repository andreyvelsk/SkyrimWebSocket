#include "MagicReader.h"
#include "InventoryReader.h"

#include <array>
#include <format>
#include <unordered_map>

namespace MagicReader
{
    // ─── Magic School & Type Mappings ─────────────────────────────────────

    // Map categoryId string back to ActorValue school (for display name lookup).
    static RE::ActorValue IdToSchool(const std::string& catId)
    {
        if (catId == "Destruction")  return RE::ActorValue::kDestruction;
        if (catId == "Restoration")  return RE::ActorValue::kRestoration;
        if (catId == "Illusion")     return RE::ActorValue::kIllusion;
        if (catId == "Conjuration")  return RE::ActorValue::kConjuration;
        if (catId == "Alteration")   return RE::ActorValue::kAlteration;
        return RE::ActorValue::kHealth;  // Default fallback
    }

    // Map ActorValue school enum to stable string categoryId (always English).
    static std::string SchoolToId(RE::ActorValue school)
    {
        switch (school) {
            case RE::ActorValue::kDestruction:  return "Destruction";
            case RE::ActorValue::kRestoration:  return "Restoration";
            case RE::ActorValue::kIllusion:     return "Illusion";
            case RE::ActorValue::kConjuration:  return "Conjuration";
            case RE::ActorValue::kAlteration:   return "Alteration";
            default:                            return "Unknown";
        }
    }

    // Map ActorValue school to localized display name via GMST.
    // Falls back to SchoolToId result if GMST is not found.
    static std::string GetSchoolDisplayName(RE::ActorValue school)
    {
        const char* gmstKey = nullptr;
        switch (school) {
            case RE::ActorValue::kDestruction:  gmstKey = "sSkillDestruction";  break;
            case RE::ActorValue::kRestoration:  gmstKey = "sSkillRestoration";  break;
            case RE::ActorValue::kIllusion:     gmstKey = "sSkillIllusion";     break;
            case RE::ActorValue::kConjuration:  gmstKey = "sSkillConjuration";  break;
            case RE::ActorValue::kAlteration:   gmstKey = "sSkillAlteration";   break;
            default:                            return SchoolToId(school);
        }

        auto* gmst = RE::GameSettingCollection::GetSingleton();
        if (gmst) {
            auto* setting = gmst->GetSetting(gmstKey);
            if (setting) {
                const char* str = setting->GetString();
                if (str && str[0] != '\0')
                    return str;
            }
        }
        return SchoolToId(school);
    }

    // Map spell difficulty level (0-4) to stable string.
    static std::string DifficultyToString(std::uint16_t difficulty)
    {
        switch (difficulty) {
            case 0:  return "Novice";
            case 1:  return "Apprentice";
            case 2:  return "Adept";
            case 3:  return "Expert";
            case 4:  return "Master";
            default: return "Unknown";
        }
    }

    // ─── Effects Helper (from InventoryReader) ────────────────────────────

    // Helper functions from InventoryReader for building effect descriptions.
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

    // ─── Spell Detection ──────────────────────────────────────────────────

    // Check if a spell is currently equipped in a hand.
    static bool IsSpellEquipped(RE::PlayerCharacter* player, RE::SpellItem* spell, bool checkLeft = false)
    {
        if (!player || !spell)
            return false;

        if (checkLeft)
            return player->GetMagicItems(RE::MagicSystem::CastingSource::kLeftHand).contains(spell);
        else
            return player->GetMagicItems(RE::MagicSystem::CastingSource::kRightHand).contains(spell);
    }

    // Determine currently equipped hand for a spell.
    // Returns "right", "left", "both", or null.
    static nlohmann::json GetEquippedHand(RE::PlayerCharacter* player, RE::SpellItem* spell)
    {
        if (!player || !spell)
            return nullptr;

        bool right = IsSpellEquipped(player, spell, false);
        bool left  = IsSpellEquipped(player, spell, true);

        if (right && left)
            return "both";
        if (right)
            return "right";
        if (left)
            return "left";
        return nullptr;
    }

    // ─── Base Spell Entry Builder ─────────────────────────────────────────

    // Build a spell entry with common fields.
    static nlohmann::json BuildSpellEntry(RE::SpellItem* spell)
    {
        nlohmann::json j;
        if (!spell)
            return j;

        j["name"]           = spell->GetName();
        j["formId"]         = std::format("0x{:08X}", spell->GetFormID());
        j["categoryId"]     = SchoolToId(spell->GetSchool());
        j["categoryName"]   = GetSchoolDisplayName(spell->GetSchool());
        j["cost"]           = spell->GetMagickaCost();
        j["level"]          = DifficultyToString(spell->GetLeveledSpell() ? 
                                                 spell->GetLeveledSpell()->GetSpellLevel() : 
                                                 spell->GetSpellLevel());
        j["effects"]        = BuildMagicEffectsArray(spell->As<RE::MagicItem>());

        return j;
    }

    // Build a spell entry for equipped spells (with equip state).
    static nlohmann::json BuildEquippableSpellEntry(RE::PlayerCharacter* player, RE::SpellItem* spell)
    {
        auto j = BuildSpellEntry(spell);
        j["isEquipped"]   = !GetEquippedHand(player, spell).is_null();
        j["isTwoHanded"]  = true;  // Spells always occupy both hands
        j["equippedHand"] = GetEquippedHand(player, spell);
        return j;
    }

    // ─── Category Counting ────────────────────────────────────────────────

    nlohmann::json ReadCategories()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        // Collect all known spells/powers/abilities
        std::unordered_map<std::string, int32_t> categoryCounts;

        // Player spells (from GetSpells())
        for (auto& spell : player->GetSpells()) {
            if (!spell)
                continue;
            std::string catId = SchoolToId(spell->GetSchool());
            ++categoryCounts[catId];
        }

        // Powers, lesser powers, shouts, and diseases (from GetPowers())
        for (auto& power : player->GetPowers()) {
            if (!power)
                continue;
            std::string catId = SchoolToId(power->GetSchool());
            ++categoryCounts[catId];
        }

        // Build result
        nlohmann::json result = nlohmann::json::array();
        for (auto& [catId, count] : categoryCounts) {
            result.push_back({
                { "categoryId", catId },
                { "name",       GetSchoolDisplayName(IdToSchool(catId)) },
                { "count",      count },
            });
        }
        return result;
    }

    // ─── Player Spells ────────────────────────────────────────────────────

    nlohmann::json ReadPlayerSpells()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        nlohmann::json result = nlohmann::json::array();

        for (auto& spell : player->GetSpells()) {
            if (!spell || !IsPlayerSpell(spell))
                continue;

            auto j = BuildEquippableSpellEntry(player, spell);
            result.push_back(std::move(j));
        }

        return result;
    }

    // ─── Lesser Powers ────────────────────────────────────────────────────

    nlohmann::json ReadLesserPowers()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        nlohmann::json result = nlohmann::json::array();

        for (auto& power : player->GetPowers()) {
            if (!power || power->GetSpellType() != RE::SpellItem::SpellType::kLesserPower)
                continue;

            auto j = BuildSpellEntry(power);
            j["usesPerDay"]  = 1;  // Lesser powers have 1 use per day
            j["isEquipped"]  = false;  // Cannot equip lesser powers
            j["isTwoHanded"] = false;
            j["equippedHand"] = nullptr;
            result.push_back(std::move(j));
        }

        return result;
    }

    // ─── Powers & Abilities ───────────────────────────────────────────────

    nlohmann::json ReadPowers()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        nlohmann::json result = nlohmann::json::array();

        for (auto& power : player->GetPowers()) {
            if (!power || power->GetSpellType() != RE::SpellItem::SpellType::kPower)
                continue;

            auto j = BuildSpellEntry(power);
            j["usesPerDay"]  = 0;  // Powers have unlimited uses
            j["isEquipped"]  = false;
            j["isTwoHanded"] = false;
            j["equippedHand"] = nullptr;
            result.push_back(std::move(j));
        }

        return result;
    }

    // ─── Shouts ───────────────────────────────────────────────────────────

    nlohmann::json ReadShouts()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        nlohmann::json result = nlohmann::json::array();

        for (auto& shout : player->GetPowers()) {
            if (!shout || shout->GetSpellType() != RE::SpellItem::SpellType::kShout)
                continue;

            auto j = BuildSpellEntry(shout);
            j["usesPerDay"]  = 0;  // Shouts recharge after combat
            j["isEquipped"]  = false;
            j["isTwoHanded"] = false;
            j["equippedHand"] = nullptr;
            result.push_back(std::move(j));
        }

        return result;
    }

    // ─── Diseases ─────────────────────────────────────────────────────────

    nlohmann::json ReadDiseases()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        nlohmann::json result = nlohmann::json::array();

        for (auto& disease : player->GetPowers()) {
            if (!disease || disease->GetSpellType() != RE::SpellItem::SpellType::kDisease)
                continue;

            auto j = BuildSpellEntry(disease);
            j["isEquipped"]  = false;
            j["isTwoHanded"] = false;
            j["equippedHand"] = nullptr;
            result.push_back(std::move(j));
        }

        return result;
    }

    // ─── Generic Resolver ─────────────────────────────────────────────────

    static nlohmann::json ResolveSpells(RE::ActorValue school, 
                                        std::function<bool(RE::SpellItem*)> predicate)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        nlohmann::json result = nlohmann::json::array();

        // Check all known spells from GetSpells()
        auto allSpells = player->GetSpells();
        for (auto& spell : allSpells) {
            if (!spell || !predicate(spell))
                continue;
            // Optional school filtering: if school is not kHealth (dummy non-magic value),
            // filter by school. Otherwise, include all matching predicates.
            if (school != RE::ActorValue::kHealth && spell->GetSchool() != school)
                continue;
            result.push_back(BuildEquippableSpellEntry(player, spell));
        }

        return result;
    }

    std::function<nlohmann::json()> MakeSpellResolver(
        RE::ActorValue school,
        std::function<bool(RE::SpellItem*)> predicate)
    {
        return [school, predicate]() { return ResolveSpells(school, predicate); };
    }
}
