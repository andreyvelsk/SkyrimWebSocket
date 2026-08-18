#include "HotkeyCommands.h"
#include "Common.h"
#include "../Utils.h"

#include <algorithm>
#include <format>

namespace logger = SKSE::log;

namespace HotkeyCommands
{
    // ─── Private helpers ───────────────────────────────────────────────────

    // True when a spell form can be placed on a hotkey slot (same categories
    // that the vanilla magic favorites menu accepts: spells, powers, lesser
    // powers, shouts).  Diseases, abilities, scrolls etc. are not eligible.
    static bool IsHotkeyableSpell(RE::SpellItem* spell)
    {
        if (!spell)
            return false;
        switch (spell->GetSpellType()) {
            case RE::MagicSystem::SpellType::kSpell:
            case RE::MagicSystem::SpellType::kPower:
            case RE::MagicSystem::SpellType::kLesserPower:
            case RE::MagicSystem::SpellType::kVoicePower:
                return true;
            default:
                return false;
        }
    }

    // Ensures MagicFavorites::hotkeys has exactly 8 slots.  The game usually
    // keeps this array sized at 8, but saves from other mods may leave it
    // shorter — resize defensively so that direct indexing is always safe.
    static void EnsureHotkeySlots(RE::MagicFavorites* favorites)
    {
        if (!favorites)
            return;
        while (favorites->hotkeys.size() < 8)
            favorites->hotkeys.push_back(nullptr);
    }

    // Result of scanning the inventory for a hotkey binding.
    struct ItemHotkeyRef
    {
        RE::InventoryEntryData* entry = nullptr;
        RE::ExtraDataList*      xList = nullptr;
        RE::ExtraHotkey*        extra = nullptr;
    };

    // Find the inventory entry + extra-data list carrying ExtraHotkey with the
    // given slot index.  Returns an empty struct when no match is found.
    static ItemHotkeyRef FindItemHotkey(RE::PlayerCharacter* player, std::uint8_t slotIdx)
    {
        ItemHotkeyRef ref{};
        if (!player)
            return ref;
        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges || !invChanges->entryList)
            return ref;

        for (auto* entry : *invChanges->entryList) {
            if (!entry || !entry->extraLists)
                continue;
            for (auto* xList : *entry->extraLists) {
                if (!xList)
                    continue;
                auto* xHotkey = xList->GetByType<RE::ExtraHotkey>();
                if (xHotkey && xHotkey->hotkey.underlying() == slotIdx) {
                    ref.entry = entry;
                    ref.xList = xList;
                    ref.extra = xHotkey;
                    return ref;
                }
            }
        }
        return ref;
    }

    // Remove any existing binding for slot `slotIdx` from both magic and item stores.
    static void ClearSlotBinding(RE::PlayerCharacter* player,
                                 RE::MagicFavorites*  favorites,
                                 std::uint8_t         slotIdx)
    {
        if (favorites && slotIdx < favorites->hotkeys.size())
            favorites->hotkeys[slotIdx] = nullptr;

        auto itemRef = FindItemHotkey(player, slotIdx);
        if (itemRef.xList && itemRef.extra)
            itemRef.xList->Remove<RE::ExtraHotkey>(itemRef.extra);
    }

    // Validates user-facing slot number (1..8) and stores the 0-based index.
    static bool ValidateSlot(std::uint8_t slot, std::uint8_t& outIdx)
    {
        if (slot < 1 || slot > 8)
            return false;
        outIdx = static_cast<std::uint8_t>(slot - 1);
        return true;
    }

    // Ensure a spell/shout form is in the MagicFavorites list (hotkey overlay
    // only shows favorited forms).  Idempotent.
    static void EnsureMagicFavorited(RE::MagicFavorites* favorites, RE::TESForm* form)
    {
        if (std::find(favorites->spells.begin(), favorites->spells.end(), form)
            == favorites->spells.end()) {
            favorites->SetFavorite(form);
        }
    }

    // Find an existing ExtraHotkey on the item, or create one via SetFavorite.
    // Returns the ExtraDataList and ExtraHotkey pointers, or nullptr on failure.
    static bool FindOrCreateHotkeyExtra(RE::InventoryChanges*  invChanges,
                                        RE::InventoryEntryData* liveEntry,
                                        RE::ExtraDataList*&     outXList,
                                        RE::ExtraHotkey*&       outExtra)
    {
        outXList = nullptr;
        outExtra = nullptr;

        // Look for an existing ExtraHotkey on this item.
        if (liveEntry->extraLists) {
            for (auto* xl : *liveEntry->extraLists) {
                if (!xl) continue;
                if (auto* xh = xl->GetByType<RE::ExtraHotkey>()) {
                    outXList = xl;
                    outExtra = xh;
                    return true;
                }
            }
        }

        // No existing ExtraHotkey — call SetFavorite to allocate one.
        RE::ExtraDataList* xList = nullptr;
        if (liveEntry->extraLists) {
            for (auto* xl : *liveEntry->extraLists) {
                if (xl) { xList = xl; break; }
            }
        }
        invChanges->SetFavorite(liveEntry, xList);

        // Re-scan after SetFavorite — it may have created a new xList.
        if (liveEntry->extraLists) {
            for (auto* xl : *liveEntry->extraLists) {
                if (!xl) continue;
                if (auto* xh = xl->GetByType<RE::ExtraHotkey>()) {
                    outXList = xl;
                    outExtra = xh;
                    return true;
                }
            }
        }
        return false;
    }

    // ─── Commands ─────────────────────────────────────────────────────────

    CommandResult SetHotkey(std::uint8_t slot, RE::FormID formId)
    {
        std::uint8_t slotIdx;
        if (!ValidateSlot(slot, slotIdx))
            return {false, "Slot must be in range 1..8"};

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* favorites = RE::MagicFavorites::GetSingleton();
        if (!favorites)
            return {false, "Magic favorites not available"};
        EnsureHotkeySlots(favorites);

        auto* form = RE::TESForm::LookupByID(formId);
        if (!form)
            return {false, "Form not found"};

        // Path A: spell hotkey.
        if (auto* spell = form->As<RE::SpellItem>()) {
            if (!IsHotkeyableSpell(spell))
                return {false, "Spell type cannot be hotkeyed"};
            if (!Common::PlayerKnowsSpell(player, spell))
                return {false, "Spell not known by player"};

            ClearSlotBinding(player, favorites, slotIdx);
            EnsureMagicFavorited(favorites, static_cast<RE::TESForm*>(spell));
            favorites->hotkeys[slotIdx] = spell;

            PrintConsole(std::format("[WS] Hotkey {} <- spell {}", slot, spell->GetName()));
            return {true, ""};
        }

        // Path A½: dragon shout hotkey.
        if (auto* shout = form->As<RE::TESShout>()) {
            if (!player->HasShout(shout))
                return {false, "Shout not known by player"};

            ClearSlotBinding(player, favorites, slotIdx);
            EnsureMagicFavorited(favorites, static_cast<RE::TESForm*>(shout));
            favorites->hotkeys[slotIdx] = shout;

            PrintConsole(std::format("[WS] Hotkey {} <- shout {}", slot, shout->GetName()));
            return {true, ""};
        }

        // Path B: inventory item hotkey.
        auto* bound = form->As<RE::TESBoundObject>();
        if (!bound)
            return {false, "Form is neither a spell nor a bound object"};

        if (Common::GetInventoryCount(player, formId) <= 0)
            return {false, "Item not in inventory"};

        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges)
            return {false, "Inventory changes not available"};

        auto* liveEntry = Common::MaterializeInventoryEntry(player, bound);
        if (!liveEntry)
            return {false, "Failed to materialize inventory entry for item"};

        ClearSlotBinding(player, favorites, slotIdx);

        RE::ExtraDataList* targetXList = nullptr;
        RE::ExtraHotkey*   targetExtra = nullptr;
        if (!FindOrCreateHotkeyExtra(invChanges, liveEntry, targetXList, targetExtra))
            return {false, "Failed to attach ExtraHotkey to item"};

        targetExtra->hotkey = static_cast<RE::ExtraHotkey::Hotkey>(slotIdx);

        PrintConsole(std::format("[WS] Hotkey {} <- item {}", slot, bound->GetName()));
        return {true, ""};
    }

    CommandResult ClearHotkey(std::uint8_t slot)
    {
        std::uint8_t slotIdx;
        if (!ValidateSlot(slot, slotIdx))
            return {false, "Slot must be in range 1..8"};

        auto* player    = RE::PlayerCharacter::GetSingleton();
        auto* favorites = RE::MagicFavorites::GetSingleton();
        if (!player || !favorites)
            return {false, "Player or favorites not available"};
        EnsureHotkeySlots(favorites);

        ClearSlotBinding(player, favorites, slotIdx);
        PrintConsole(std::format("[WS] Hotkey {} cleared", slot));
        return {true, ""};
    }

    CommandResult TriggerHotkey(std::uint8_t slot)
    {
        std::uint8_t slotIdx;
        if (!ValidateSlot(slot, slotIdx))
            return {false, "Slot must be in range 1..8"};

        // Native engine path: synthesize a keyboard ButtonEvent with the
        // appropriate "HotkeyN" user-event name and feed it directly to
        // FavoritesHandler::ProcessButton — the same call the game itself
        // performs when the player presses 1..8 in gameplay.  This ensures
        // 100% vanilla behavior, including:
        //   * spells: right-hand → left-hand → no-op toggle,
        //   * shouts/powers: voice slot equip,
        //   * weapons: equip ↔ unequip toggle,
        //   * 1H weapon with 2+ copies: right → other-hand → no-op,
        //   * armor / ammo / consumables handled identically to vanilla,
        //   * any third-party MCM tweaks of FavoritesHandler are honored.

        auto* mc = RE::MenuControls::GetSingleton();
        if (!mc || !mc->favoritesHandler)
            return {false, "MenuControls/FavoritesHandler not available"};

        auto* userEvents = RE::UserEvents::GetSingleton();
        if (!userEvents)
            return {false, "UserEvents singleton not available"};

        // Pick the BSFixedString that the engine uses for this hotkey slot
        // (matches what UserEvents stores: "Hotkey1".."Hotkey8").
        const RE::BSFixedString* hotkeyNames[8] = {
            &userEvents->hotkey1, &userEvents->hotkey2,
            &userEvents->hotkey3, &userEvents->hotkey4,
            &userEvents->hotkey5, &userEvents->hotkey6,
            &userEvents->hotkey7, &userEvents->hotkey8,
        };
        const RE::BSFixedString& userEvent = *hotkeyNames[slotIdx];

        // DIK_1..DIK_8 are 0x02..0x09 — what the keyboard input layer would
        // report.  Value=1.0 + heldDownSecs=0.0 represents a fresh key-down
        // (ButtonEvent::IsDown() == true), which is what FavoritesHandler
        // acts on.
        const std::uint32_t idCode = 0x02u + slotIdx;

        auto* event = RE::ButtonEvent::Create(
            RE::INPUT_DEVICE::kKeyboard, userEvent, idCode, 1.0f, 0.0f);
        if (!event)
            return {false, "Failed to allocate ButtonEvent"};

        auto* handler = mc->favoritesHandler;
        const bool handled = handler->ProcessButton(event);

        // ButtonEvent::Create allocates via the game's heap; release it.
        RE::free(event);

        PrintConsole(std::format("[WS] Hotkey {} triggered (handled={})",
                                 slot, handled ? "true" : "false"));
        return {true, ""};
    }
}