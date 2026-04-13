#include "GameWriter.h"
#include "../Utils.h"

#include <cstdio>

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

    // Look up the BGSEquipSlot for a given hand name ("right" or "left").
    // Returns nullptr for non-weapon items where the engine should auto-select.
    static const RE::BGSEquipSlot* GetHandSlot(const std::string& hand)
    {
        auto* dom = RE::BGSDefaultObjectManager::GetSingleton();
        if (!dom)
            return nullptr;
        if (hand == "left")
            return dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kLeftHandEquip);
        return dom->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kRightHandEquip);
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

    // ─── Debug helpers ────────────────────────────────────────────────────

    // Formats a FormID as a hex string for debug output.
    static std::string FmtFormId(RE::FormID id)
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "0x%08X", id);
        return buf;
    }

    // ─── Commands ─────────────────────────────────────────────────────────

    CommandResult EquipItem(RE::FormID formId, const std::string& hand)
    {
        std::vector<std::string> dbg;
        dbg.push_back("EquipItem called: formId=" + FmtFormId(formId) + " hand=" + hand);

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available", dbg};

        auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!form)
            return {false, "Form not found", dbg};

        dbg.push_back("Item: " + std::string(form->GetName()) +
                       " type=" + std::to_string(static_cast<int>(form->GetFormType())));

        const int32_t itemCount = GetInventoryCount(player, formId);
        if (itemCount <= 0)
            return {false, "Item not in inventory", dbg};

        dbg.push_back("Inventory count: " + std::to_string(itemCount));

        const auto ft = form->GetFormType();
        if (!IsEquippable(ft))
            return {false, "Item is not equippable (use 'use' for consumables)", dbg};

        auto* equipMgr = RE::ActorEquipManager::GetSingleton();
        if (!equipMgr)
            return {false, "Equipment manager not available", dbg};

        const RE::BGSEquipSlot* slot  = nullptr;
        RE::ExtraDataList*      xData = nullptr;

        if (ft == RE::FormType::Weapon) {
            const auto* weap = form->As<RE::TESObjectWEAP>();
            if (weap) {
                dbg.push_back("WeaponType=" + std::to_string(static_cast<int>(weap->GetWeaponType())));
            }
            if (weap && IsWeaponTwoHanded(weap->GetWeaponType()) && hand == "left")
                return {false, "Two-handed weapon can only be equipped in the right hand", dbg};

            // --- Resolve target equip slot ---
            slot = GetHandSlot(hand);
            dbg.push_back("GetHandSlot(\"" + hand + "\") = " +
                           (slot ? FmtFormId(slot->GetFormID()) : "nullptr"));

            const auto* rightSlot = GetHandSlot("right");
            const auto* leftSlot  = GetHandSlot("left");
            dbg.push_back("LeftSlot="  + (leftSlot  ? FmtFormId(leftSlot->GetFormID())  : "nullptr") +
                           " RightSlot=" + (rightSlot ? FmtFormId(rightSlot->GetFormID()) : "nullptr"));

            // Log the weapon's own equip slot (from BGSEquipType).
            if (const auto* et = form->As<RE::BGSEquipType>()) {
                const auto* ws = et->GetEquipSlot();
                dbg.push_back("Weapon equipSlot: " +
                               (ws ? FmtFormId(ws->GetFormID()) : "nullptr"));
            }

            const bool leftHand = (hand == "left");

            // --- Analyse current equipped state ---
            auto* liveEntry = FindLiveEntry(player, formId);
            dbg.push_back("liveEntry=" + std::string(liveEntry ? "found" : "nullptr"));

            if (liveEntry) {
                int xListCount = 0;
                if (liveEntry->extraLists) {
                    for (auto* xl : *liveEntry->extraLists)
                        if (xl) ++xListCount;
                }
                dbg.push_back("extraLists count: " + std::to_string(xListCount));

                RE::ExtraDataList* wornRight = FindWornExtraDataList(liveEntry, false);
                RE::ExtraDataList* wornLeft  = FindWornExtraDataList(liveEntry, true);
                dbg.push_back("wornRight=" + std::string(wornRight ? "yes" : "no") +
                               " wornLeft=" + std::string(wornLeft ? "yes" : "no"));

                const bool inTarget = leftHand ? (wornLeft != nullptr)
                                               : (wornRight != nullptr);
                const bool inOther  = leftHand ? (wornRight != nullptr)
                                               : (wornLeft != nullptr);

                if (inTarget) {
                    dbg.push_back("Already in target hand, skipping");
                    return {true, "", dbg};
                }

                if (inOther) {
                    dbg.push_back("Weapon in OTHER hand, itemCount=" + std::to_string(itemCount));
                    if (itemCount < 2) {
                        RE::ExtraDataList* otherXList = leftHand ? wornRight : wornLeft;
                        dbg.push_back("Unequipping from " + std::string(!leftHand ? "left" : "right") + " hand first");
                        DoUnequipWeapon(equipMgr, player, form, otherXList, !leftHand);
                    }
                }

                xData = FindUnwornExtraDataList(liveEntry);
                dbg.push_back("FindUnwornExtraDataList = " +
                               std::string(xData ? "found" : "nullptr"));
            }
        }
        // For armor and ammo: slot = nullptr, xData = nullptr → engine auto-selects.

        dbg.push_back("Calling EquipObject: slot=" +
                       (slot ? FmtFormId(slot->GetFormID()) : "nullptr") +
                       " xData=" + std::string(xData ? "present" : "nullptr") +
                       " forceEquip=false");

        equipMgr->EquipObject(player, form, xData, 1, slot,
                              /*a_queueEquip=*/true,
                              /*a_forceEquip=*/false,   // Must stay false — true breaks equip logic
                              /*a_playSounds=*/true,
                              /*a_applyNow=*/false);

        // Post-equip check: verify which hand the weapon ended up in.
        if (ft == RE::FormType::Weapon) {
            auto* postEntry = FindLiveEntry(player, formId);
            if (postEntry) {
                bool postRight = FindWornExtraDataList(postEntry, false) != nullptr;
                bool postLeft  = FindWornExtraDataList(postEntry, true) != nullptr;
                dbg.push_back("Post-equip: right=" +
                               std::string(postRight ? "yes" : "no") +
                               " left=" + std::string(postLeft ? "yes" : "no"));
            } else {
                dbg.push_back("Post-equip: liveEntry not found");
            }
        }

        PrintConsole("[WS] Equip " + std::string(form->GetName()) + (slot ? " → " + hand : ""));
        return {true, "", dbg};
    }

    CommandResult UnequipItem(RE::FormID formId, const std::string& hand)
    {
        std::vector<std::string> dbg;
        dbg.push_back("UnequipItem called: formId=" + FmtFormId(formId) + " hand=" + hand);

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available", dbg};

        auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!form)
            return {false, "Form not found", dbg};

        if (GetInventoryCount(player, formId) <= 0)
            return {false, "Item not in inventory", dbg};

        auto* liveEntry = FindLiveEntry(player, formId);
        if (!liveEntry || !liveEntry->IsWorn())
            return {false, "Item is not equipped", dbg};

        auto* equipMgr = RE::ActorEquipManager::GetSingleton();
        if (!equipMgr)
            return {false, "Equipment manager not available", dbg};

        const auto ft = form->GetFormType();

        if (ft == RE::FormType::Weapon) {
            const bool leftHand = (hand == "left");

            RE::ExtraDataList* wornRight = FindWornExtraDataList(liveEntry, false);
            RE::ExtraDataList* wornLeft  = FindWornExtraDataList(liveEntry, true);

            dbg.push_back("Unequip: wornRight=" + std::string(wornRight ? "yes" : "no") +
                           " wornLeft=" + std::string(wornLeft ? "yes" : "no") +
                           " targetLeft=" + std::string(leftHand ? "yes" : "no"));

            bool doRight = !leftHand && wornRight;
            bool doLeft  = leftHand && wornLeft;

            if (!doRight && !doLeft) {
                doRight = wornRight != nullptr;
                doLeft  = wornLeft != nullptr;
                dbg.push_back("Fallback: doRight=" + std::string(doRight ? "yes" : "no") +
                               " doLeft=" + std::string(doLeft ? "yes" : "no"));
            }

            if (!doRight && !doLeft)
                return {false, "Weapon not found in any hand", dbg};

            if (doRight && wornRight) {
                dbg.push_back("Unequipping from RIGHT hand");
                DoUnequipWeapon(equipMgr, player, form, wornRight, false);
            }
            if (doLeft && wornLeft) {
                dbg.push_back("Unequipping from LEFT hand");
                DoUnequipWeapon(equipMgr, player, form, wornLeft, true);
            }
        } else {
            equipMgr->UnequipObject(player, form);
        }

        PrintConsole("[WS] Unequip " + std::string(form->GetName()));
        return {true, "", dbg};
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
