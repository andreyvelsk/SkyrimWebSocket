#include "GameWriter.h"
#include "../Utils.h"

#include <algorithm>

namespace logger = SKSE::log;

namespace GameWriter
{
    // ─── Helpers ──────────────────────────────────────────────────────────

    // Returns true for consumable form types that the "use" command accepts.
    static bool IsConsumable(RE::FormType ft)
    {
        return ft == RE::FormType::AlchemyItem || ft == RE::FormType::Ingredient || ft == RE::FormType::Scroll;
    }

    // Returns true for form types that can be equipped via the "equip" command.
    static bool IsEquippable(RE::FormType ft)
    {
        return ft == RE::FormType::Weapon || ft == RE::FormType::Armor || ft == RE::FormType::Ammo;
    }

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

    // Finds the live InventoryEntryData for a given formId from the player's
    // InventoryChanges.  Returns nullptr if not found.
    static RE::InventoryEntryData* FindLiveEntry(RE::PlayerCharacter* player, RE::FormID formId)
    {
        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges || !invChanges->entryList)
            return nullptr;
        for (auto* entry : *invChanges->entryList) {
            if (entry && entry->object && entry->object->GetFormID() == formId)
                return entry;
        }
        return nullptr;
    }

    // Returns the ExtraDataList that carries the kWorn or kWornLeft flag
    // for unequipping from a specific hand.
    static RE::ExtraDataList* FindWornExtraDataList(RE::InventoryEntryData* entry, bool leftHand)
    {
        if (!entry || !entry->extraLists)
            return nullptr;
        auto type = leftHand ? RE::ExtraDataType::kWornLeft : RE::ExtraDataType::kWorn;
        for (auto* xList : *entry->extraLists) {
            if (xList && xList->HasType(type))
                return xList;
        }
        return nullptr;
    }

    // Verifies that the item is in the player's inventory and returns
    // the count.  Returns 0 when not found.
    static int32_t GetInventoryCount(RE::PlayerCharacter* player, RE::FormID formId)
    {
        auto inv = player->GetInventory([formId](RE::TESBoundObject& obj) { return obj.GetFormID() == formId; });
        if (inv.empty())
            return 0;
        return inv.begin()->second.first;
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

    // Returns the first ExtraDataList that is NOT worn in either hand.
    // Used to obtain a "clean" xList to pass to EquipObject.
    static RE::ExtraDataList* FindUnwornExtraDataList(RE::InventoryEntryData* entry)
    {
        if (!entry || !entry->extraLists)
            return nullptr;
        for (auto* xList : *entry->extraLists) {
            if (!xList)
                continue;
            if (!xList->HasType(RE::ExtraDataType::kWorn) &&
                !xList->HasType(RE::ExtraDataType::kWornLeft))
                return xList;
        }
        return nullptr;
    }

    // Unequips a weapon from a specific hand.  Used internally before
    // equipping to the opposite hand (hand swap).
    static void DoUnequipWeapon(RE::ActorEquipManager* equipMgr,
                                RE::PlayerCharacter*   player,
                                RE::TESBoundObject*    form,
                                RE::ExtraDataList*     xList,
                                bool                   fromLeftHand)
    {
        const auto* slot = GetHandSlot(fromLeftHand ? "left" : "right");
        equipMgr->UnequipObject(player, form, xList, 1, slot,
                                /*a_queueEquip=*/true,
                                /*a_forceEquip=*/false,
                                /*a_playSounds=*/true,
                                /*a_applyNow=*/false);
    }

    // ─── Commands ─────────────────────────────────────────────────────────

    CommandResult EquipItem(RE::FormID formId, const std::string& hand)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!form)
            return {false, "Form not found"};

        const int32_t itemCount = GetInventoryCount(player, formId);
        if (itemCount <= 0)
            return {false, "Item not in inventory"};

        const auto ft = form->GetFormType();
        if (!IsEquippable(ft))
            return {false, "Item is not equippable (use 'use' for consumables)"};

        auto* equipMgr = RE::ActorEquipManager::GetSingleton();
        if (!equipMgr)
            return {false, "Equipment manager not available"};

        const RE::BGSEquipSlot* slot  = nullptr;
        RE::ExtraDataList*      xData = nullptr;

        if (ft == RE::FormType::Weapon) {
            const auto* weap = form->As<RE::TESObjectWEAP>();
            if (weap && IsWeaponTwoHanded(weap->GetWeaponType()) && hand == "left")
                return {false, "Two-handed weapon can only be equipped in the right hand"};

            slot = GetHandSlot(hand);

            const bool leftHand = (hand == "left");

            auto* liveEntry = FindLiveEntry(player, formId);
            if (liveEntry) {
                RE::ExtraDataList* wornRight = FindWornExtraDataList(liveEntry, false);
                RE::ExtraDataList* wornLeft  = FindWornExtraDataList(liveEntry, true);

                const bool inTarget = leftHand ? (wornLeft != nullptr)
                                               : (wornRight != nullptr);
                const bool inOther  = leftHand ? (wornRight != nullptr)
                                               : (wornLeft != nullptr);

                if (inTarget)
                    return {true, ""};

                if (inOther && itemCount < 2) {
                    RE::ExtraDataList* otherXList = leftHand ? wornRight : wornLeft;
                    DoUnequipWeapon(equipMgr, player, form, otherXList, !leftHand);
                }

                xData = FindUnwornExtraDataList(liveEntry);
            }
        }
        // For armor and ammo: slot = nullptr, xData = nullptr → engine auto-selects.

        equipMgr->EquipObject(player, form, xData, 1, slot,
                              /*a_queueEquip=*/true,
                              /*a_forceEquip=*/false,
                              /*a_playSounds=*/true,
                              /*a_applyNow=*/false);
        logger::debug("equip 0x{:08X} ('{}') hand='{}'  type={}",
                      formId, form->GetName(), hand, static_cast<int>(ft));        PrintConsole("[WS] Equip " + std::string(form->GetName()) + (slot ? " → " + hand : ""));
        return {true, ""};
    }

    CommandResult UnequipItem(RE::FormID formId, const std::string& hand)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!form)
            return {false, "Form not found"};

        if (GetInventoryCount(player, formId) <= 0)
            return {false, "Item not in inventory"};

        auto* liveEntry = FindLiveEntry(player, formId);
        if (!liveEntry || !liveEntry->IsWorn())
            return {false, "Item is not equipped"};

        auto* equipMgr = RE::ActorEquipManager::GetSingleton();
        if (!equipMgr)
            return {false, "Equipment manager not available"};

        const auto ft = form->GetFormType();

        if (ft == RE::FormType::Weapon) {
            const bool leftHand = (hand == "left");

            RE::ExtraDataList* wornRight = FindWornExtraDataList(liveEntry, false);
            RE::ExtraDataList* wornLeft  = FindWornExtraDataList(liveEntry, true);

            bool doRight = !leftHand && wornRight;
            bool doLeft  = leftHand && wornLeft;

            if (!doRight && !doLeft) {
                doRight = wornRight != nullptr;
                doLeft  = wornLeft != nullptr;
            }

            if (!doRight && !doLeft)
                return {false, "Weapon not found in any hand"};

            if (doRight && wornRight)
                DoUnequipWeapon(equipMgr, player, form, wornRight, false);
            if (doLeft && wornLeft)
                DoUnequipWeapon(equipMgr, player, form, wornLeft, true);
        } else {
            equipMgr->UnequipObject(player, form);
        }

        logger::debug("unequip 0x{:08X} ('{}') hand='{}'", formId, form->GetName(), hand);
        PrintConsole("[WS] Unequip " + std::string(form->GetName()));
        return {true, ""};
    }

    CommandResult UseItem(RE::FormID formId)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!form)
            return {false, "Form not found"};

        if (GetInventoryCount(player, formId) <= 0)
            return {false, "Item not in inventory"};

        if (!IsConsumable(form->GetFormType()))
            return {false, "Item is not consumable (use 'equip' for weapons/apparel)"};

        auto* equipMgr = RE::ActorEquipManager::GetSingleton();
        if (!equipMgr)
            return {false, "Equipment manager not available"};

        // EquipObject on consumables triggers consumption (potions, food,
        // ingredients) or equips for casting (scrolls).
        equipMgr->EquipObject(player, form);

        PrintConsole("[WS] Use " + std::string(form->GetName()));
        return {true, ""};
    }

    CommandResult DropItem(RE::FormID formId, int count)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!form)
            return {false, "Form not found"};

        if (count <= 0)
            return {false, "Count must be positive"};

        const int32_t have = GetInventoryCount(player, formId);
        if (have <= 0)
            return {false, "Item not in inventory"};
        if (count > have)
            return {false, "Not enough items (have " + std::to_string(have) + ")"};

        player->RemoveItem(form, count, RE::ITEM_REMOVE_REASON::kDropping, nullptr, nullptr);

        PrintConsole("[WS] Drop " + std::to_string(count) + "x " + std::string(form->GetName()));
        return {true, ""};
    }

    CommandResult FavoriteItem(RE::FormID formId)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!form)
            return {false, "Form not found"};

        const int32_t count = GetInventoryCount(player, formId);
        logger::debug("favorite 0x{:08X} ('{}') invCount={}", formId, form->GetName(), count);

        if (count <= 0)
            return {false, "Item not in inventory"};

        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges)
            return {false, "Inventory changes not available"};

        auto* liveEntry = FindLiveEntry(player, formId);
        if (!liveEntry) {
            // Item is in inventory but has no InventoryEntryData yet.
            // This happens with base-NPC inventory items that haven't been modified
            // (equipped, repaired, renamed, etc.) since the game started.
            // Create an entry so SetFavorite/RemoveFavorite can operate on it.
            logger::debug("favorite 0x{:08X}: no live entry — base inventory item, creating entry", formId);
            liveEntry = new RE::InventoryEntryData(form, count);
            if (!invChanges->entryList)
                return {false, "Inventory entry list not available"};
            invChanges->entryList->emplace_front(liveEntry);
        }

        // Get the first available ExtraDataList (may be nullptr for unmodified items).
        RE::ExtraDataList* xList = nullptr;
        if (liveEntry->extraLists) {
            for (auto* xl : *liveEntry->extraLists) {
                if (xl) {
                    xList = xl;
                    break;
                }
            }
        }

        if (liveEntry->IsFavorited()) {
            invChanges->RemoveFavorite(liveEntry, xList);
            logger::debug("favorite 0x{:08X}: removed from favorites", formId);
            PrintConsole("[WS] Unfavorite " + std::string(liveEntry->object->GetName()));
        } else {
            invChanges->SetFavorite(liveEntry, xList);
            logger::debug("favorite 0x{:08X}: added to favorites", formId);
            PrintConsole("[WS] Favorite " + std::string(liveEntry->object->GetName()));
        }
        return {true, ""};
    }

    CommandResult EquipSpell(RE::FormID formId, const std::string& hand)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* spell = RE::TESForm::LookupByID<RE::SpellItem>(formId);
        if (!spell)
            return {false, "Spell not found"};

        // Verify the player knows this spell.
        bool known = false;

        // 1) Check spells baked into the player's base NPC form.
        auto* npc       = player->GetActorBase();
        auto* spellData = npc ? npc->GetSpellList() : nullptr;
        if (spellData) {
            for (std::uint32_t i = 0; i < spellData->numSpells; ++i) {
                if (spellData->spells[i] && spellData->spells[i]->GetFormID() == formId) {
                    known = true;
                    break;
                }
            }
        }

        // 2) Check spells learned at runtime (spell tomes, AddSpell(), console, etc.).
        if (!known) {
            for (auto* s : player->GetActorRuntimeData().addedSpells) {
                if (s && s->GetFormID() == formId) {
                    known = true;
                    break;
                }
            }
        }

        if (!known)
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
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* targetSpell = RE::TESForm::LookupByID<RE::SpellItem>(formId);
        if (!targetSpell)
            return {false, "Spell not found"};

        // Verify the player knows this spell.
        bool known = false;

        // 1) Check spells baked into the player's base NPC form.
        auto* npc       = player->GetActorBase();
        auto* spellData = npc ? npc->GetSpellList() : nullptr;
        if (spellData) {
            for (std::uint32_t i = 0; i < spellData->numSpells; ++i) {
                if (spellData->spells[i] && spellData->spells[i]->GetFormID() == formId) {
                    known = true;
                    break;
                }
            }
        }

        // 2) Check spells learned at runtime (spell tomes, AddSpell(), console, etc.).
        if (!known) {
            for (auto* s : player->GetActorRuntimeData().addedSpells) {
                if (s && s->GetFormID() == formId) {
                    known = true;
                    break;
                }
            }
        }

        if (!known)
            return {false, "Spell not known by player"};

        auto* favorites = RE::MagicFavorites::GetSingleton();
        if (!favorites)
            return {false, "Magic favorites not available"};

        // Find the spell in favorites list to check if it's already favorited
        auto it = std::find(favorites->spells.begin(), favorites->spells.end(), 
                           static_cast<RE::TESForm*>(targetSpell));
        
        bool isFavorited = (it != favorites->spells.end());

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
}
