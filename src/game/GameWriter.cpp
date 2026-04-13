#include "GameWriter.h"
#include "../Utils.h"

namespace GameWriter
{
    // ─── Constants ────────────────────────────────────────────────────────

    // Sentinel value for ExtraHotkey::hotkey indicating "favorited but not
    // assigned to a specific hotkey slot".  The Skyrim engine treats any
    // ExtraHotkey presence as a favourite marker regardless of the value.
    static constexpr RE::ExtraHotkey::Hotkey kFavoriteNoHotkey = static_cast<RE::ExtraHotkey::Hotkey>(0xFF);

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

    // Returns true for weapon types that occupy both hands.
    static bool IsWeaponTwoHanded(RE::WEAPON_TYPE type)
    {
        switch (type) {
            case RE::WEAPON_TYPE::kTwoHandSword:
            case RE::WEAPON_TYPE::kTwoHandAxe:
            case RE::WEAPON_TYPE::kBow:
            case RE::WEAPON_TYPE::kCrossbow:
                return true;
            default:
                return false;
        }
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

        PrintConsole("[WS] Equip " + std::string(form->GetName()) + (slot ? " → " + hand : ""));
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

        if (GetInventoryCount(player, formId) <= 0)
            return {false, "Item not in inventory"};

        auto* liveEntry = FindLiveEntry(player, formId);
        if (!liveEntry)
            return {false, "Item not found in inventory changes"};

        if (liveEntry->IsFavorited()) {
            // Remove favorite — strip ExtraHotkey from all extra-data lists.
            if (liveEntry->extraLists) {
                for (auto* xList : *liveEntry->extraLists) {
                    if (!xList)
                        continue;
                    auto* hotkey = xList->GetByType<RE::ExtraHotkey>();
                    if (hotkey)
                        xList->Remove(hotkey);
                }
            }
            PrintConsole("[WS] Unfavorite " + std::string(liveEntry->object->GetName()));
        } else {
            // Add favorite — attach ExtraHotkey to the first available extra-data list.
            RE::ExtraDataList* targetXList = nullptr;
            if (liveEntry->extraLists) {
                for (auto* xList : *liveEntry->extraLists) {
                    if (xList) {
                        targetXList = xList;
                        break;
                    }
                }
            }
            if (!targetXList)
                return {false, "Cannot favorite: item has no extra data list"};

            // ExtraHotkey is a game-engine-managed object; ownership transfers
            // to the ExtraDataList on Add().
            auto* hotkey    = new RE::ExtraHotkey();
            hotkey->hotkey  = kFavoriteNoHotkey;
            targetXList->Add(hotkey);
            PrintConsole("[WS] Favorite " + std::string(liveEntry->object->GetName()));
        }
        return {true, ""};
    }
}
