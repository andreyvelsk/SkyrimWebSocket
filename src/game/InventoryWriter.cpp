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

    // Returns the item (by FormID) currently worn in the given hand slot, or 0 if empty.
    // wantLeft=true  → checks kWornLeft  (left hand)
    // wantLeft=false → checks kWorn      (right hand / default slot)
    static RE::FormID GetWornInHand(RE::PlayerCharacter* player, bool wantLeft)
    {
        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges || !invChanges->entryList)
            return 0;

        for (auto* entry : *invChanges->entryList) {
            if (!entry || !entry->object || !entry->extraLists)
                continue;
            for (auto* xList : *entry->extraLists) {
                if (!xList)
                    continue;
                const bool match = wantLeft
                                       ? xList->HasType(RE::ExtraDataType::kWornLeft)
                                       : xList->HasType(RE::ExtraDataType::kWorn);
                if (match)
                    return entry->object->GetFormID();
            }
        }
        return 0;
    }

    // Returns how many instances of `item` are currently worn in either hand slot.
    static int CountWornInstances(RE::PlayerCharacter* player, RE::TESBoundObject* item)
    {
        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges || !invChanges->entryList)
            return 0;

        for (auto* entry : *invChanges->entryList) {
            if (!entry || entry->object != item || !entry->extraLists)
                continue;
            int worn = 0;
            for (auto* xList : *entry->extraLists) {
                if (!xList)
                    continue;
                if (xList->HasType(RE::ExtraDataType::kWorn) ||
                    xList->HasType(RE::ExtraDataType::kWornLeft))
                    ++worn;
            }
            return worn;
        }
        return 0;
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

        // Only pass a hand-specific slot for weapons; all other equippable items
        // use their own default slot (nullptr = let the game decide).
        const RE::BGSEquipSlot* slot = nullptr;
        const bool isWeapon = (item->GetFormType() == RE::FormType::Weapon);
        const bool targetIsLeft = (hand == "left");

        // For weapons, snapshot what is currently in the OTHER hand before the
        // equip call.  The engine sometimes unequips the other hand as a side
        // effect of its internal one-handed weapon slot logic; we detect and
        // correct that below.
        RE::FormID otherHandFormId = 0;
        if (isWeapon) {
            slot          = GetHandSlot(hand);
            otherHandFormId = GetWornInHand(player, !targetIsLeft);
        }

        equipManager->EquipObject(player, item, nullptr, 1, slot,
                                  /*queueEquip*/ false, /*forceEquip*/ true,
                                  /*playSounds*/ true,  /*applyNow*/   true);

        // ── Protect the other hand ────────────────────────────────────────
        // If the engine unequipped the other hand as a collateral effect,
        // silently restore it — but only when a free (non-worn) instance of
        // that item is still available in the inventory.
        if (isWeapon && otherHandFormId != 0) {
            const RE::FormID nowOther = GetWornInHand(player, !targetIsLeft);
            if (nowOther != otherHandFormId) {
                auto* otherItem = RE::TESForm::LookupByID<RE::TESBoundObject>(otherHandFormId);
                if (otherItem) {
                    int totalCount = 0;
                    FindInInventory(player, otherItem, totalCount);
                    const int wornCount = CountWornInstances(player, otherItem);
                    // Only re-equip when there is at least one unequipped instance.
                    if (totalCount > wornCount) {
                        const RE::BGSEquipSlot* otherSlot =
                            GetHandSlot(targetIsLeft ? "right" : "left");
                        equipManager->EquipObject(player, otherItem, nullptr, 1, otherSlot,
                                                  /*queueEquip*/ false, /*forceEquip*/ true,
                                                  /*playSounds*/ false, /*applyNow*/   true);
                    }
                }
            }
        }

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
