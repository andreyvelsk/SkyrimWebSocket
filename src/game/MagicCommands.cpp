#include "MagicCommands.h"
#include "Common.h"
#include "../Utils.h"

// Windows wingdi.h defines GetObject as GetObjectW/GetObjectA;
// conflict with RE::BGSDefaultObjectManager::GetObject.
#undef GetObject

#include <algorithm>

namespace logger = SKSE::log;

namespace MagicCommands
{
    // ─── Private helpers ───────────────────────────────────────────────────

    // Well-known Skyrim equip-slot FormIDs used as a fallback when
    // BGSDefaultObjectManager fails to resolve them.
    static constexpr RE::FormID kRightHandSlotID = 0x00013F42;
    static constexpr RE::FormID kLeftHandSlotID  = 0x00013F43;

    // Look up the BGSEquipSlot for a given hand name ("right" or "left").
    // Tries BGSDefaultObjectManager first, falls back to direct FormID lookup.
    // Returns nullptr only when everything fails.
    static const RE::BGSEquipSlot* GetHandSlot(const std::string& hand)
    {
        const bool left = (hand == "left");
        const auto defObj = left ? RE::DEFAULT_OBJECT::kLeftHandEquip
                                 : RE::DEFAULT_OBJECT::kRightHandEquip;

        auto* dom = RE::BGSDefaultObjectManager::GetSingleton();
        if (dom) {
            auto* slot = dom->GetObject<RE::BGSEquipSlot>(defObj);
            if (slot)
                return slot;
        }

        // Fallback: look up by well-known FormID.
        const RE::FormID id = left ? kLeftHandSlotID : kRightHandSlotID;
        return RE::TESForm::LookupByID<RE::BGSEquipSlot>(id);
    }

    // Returns true if a spell is a master-level dual-cast spell.
    // Master-level spells (requiring 100 skill) occupy both hands and cannot be single-handed.
    static bool IsMasterLevelSpell(RE::SpellItem* spell)
    {
        if (!spell)
            return false;

        const auto* costliestEff = spell->GetCostliestEffectItem();
        if (!costliestEff || !costliestEff->baseEffect)
            return false;

        // Master level = 100 skill requirement
        return costliestEff->baseEffect->GetMinimumSkillLevel() >= 100;
    }

    // ─── Commands ─────────────────────────────────────────────────────────

    CommandResult EquipSpell(RE::FormID formId, const std::string& hand)
    {
        logger::trace("EquipSpell enter: formId=0x{:08X} hand='{}'", formId, hand);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(formId);
        if (!spell)
            return {false, "Spell not found"};

        if (!Common::PlayerKnowsSpell(player, spell))
            return {false, "Spell not known by player"};

        auto* equipMgr = RE::ActorEquipManager::GetSingleton();
        if (!equipMgr)
            return {false, "Equipment manager not available"};

        const auto* slot = GetHandSlot(hand);
        equipMgr->EquipSpell(player, spell, slot);

        logger::debug("equip_spell 0x{:08X} ('{}') hand='{}'", formId, spell->GetName(), hand);
        PrintConsole("[WS] Equip spell " + std::string(spell->GetName()) + " \xe2\x86\x92 " + hand);
        return {true, ""};
    }

    CommandResult UnequipSpell(RE::FormID formId, const std::string& hand)
    {
        logger::trace("UnequipSpell enter: formId=0x{:08X} hand='{}'", formId, hand);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(formId);
        if (!spell)
            return {false, "Spell not found"};

        // Verify the spell is equipped in the requested hand via selectedSpells[],
        // which tracks the HUD-visible slot (not currentSpell — that's only set while casting).
        const int slotIdx = (hand == "left") ? RE::Actor::SlotTypes::kLeftHand
                                              : RE::Actor::SlotTypes::kRightHand;
        if (player->GetActorRuntimeData().selectedSpells[slotIdx] != spell)
            return {false, "Spell is not equipped in " + hand + " hand"};

        // DeselectSpell clears ALL slots where the spell appears. If the other hand
        // had a DIFFERENT spell, we need to restore it since DeselectSpell removes
        // it from everywhere. However, if the spell was in both hands (dual-cast):
        // - Master-level spells: cannot be single-handed, so don't restore
        // - Non-master spells: restore to the other hand if not a different spell
        const int otherSlotIdx = (slotIdx == RE::Actor::SlotTypes::kLeftHand)
                                     ? RE::Actor::SlotTypes::kRightHand
                                     : RE::Actor::SlotTypes::kLeftHand;
        auto* otherMagicItem = player->GetActorRuntimeData().selectedSpells[otherSlotIdx];
        const bool spellInBothHands = (otherMagicItem == spell);
        const bool isMasterSpell = IsMasterLevelSpell(spell);

        player->DeselectSpell(spell);

        // If spell was in both hands and it's NOT a master spell, restore it to the other hand
        if (spellInBothHands && !isMasterSpell) {
            auto* equipMgr = RE::ActorEquipManager::GetSingleton();
            if (equipMgr) {
                const std::string otherHand = (hand == "left") ? "right" : "left";
                equipMgr->EquipSpell(player, spell, GetHandSlot(otherHand));
            }
        }
        // If spell was in both hands and IS a master spell, it's now completely unequipped

        // If other hand had a DIFFERENT spell, restore it
        if (otherMagicItem && otherMagicItem != spell) {
            auto* otherSpell = otherMagicItem->As<RE::SpellItem>();
            if (otherSpell) {
                auto* equipMgr = RE::ActorEquipManager::GetSingleton();
                if (equipMgr) {
                    const std::string otherHand = (hand == "left") ? "right" : "left";
                    equipMgr->EquipSpell(player, otherSpell, GetHandSlot(otherHand));
                }
            }
        }

        logger::debug("unequip_spell 0x{:08X} ('{}') hand='{}' inBothHands={} isMaster={}",
                      formId, spell->GetName(), hand, spellInBothHands, isMasterSpell);
        PrintConsole("[WS] Unequip spell " + std::string(spell->GetName()) + " \xe2\x86\x90 " + hand);
        return {true, ""};
    }

    CommandResult FavoriteSpell(RE::FormID formId)
    {
        logger::trace("FavoriteSpell enter: formId=0x{:08X}", formId);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* targetSpell = RE::TESForm::LookupByID<RE::SpellItem>(formId);
        if (!targetSpell)
            return {false, "Spell not found"};

        if (!Common::PlayerKnowsSpell(player, targetSpell))
            return {false, "Spell not known by player"};

        auto* favorites = RE::MagicFavorites::GetSingleton();
        if (!favorites)
            return {false, "Magic favorites not available"};

        // Find the spell in favorites list to check if it's already favorited
        auto it = std::find(favorites->spells.begin(), favorites->spells.end(),
                            static_cast<RE::TESForm*>(targetSpell));

        const bool isFavorited = (it != favorites->spells.end());

        // Toggle favorite
        if (isFavorited) {
            favorites->spells.erase(it);
            PrintConsole("[WS] Unfavorite spell " + std::string(targetSpell->GetName()));
        } else {
            favorites->spells.push_back(targetSpell);
            PrintConsole("[WS] Favorite spell " + std::string(targetSpell->GetName()));
        }

        return {true, ""};
    }

    CommandResult EquipShout(RE::FormID formId)
    {
        logger::trace("EquipShout enter: formId=0x{:08X}", formId);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* shout = RE::TESForm::LookupByID<RE::TESShout>(formId);
        if (!shout)
            return {false, "Shout not found"};

        if (!player->HasShout(shout))
            return {false, "Shout not known by player"};

        // Directly write the voice slot — same approach as EquipPower.
        // The selectedPower field holds both TESShout* and SpellItem*;
        // the HUD refreshes on the next game tick.
        player->GetActorRuntimeData().selectedPower = shout;

        logger::debug("equip_shout 0x{:08X} ('{}')", formId, shout->GetName());
        PrintConsole("[WS] Equip shout " + std::string(shout->GetName()));
        return {true, ""};
    }

    CommandResult UnequipShout(RE::FormID formId)
    {
        logger::trace("UnequipShout enter: formId=0x{:08X}", formId);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* shout = RE::TESForm::LookupByID<RE::TESShout>(formId);
        if (!shout)
            return {false, "Shout not found"};

        auto* current = player->GetActorRuntimeData().selectedPower;
        if (!current || current != static_cast<RE::TESForm*>(shout))
            return {false, "Shout is not currently equipped"};

        player->GetActorRuntimeData().selectedPower = nullptr;

        logger::debug("unequip_shout 0x{:08X} ('{}')", formId, shout->GetName());
        PrintConsole("[WS] Unequip shout " + std::string(shout->GetName()));
        return {true, ""};
    }

    CommandResult EquipPower(RE::FormID formId)
    {
        logger::trace("EquipPower enter: formId=0x{:08X}", formId);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(formId);
        if (!spell)
            return {false, "Power not found"};

        const auto type = spell->GetSpellType();
        if (type != RE::MagicSystem::SpellType::kPower &&
            type != RE::MagicSystem::SpellType::kLesserPower)
            return {false, "Spell is not a power or lesser power"};

        if (!Common::PlayerKnowsSpell(player, spell))
            return {false, "Power not known by player"};

        // Powers/lesser powers share the voice slot with shouts.  There is no
        // Papyrus EquipPower equivalent for SpellItem-based powers, so we set
        // selectedPower directly — the HUD refreshes on the next game tick.
        player->GetActorRuntimeData().selectedPower = spell;

        logger::debug("equip_power 0x{:08X} ('{}')", formId, spell->GetName());
        PrintConsole("[WS] Equip power " + std::string(spell->GetName()));
        return {true, ""};
    }

    CommandResult FavoriteShout(RE::FormID formId)
    {
        logger::trace("FavoriteShout enter: formId=0x{:08X}", formId);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* shout = RE::TESForm::LookupByID<RE::TESShout>(formId);
        if (!shout)
            return {false, "Shout not found"};

        if (!player->HasShout(shout))
            return {false, "Shout not known by player"};

        auto* favorites = RE::MagicFavorites::GetSingleton();
        if (!favorites)
            return {false, "Magic favorites not available"};

        auto it = std::find(favorites->spells.begin(), favorites->spells.end(),
                            static_cast<RE::TESForm*>(shout));
        if (it != favorites->spells.end()) {
            favorites->spells.erase(it);
            logger::debug("favorite_shout 0x{:08X}: removed from favorites", formId);
            PrintConsole("[WS] Unfavorite shout " + std::string(shout->GetName()));
        } else {
            favorites->spells.push_back(static_cast<RE::TESForm*>(shout));
            logger::debug("favorite_shout 0x{:08X}: added to favorites", formId);
            PrintConsole("[WS] Favorite shout " + std::string(shout->GetName()));
        }
        return {true, ""};
    }
}