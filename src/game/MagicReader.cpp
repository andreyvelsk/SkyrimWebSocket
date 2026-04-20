#include "MagicReader.h"

#include <format>
#include <unordered_map>

namespace MagicReader
{
    // ─── School metadata ──────────────────────────────────────────────────

    struct SchoolInfo
    {
        std::string categoryId;
        const char* gmstKey;
    };

    // clang-format off
    static const std::unordered_map<RE::ActorValue, SchoolInfo> s_schools = {
        { RE::ActorValue::kDestruction, { "Destruction", "sSkillDestruction" } },
        { RE::ActorValue::kAlteration,  { "Alteration",  "sSkillAlteration"  } },
        { RE::ActorValue::kConjuration, { "Conjuration", "sSkillConjuration" } },
        { RE::ActorValue::kIllusion,    { "Illusion",    "sSkillIllusion"    } },
        { RE::ActorValue::kRestoration, { "Restoration", "sSkillRestoration" } },
        { RE::ActorValue::kEnchanting,  { "Enchanting",  "sSkillEnchanting"  } },
    };
    // clang-format on

    // ─── Private helpers ──────────────────────────────────────────────────

    // Looks up a GMST string by key.
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

    static void ReplaceAll(std::string& str, const std::string_view from, const std::string& to)
    {
        for (std::size_t pos = 0; (pos = str.find(from, pos)) != std::string::npos; pos += to.size())
            str.replace(pos, from.size(), to);
    }

    // Format a float: integer when no fractional part, otherwise one decimal place.
    // Matches vanilla Skyrim inventory display convention.
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

        // EffectSetting stores the localized description in magicItemDescription (DNAM).
        const auto& desc = eff->baseEffect->magicItemDescription;
        std::string tmpl = desc.empty() ? "" : std::string(desc.c_str());
        j["descriptionTemplate"] = tmpl;

        std::string resolved = tmpl;
        ReplaceAll(resolved, "<mag>", FormatMagnitude(eff->effectItem.magnitude));
        ReplaceAll(resolved, "<dur>", std::to_string(eff->effectItem.duration));
        j["description"] = std::move(resolved);

        return j;
    }

    static nlohmann::json BuildEffectsArray(const RE::MagicItem* magic)
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

    static const char* CastingTypeToString(RE::MagicSystem::CastingType type)
    {
        switch (type) {
            case RE::MagicSystem::CastingType::kConstantEffect: return "ConstantEffect";
            case RE::MagicSystem::CastingType::kFireAndForget:  return "FireAndForget";
            case RE::MagicSystem::CastingType::kConcentration:  return "Concentration";
            case RE::MagicSystem::CastingType::kScroll:         return "Scroll";
            default:                                            return "Unknown";
        }
    }

    static const char* DeliveryToString(RE::MagicSystem::Delivery delivery)
    {
        switch (delivery) {
            case RE::MagicSystem::Delivery::kSelf:           return "Self";
            case RE::MagicSystem::Delivery::kTouch:          return "Touch";
            case RE::MagicSystem::Delivery::kAimed:          return "Aimed";
            case RE::MagicSystem::Delivery::kTargetActor:    return "TargetActor";
            case RE::MagicSystem::Delivery::kTargetLocation: return "TargetLocation";
            default:                                         return "Unknown";
        }
    }

    // Returns which hands (if any) the spell is currently equipped for casting.
    // Checks selectedSpells[] in ACTOR_RUNTIME_DATA — the HUD-visible equipped slot,
    // not currentSpell which is only set while actively casting.
    // Returns nullptr, "left", "right", or "both".
    static nlohmann::json GetEquippedHand(RE::SpellItem* spell, RE::PlayerCharacter* player)
    {
        const auto& rt     = player->GetActorRuntimeData();
        const bool  inLeft  = rt.selectedSpells[RE::Actor::SlotTypes::kLeftHand]  == spell;
        const bool  inRight = rt.selectedSpells[RE::Actor::SlotTypes::kRightHand] == spell;

        if (inLeft && inRight) return "both";
        if (inRight)           return "right";
        if (inLeft)            return "left";
        return nullptr;
    }

    // Builds the JSON object for a single known spell.
    // Must be called on the game thread.
    static nlohmann::json BuildSpellEntry(
        RE::SpellItem*       spell,
        const std::string&   categoryType,
        RE::MagicFavorites*  favorites,
        RE::PlayerCharacter* player)
    {
        nlohmann::json j;
        j["name"]         = spell->GetName();
        j["formId"]       = std::format("0x{:08X}", spell->GetFormID());
        j["categoryType"] = categoryType;

        // cost: real in-game magicka cost with player skill/perk modifiers applied.
        j["cost"] = static_cast<int32_t>(spell->CalculateMagickaCost(player));

        // costValue: raw base cost — costOverride when explicitly set, otherwise
        // unmodified (no-actor) calculation.
        const int32_t costBase = (spell->data.costOverride >= 0)
                                     ? spell->data.costOverride
                                     : static_cast<int32_t>(spell->CalculateMagickaCost(nullptr));
        j["costValue"] = costBase;

        // level: minimum school skill required (0=Novice, 25=Apprentice, 50=Adept,
        // 75=Expert, 100=Master).  Taken from the costliest effect's base setting.
        int32_t level = 0;
        const auto* costliestEff = spell->GetCostliestEffectItem();
        if (costliestEff && costliestEff->baseEffect)
            level = costliestEff->baseEffect->GetMinimumSkillLevel();
        j["level"] = level;

        j["castingType"] = CastingTypeToString(spell->data.castingType);
        j["delivery"]    = DeliveryToString(spell->data.delivery);
        j["range"]       = spell->data.range;
        j["chargeTime"]  = spell->data.chargeTime;
        j["effects"]     = BuildEffectsArray(spell);

        // Equipped hand: which casting slot (if any) has this spell ready.
        auto hand       = GetEquippedHand(spell, player);
        j["isEquipped"] = !hand.is_null();
        j["equippedHand"] = std::move(hand);

        // isActive: currently being cast by the player.
        j["isActive"] = player->IsCasting(spell);

        // Hotkeys: collect all number-key slot indices (0-7) for this spell.
        nlohmann::json hotkeys = nlohmann::json::array();
        if (favorites) {
            for (int i = 0; i < 8; ++i) {
                if (favorites->hotkeys[i] == spell)
                    hotkeys.push_back(i);
            }
        }
        j["hotkeys"] = std::move(hotkeys);

        // isFavorite: is this spell marked as a favorite.
        bool isFavorite = false;
        if (favorites) {
            for (const auto* fav : favorites->spells) {
                if (fav == spell) {
                    isFavorite = true;
                    break;
                }
            }
        }
        j["isFavorite"] = isFavorite;

        return j;
    }

    // Generic per-school reader.
    // Iterates both the NPC base spell list and runtime addedSpells (learned via tomes).
    static nlohmann::json ReadSchool(RE::ActorValue school)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        const std::string& categoryType = s_schools.at(school).categoryId;
        auto*              favorites    = RE::MagicFavorites::GetSingleton();

        nlohmann::json result = nlohmann::json::array();

        auto tryAdd = [&](RE::SpellItem* spell) {
            if (!spell)
                return;
            // Only regular castable spells — skip powers, diseases, abilities, etc.
            if (spell->GetSpellType() != RE::MagicSystem::SpellType::kSpell)
                return;
            if (spell->GetAssociatedSkill() != school)
                return;
            result.push_back(BuildSpellEntry(spell, categoryType, favorites, player));
        };

        // 1) Spells baked into the player's base NPC form.
        auto* npc       = player->GetActorBase();
        auto* spellData = npc ? npc->GetSpellList() : nullptr;
        if (spellData) {
            for (std::uint32_t i = 0; i < spellData->numSpells; ++i)
                tryAdd(spellData->spells[i]);
        }

        // 2) Spells learned at runtime (spell tomes, AddSpell(), console, etc.).
        for (auto* spell : player->GetActorRuntimeData().addedSpells)
            tryAdd(spell);

        return result;
    }

    // ─── Public API ───────────────────────────────────────────────────────

    nlohmann::json ReadCategories()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        std::unordered_map<RE::ActorValue, int32_t> counts;

        auto tryCount = [&](RE::SpellItem* spell) {
            if (!spell)
                return;
            if (spell->GetSpellType() != RE::MagicSystem::SpellType::kSpell)
                return;
            const auto school = spell->GetAssociatedSkill();
            if (s_schools.find(school) == s_schools.end())
                return;
            ++counts[school];
        };

        // 1) Spells baked into the player's base NPC form.
        auto* npc       = player->GetActorBase();
        auto* spellData = npc ? npc->GetSpellList() : nullptr;
        if (spellData) {
            for (std::uint32_t i = 0; i < spellData->numSpells; ++i)
                tryCount(spellData->spells[i]);
        }

        // 2) Spells learned at runtime (spell tomes, AddSpell(), console, etc.).
        for (auto* spell : player->GetActorRuntimeData().addedSpells)
            tryCount(spell);

        nlohmann::json result = nlohmann::json::array();
        for (auto& [school, count] : counts) {
            const auto& info        = s_schools.at(school);
            std::string displayName = GetGMSTString(info.gmstKey);
            if (displayName.empty())
                displayName = info.categoryId;
            result.push_back({
                { "categoryId", info.categoryId },
                { "name",       displayName     },
                { "count",      count           },
            });
        }
        return result;
    }

    nlohmann::json ReadDestruction() { return ReadSchool(RE::ActorValue::kDestruction); }
    nlohmann::json ReadAlteration()  { return ReadSchool(RE::ActorValue::kAlteration);  }
    nlohmann::json ReadConjuration() { return ReadSchool(RE::ActorValue::kConjuration); }
    nlohmann::json ReadIllusion()    { return ReadSchool(RE::ActorValue::kIllusion);    }
    nlohmann::json ReadRestoration() { return ReadSchool(RE::ActorValue::kRestoration); }
    nlohmann::json ReadEnchanting()  { return ReadSchool(RE::ActorValue::kEnchanting);  }
}
