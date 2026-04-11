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

    // Returns the BGSEquipSlot for the requested hand (right = 0x13F43, left = 0x13F44).
    static const RE::BGSEquipSlot* GetHandSlot(const std::string& hand)
    {
        constexpr RE::FormID kRightHandSlot = 0x00013F43;
        constexpr RE::FormID kLeftHandSlot  = 0x00013F44;
        const RE::FormID id = (hand == "left") ? kLeftHandSlot : kRightHandSlot;
        return RE::TESForm::LookupByID<RE::BGSEquipSlot>(id);
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
        if (item->GetFormType() == RE::FormType::Weapon)
            slot = GetHandSlot(hand);

        equipManager->EquipObject(player, item, nullptr, 1, slot,
                                  false, true, true, true);
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
                                    false, true, true, true, nullptr);
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
                                  false, true, true, true);
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

            // We need an ExtraDataList to attach the ExtraHotkey to.
            RE::ExtraDataList* xList = nullptr;
            if (targetEntry->extraLists && !targetEntry->extraLists->empty())
                xList = targetEntry->extraLists->front();

            if (!xList) {
                // Create a fresh ExtraDataList and register it with the entry.
                xList = new RE::ExtraDataList();
                if (!targetEntry->extraLists)
                    targetEntry->extraLists =
                        new RE::BSSimpleList<RE::ExtraDataList*>();
                targetEntry->extraLists->push_front(xList);
            }

            // Create an ExtraHotkey with no hotkey binding (favourited only).
            auto* xHotkey = new RE::ExtraHotkey();
            xHotkey->index = RE::ExtraHotkey::Hotkey::kUnbound;
            xList->Add(xHotkey);

        } else {
            // Remove any ExtraHotkey entries from every list in this entry.
            if (targetEntry->extraLists) {
                for (auto* xList : *targetEntry->extraLists) {
                    if (xList && xList->HasType(RE::ExtraDataType::kHotkey))
                        xList->Remove(RE::ExtraDataType::kHotkey, nullptr);
                }
            }
        }

        return MakeSuccess();
    }
}
