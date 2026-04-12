#include "InventoryWriter.h"

#include <format>

namespace InventoryWriter
{
    // ─── Helpers ──────────────────────────────────────────────────────────

    static nlohmann::json MakeSuccess()
    {
        return { { "success", true } };
    }

    static nlohmann::json MakeError(const char* msg)
    {
        return { { "success", false }, { "error", msg } };
    }

    // Returns true and populates `count` if the item is in the player's inventory.
    static bool FindInInventory(RE::PlayerCharacter* player,
                                RE::TESBoundObject*  item,
                                int&                 outCount)
    {
        auto inv = player->GetInventory(
            [item](RE::TESBoundObject& obj) { return &obj == item; });
        auto it = inv.find(item);
        if (it == inv.end() || it->second.first <= 0)
            return false;
        outCount = it->second.first;
        return true;
    }

    // Returns the BGSEquipSlot for the requested hand.
    // Empirically verified: 0x13F44 is the right-hand slot, 0x13F43 is the left-hand slot.
    static const RE::BGSEquipSlot* GetHandSlot(const std::string& hand)
    {
        constexpr RE::FormID kRightHandSlot = 0x00013F44;
        constexpr RE::FormID kLeftHandSlot  = 0x00013F43;
        const RE::FormID id = (hand == "left") ? kLeftHandSlot : kRightHandSlot;
        return RE::TESForm::LookupByID<RE::BGSEquipSlot>(id);
    }

    // Returns true when the item is currently equipped in the specified hand.
    // isLeft=true  → checks kWornLeft  (left hand)
    // isLeft=false → checks kWorn      (right hand)
    static bool IsWornInHand(RE::PlayerCharacter* player,
                              RE::TESBoundObject*  item,
                              bool                 isLeft)
    {
        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges || !invChanges->entryList)
            return false;

        for (auto* entry : *invChanges->entryList) {
            if (!entry || entry->object != item || !entry->extraLists)
                continue;
            for (auto* xList : *entry->extraLists) {
                if (!xList)
                    continue;
                const bool worn = isLeft ? xList->HasType(RE::ExtraDataType::kWornLeft)
                                         : xList->HasType(RE::ExtraDataType::kWorn);
                if (worn)
                    return true;
            }
            break;  // only one entry per form
        }
        return false;
    }

    // Returns the first ExtraDataList that is NOT worn in the target hand, or nullptr
    // if the item has no ExtraDataLists at all.  Passing this xList to EquipObject
    // tells the engine which specific instance to equip, preventing it from grabbing
    // a copy that is already equipped in the other hand.
    //
    // isLeft=true  → target hand is left  → skip xLists that have kWornLeft
    // isLeft=false → target hand is right → skip xLists that have kWorn
    static RE::ExtraDataList* FindXListNotInHand(RE::PlayerCharacter* player,
                                                 RE::TESBoundObject*  item,
                                                 bool                 isLeft)
    {
        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges || !invChanges->entryList)
            return nullptr;

        for (auto* entry : *invChanges->entryList) {
            if (!entry || entry->object != item)
                continue;
            if (!entry->extraLists)
                return nullptr;  // item has no per-instance data at all; passing nullptr lets
                                 // the engine create a fresh ExtraDataList for this equip

            for (auto* xList : *entry->extraLists) {
                if (!xList)
                    continue;
                const bool inTargetHand = isLeft ? xList->HasType(RE::ExtraDataType::kWornLeft)
                                                 : xList->HasType(RE::ExtraDataType::kWorn);
                if (!inTargetHand)
                    return xList;  // found a usable xList
            }
            return nullptr;  // all xLists are in the target hand (already equipped there)
        }
        return nullptr;
    }

    // ─── Equip ────────────────────────────────────────────────────────────

    nlohmann::json Equip(std::uint32_t formId, const std::string& hand)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return MakeError("Player not available");

        auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!item)
            return MakeError("Item form not found");

        int count = 0;
        if (!FindInInventory(player, item, count))
            return MakeError("Item not in player inventory");

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager)
            return MakeError("Equip manager not available");

        // Only weapons need hand-specific slot and xList logic.
        // All other equippable items use their own default slot.
        const RE::BGSEquipSlot* slot   = nullptr;
        RE::ExtraDataList*      xList  = nullptr;
        const bool              isWeapon     = (item->GetFormType() == RE::FormType::Weapon);
        const bool              targetIsLeft = (hand == "left");

        if (isWeapon) {
            slot = GetHandSlot(hand);

            // Short-circuit: item is already equipped in the requested hand.
            if (IsWornInHand(player, item, targetIsLeft))
                return MakeSuccess();

            // Mirror SKSE's EquipItemEx approach: pass the first ExtraDataList that is
            // NOT already in the target hand.  This tells the engine which specific
            // instance to use, so it does NOT grab the copy worn in the other hand.
            //
            // Engine behaviour when it receives a specific xList:
            //   • xList is free (unworn)         → equips it to the target slot.
            //   • xList is worn in the OTHER hand AND count > worn → creates a new
            //     xList for the target slot, leaving the other-hand copy untouched
            //     (this is the dual-wield same-item case).
            //   • xList is worn in the OTHER hand AND count == worn (single copy) →
            //     moves it from the other slot to the target slot.
            //
            // If the item has no ExtraDataLists at all (fresh/base copy), nullptr is
            // returned and the engine creates a new xList automatically.
            xList = FindXListNotInHand(player, item, targetIsLeft);
        }

        // forceEquip=false is the NORMAL equip mode.
        // forceEquip=true locks the item (abPreventRemoval), causing the engine to show
        // "You cannot remove this item" and re-equip it automatically when something
        // else displaces it.  We never want that behaviour here.
        equipManager->EquipObject(player, item, xList, 1, slot,
                                  /*queueEquip*/ false, /*forceEquip*/ false,
                                  /*playSounds*/ true,  /*applyNow*/   true);
        return MakeSuccess();
    }

    // ─── Unequip ──────────────────────────────────────────────────────────

    nlohmann::json Unequip(std::uint32_t formId, const std::string& hand)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return MakeError("Player not available");

        auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!item)
            return MakeError("Item form not found");

        int count = 0;
        if (!FindInInventory(player, item, count))
            return MakeError("Item not in player inventory");

        // Check whether the item is actually worn before trying to unequip.
        {
            auto inv = player->GetInventory(
                [item](RE::TESBoundObject& obj) { return &obj == item; });
            auto it = inv.find(item);
            if (it == inv.end() || !it->second.second || !it->second.second->IsWorn())
                return MakeError("Item is not currently equipped");
        }

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager)
            return MakeError("Equip manager not available");

        // For weapons, allow targeting a specific hand slot.
        // Passing nullptr unequips from whichever slot holds the item.
        const RE::BGSEquipSlot* slot = nullptr;
        if (item->GetFormType() == RE::FormType::Weapon) {
            if (hand == "right" || hand == "left")
                slot = GetHandSlot(hand);
        }

        equipManager->UnequipObject(player, item, nullptr, 1, slot,
                                    /*queueEquip*/ false, /*forceEquip*/ true,
                                    /*playSounds*/ true,  /*applyNow*/   true,
                                    /*otherItem*/  nullptr);
        return MakeSuccess();
    }

    // ─── Use ──────────────────────────────────────────────────────────────

    nlohmann::json Use(std::uint32_t formId)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return MakeError("Player not available");

        auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!item)
            return MakeError("Item form not found");

        const auto formType = item->GetFormType();
        if (formType != RE::FormType::AlchemyItem &&
            formType != RE::FormType::Scroll)
            return MakeError(
                "Item cannot be used — only potions, food, and scrolls are supported");

        int count = 0;
        if (!FindInInventory(player, item, count))
            return MakeError("Item not in player inventory");

        auto* equipManager = RE::ActorEquipManager::GetSingleton();
        if (!equipManager)
            return MakeError("Equip manager not available");

        // Equipping a potion/food item triggers consumption; scrolls are cast.
        // Pass nullptr for the slot so the game uses the item's default behaviour.
        equipManager->EquipObject(player, item, nullptr, 1, nullptr,
                                  /*queueEquip*/ false, /*forceEquip*/ true,
                                  /*playSounds*/ true,  /*applyNow*/   true);
        return MakeSuccess();
    }

    // ─── Drop ─────────────────────────────────────────────────────────────

    nlohmann::json Drop(std::uint32_t formId, int count)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return MakeError("Player not available");

        auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!item)
            return MakeError("Item form not found");

        int available = 0;
        if (!FindInInventory(player, item, available))
            return MakeError("Item not in player inventory");

        if (count <= 0)
            count = 1;
        if (count > available)
            count = available;

        player->RemoveItem(item, count, RE::ITEM_REMOVE_REASON::kDropping,
                           nullptr, nullptr);
        return MakeSuccess();
    }

    // ─── Favorite ─────────────────────────────────────────────────────────

    nlohmann::json Favorite(std::uint32_t formId, bool add)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return MakeError("Player not available");

        auto* item = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!item)
            return MakeError("Item form not found");

        int count = 0;
        if (!FindInInventory(player, item, count))
            return MakeError("Item not in player inventory");

        // Work with the live InventoryChanges so that modifications persist.
        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges || !invChanges->entryList)
            return MakeError("Inventory changes not available");

        RE::InventoryEntryData* targetEntry = nullptr;
        for (auto* entry : *invChanges->entryList) {
            if (entry && entry->object == item) {
                targetEntry = entry;
                break;
            }
        }

        if (!targetEntry)
            return MakeError(
                "Inventory entry not found — item may be a base-container item");

        if (add) {
            // If the item is already favourited there is nothing to do.
            if (targetEntry->IsFavorited())
                return MakeSuccess();

            // Pass the first existing ExtraDataList to SetFavorite, or nullptr
            // if there is none — the game will create one internally as needed.
            RE::ExtraDataList* xList =
                (targetEntry->extraLists && !targetEntry->extraLists->empty())
                    ? targetEntry->extraLists->front()
                    : nullptr;

            invChanges->SetFavorite(targetEntry, xList);

        } else {
            // Find the ExtraDataList that holds the hotkey so we can pass it to
            // RemoveFavorite; if none is found the item is already not favourited.
            RE::ExtraDataList* xList = nullptr;
            if (targetEntry->extraLists) {
                for (auto* list : *targetEntry->extraLists) {
                    if (list && list->HasType(RE::ExtraDataType::kHotkey)) {
                        xList = list;
                        break;
                    }
                }
            }

            if (!xList)
                return MakeSuccess();

            invChanges->RemoveFavorite(targetEntry, xList);
        }

        return MakeSuccess();
    }
}
