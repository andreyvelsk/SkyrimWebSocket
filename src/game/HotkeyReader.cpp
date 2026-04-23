#include "HotkeyReader.h"
#include "../Utils.h"

#include <format>

namespace HotkeyReader
{
    // ─── Private helpers ──────────────────────────────────────────────────

    // Ensures the hotkeys array has exactly 8 slots.
    static void EnsureHotkeySlots(RE::MagicFavorites* favorites)
    {
        if (!favorites)
            return;
        while (favorites->hotkeys.size() < 8)
            favorites->hotkeys.push_back(nullptr);
    }

    // Returns the form currently bound to a 0-based magic hotkey slot, or nullptr.
    static RE::TESForm* GetMagicHotkey(RE::MagicFavorites* favorites, std::uint8_t idx)
    {
        if (!favorites || idx >= favorites->hotkeys.size())
            return nullptr;
        return favorites->hotkeys[idx];
    }

    // Finds the inventory entry whose ExtraHotkey matches a 0-based slot index.
    // Returns nullptr when nothing is bound.
    static RE::InventoryEntryData* FindItemEntry(RE::PlayerCharacter* player, std::uint8_t idx)
    {
        if (!player)
            return nullptr;
        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges || !invChanges->entryList)
            return nullptr;

        for (auto* entry : *invChanges->entryList) {
            if (!entry || !entry->extraLists)
                continue;
            for (auto* xList : *entry->extraLists) {
                if (!xList)
                    continue;
                auto* xh = xList->GetByType<RE::ExtraHotkey>();
                if (xh && xh->hotkey.underlying() == idx)
                    return entry;
            }
        }
        return nullptr;
    }

    // Builds the JSON fields for a spell/shout/power slot entry.
    static nlohmann::json BuildSpellEntry(RE::SpellItem* spell, RE::PlayerCharacter* player)
    {
        nlohmann::json j;
        j["kind"]   = "spell";
        j["name"]   = spell->GetName();
        j["formId"] = std::format("0x{:08X}", spell->GetFormID());

        const char* spellType = "Spell";
        switch (spell->GetSpellType()) {
            case RE::MagicSystem::SpellType::kPower:       spellType = "Power";       break;
            case RE::MagicSystem::SpellType::kLesserPower: spellType = "LesserPower"; break;
            case RE::MagicSystem::SpellType::kVoicePower:  spellType = "VoicePower";  break;
            default:                                        spellType = "Spell";       break;
        }
        j["spellType"] = spellType;

        const char* school = "None";
        const auto av = spell->GetAssociatedSkill();
        if      (av == RE::ActorValue::kDestruction) school = "Destruction";
        else if (av == RE::ActorValue::kAlteration)  school = "Alteration";
        else if (av == RE::ActorValue::kConjuration) school = "Conjuration";
        else if (av == RE::ActorValue::kIllusion)    school = "Illusion";
        else if (av == RE::ActorValue::kRestoration) school = "Restoration";
        j["school"] = school;

        j["cost"] = static_cast<int32_t>(spell->CalculateMagickaCost(player));

        int32_t level = 0;
        if (const auto* eff = spell->GetCostliestEffectItem(); eff && eff->baseEffect)
            level = eff->baseEffect->GetMinimumSkillLevel();
        j["level"]      = level;
        j["chargeTime"] = spell->data.chargeTime;
        return j;
    }

    // Builds the JSON fields for an inventory item slot entry.
    static nlohmann::json BuildItemEntry(RE::InventoryEntryData* entry)
    {
        nlohmann::json j;
        j["kind"] = "item";
        if (!entry || !entry->object)
            return j;

        auto* item  = entry->object;
        j["name"]   = item->GetName();
        j["formId"] = std::format("0x{:08X}", item->GetFormID());

        const char* cat = "Unknown";
        switch (item->GetFormType()) {
            case RE::FormType::Weapon: {
                cat = "Weapon";
                if (const auto* weap = item->As<RE::TESObjectWEAP>()) {
                    const char* wt = "Unknown";
                    for (const auto& [type, name] : kWeaponTypeNames)
                        if (weap->GetWeaponType() == type) { wt = name; break; }
                    j["weaponType"] = wt;
                }
                break;
            }
            case RE::FormType::Armor: {
                cat = "Apparel";
                if (const auto* armo = item->As<RE::TESObjectARMO>()) {
                    const char* bs = "Unknown";
                    for (const auto& [slot, name] : kBodySlotNames) {
                        if (armo->HasPartOf(slot)) { bs = name; break; }
                    }
                    j["bodySlot"] = bs;
                }
                break;
            }
            case RE::FormType::Book:        cat = "Book";       break;
            case RE::FormType::AlchemyItem: {
                const auto* alch = item->As<RE::AlchemyItem>();
                cat = (alch && alch->IsFood()) ? "Food" : "Potion";
                break;
            }
            case RE::FormType::Ingredient:  cat = "Ingredient"; break;
            case RE::FormType::Misc:        cat = "Misc";       break;
            case RE::FormType::Ammo:        cat = "Ammo";       break;
            case RE::FormType::KeyMaster:   cat = "Key";        break;
            case RE::FormType::SoulGem:     cat = "SoulGem";    break;
            case RE::FormType::Scroll:      cat = "Scroll";     break;
            default: break;
        }
        j["categoryType"] = cat;

        j["count"]      = entry->countDelta;
        j["weight"]     = entry->GetWeight();
        j["value"]      = entry->GetValue();
        j["isFavorite"] = entry->IsFavorited();
        return j;
    }

    // ─── Public API ───────────────────────────────────────────────────────

    nlohmann::json ReadItems()
    {
        auto* player    = RE::PlayerCharacter::GetSingleton();
        auto* favorites = RE::MagicFavorites::GetSingleton();

        if (!player || !favorites)
            return nlohmann::json::array();

        EnsureHotkeySlots(favorites);

        nlohmann::json result = nlohmann::json::array();
        for (std::uint8_t i = 0; i < 8; ++i) {
            nlohmann::json slot;
            slot["slot"] = static_cast<int>(i + 1);

            // Magic binding takes precedence (same priority as FavoritesHandler).
            if (auto* form = GetMagicHotkey(favorites, i)) {
                if (auto* spell = form->As<RE::SpellItem>()) {
                    slot["bound"] = true;
                    slot.update(BuildSpellEntry(spell, player));
                    result.push_back(std::move(slot));
                    continue;
                }
            }

            // Inventory item binding.
            if (auto* entry = FindItemEntry(player, i)) {
                slot["bound"] = true;
                slot.update(BuildItemEntry(entry));
                result.push_back(std::move(slot));
                continue;
            }

            slot["bound"] = false;
            result.push_back(std::move(slot));
        }
        return result;
    }
}
