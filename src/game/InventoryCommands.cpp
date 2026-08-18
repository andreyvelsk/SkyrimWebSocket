#include "InventoryCommands.h"
#include "Common.h"
#include "../Utils.h"

namespace logger = SKSE::log;

namespace InventoryCommands
{
    // ─── Private helpers ───────────────────────────────────────────────────

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

    // Papyrus Actor.EquipItemEx / UnequipItemEx aiEquipSlot values.
    // See https://ck.uesp.net/wiki/EquipItemEx_-_Actor
    //   0 = Default (engine picks; right for 1H, both for 2H, body slot for armor)
    //   1 = Right Hand
    //   2 = Left Hand
    static constexpr std::int32_t kEquipSlotDefault   = 0;
    static constexpr std::int32_t kEquipSlotRightHand = 1;
    static constexpr std::int32_t kEquipSlotLeftHand  = 2;

    // ─── Commands ─────────────────────────────────────────────────────────

    CommandResult EquipItem(RE::FormID formId, const std::string& hand)
    {
        logger::trace("EquipItem enter: formId=0x{:08X} hand='{}'", formId, hand);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!form)
            return {false, "Form not found"};

        const int32_t itemCount = Common::GetInventoryCount(player, formId);
        logger::trace("EquipItem 0x{:08X} ('{}') invCount={} formType={}",
                      formId, form->GetName(), itemCount, static_cast<int>(form->GetFormType()));
        if (itemCount <= 0)
            return {false, "Item not in inventory"};

        const auto ft = form->GetFormType();
        if (!IsEquippable(ft))
            return {false, "Item is not equippable (use 'use' for consumables)"};

        // Determine the Papyrus aiEquipSlot parameter.  The engine's
        // Actor.EquipItemEx handles all the 2H↔1H swapping, ExtraDataList
        // allocation and inventory bookkeeping that we previously did by hand.
        std::int32_t slotArg = kEquipSlotDefault;
        if (ft == RE::FormType::Weapon) {
            const auto* weap = form->As<RE::TESObjectWEAP>();
            const bool  twoHanded = weap && IsWeaponTwoHanded(weap->GetWeaponType());
            if (twoHanded && hand == "left")
                return {false, "Two-handed weapon can only be equipped in the right hand"};
            if (twoHanded)
                slotArg = kEquipSlotDefault;  // engine auto-grips 2H with both hands
            else if (hand == "left")
                slotArg = kEquipSlotLeftHand;
            else
                slotArg = kEquipSlotRightHand;
        }
        // For armor and ammo aiEquipSlot is ignored by the engine, so leave
        // it at the right-hand default.

        logger::trace("EquipItem 0x{:08X}: dispatching Actor.EquipItemEx slot={}", formId, slotArg);
        const bool ok = Common::DispatchPlayerMethod(
            player, "Actor", "EquipItemEx",
            static_cast<RE::TESForm*>(form),
            slotArg,
            /*abPreventRemoval=*/false,
            /*abSilent=*/false);
        if (!ok)
            return {false, "Papyrus dispatch failed"};

        logger::debug("equip 0x{:08X} ('{}') hand='{}' type={} slot={}",
                      formId, form->GetName(), hand, static_cast<int>(ft), slotArg);
        PrintConsole("[WS] Equip " + std::string(form->GetName()) +
                     (ft == RE::FormType::Weapon ? " → " + hand : ""));
        return {true, ""};
    }

    CommandResult UnequipItem(RE::FormID formId, const std::string& hand)
    {
        logger::trace("UnequipItem enter: formId=0x{:08X} hand='{}'", formId, hand);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!form)
            return {false, "Form not found"};

        if (Common::GetInventoryCount(player, formId) <= 0)
            return {false, "Item not in inventory"};

        // For weapons we select the specific hand slot via UnequipItemEx.
        // For everything else we call UnequipItem (slot is irrelevant and
        // the engine will unequip wherever the item is currently worn).
        const auto ft = form->GetFormType();
        bool ok = false;
        if (ft == RE::FormType::Weapon) {
            std::int32_t slotArg = (hand == "left") ? kEquipSlotLeftHand : kEquipSlotRightHand;
            logger::trace("UnequipItem 0x{:08X}: dispatching Actor.UnequipItemEx slot={}", formId, slotArg);
            ok = Common::DispatchPlayerMethod(
                player, "Actor", "UnequipItemEx",
                static_cast<RE::TESForm*>(form),
                slotArg,
                /*abPreventEquipping=*/false);
        } else {
            logger::trace("UnequipItem 0x{:08X}: dispatching Actor.UnequipItem", formId);
            ok = Common::DispatchPlayerMethod(
                player, "Actor", "UnequipItem",
                static_cast<RE::TESForm*>(form),
                /*abPreventEquipping=*/false,
                /*abSilent=*/false);
        }
        if (!ok)
            return {false, "Papyrus dispatch failed"};

        logger::debug("unequip 0x{:08X} ('{}') hand='{}'", formId, form->GetName(), hand);
        PrintConsole("[WS] Unequip " + std::string(form->GetName()));
        return {true, ""};
    }

    CommandResult UseItem(RE::FormID formId)
    {
        logger::trace("UseItem enter: formId=0x{:08X}", formId);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!form)
            return {false, "Form not found"};

        const int32_t invCnt = Common::GetInventoryCount(player, formId);
        logger::trace("UseItem 0x{:08X} ('{}') invCount={} formType={}",
                      formId, form->GetName(), invCnt, static_cast<int>(form->GetFormType()));
        if (invCnt <= 0)
            return {false, "Item not in inventory"};

        if (!IsConsumable(form->GetFormType()))
            return {false, "Item is not consumable (use 'equip' for weapons/apparel)"};

        // Actor.EquipItem on a potion/food/scroll triggers consumption (or
        // equip-for-casting for scrolls), mirroring vanilla UI behaviour.
        // abSilent=true suppresses the "<item> equipped" HUD message that
        // Skyrim does not show when the player consumes the item from the
        // inventory menu.
        const bool ok = Common::DispatchPlayerMethod(
            player, "Actor", "EquipItem",
            static_cast<RE::TESForm*>(form),
            /*abPreventRemoval=*/false,
            /*abSilent=*/true);
        if (!ok)
            return {false, "Papyrus dispatch failed"};

        logger::debug("use 0x{:08X} ('{}')", formId, form->GetName());
        PrintConsole("[WS] Use " + std::string(form->GetName()));
        return {true, ""};
    }

    CommandResult ReadBook(RE::FormID formId)
    {
        logger::trace("ReadBook enter: formId=0x{:08X}", formId);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* book = RE::TESForm::LookupByID<RE::TESObjectBOOK>(formId);
        if (!book)
            return {false, "Form not found or is not a book"};

        const int32_t invCnt = Common::GetInventoryCount(player, formId);
        logger::trace("ReadBook 0x{:08X} ('{}') invCount={}",
                      formId, book->GetName(), invCnt);
        if (invCnt <= 0)
            return {false, "Book not in inventory"};

        // Spell tomes: use the native read path, then mirror vanilla
        // consumption semantics by removing one tome only when the spell
        // actually transitioned from unknown -> known.
        if (book->TeachesSpell()) {
            auto* spell = book->GetSpell();
            const bool knownBefore = spell ? player->HasSpell(spell) : false;

            const bool ok = book->Read(player);
            if (!ok)
                return {false, "Failed to read spell tome"};

            const bool knownAfter = spell ? player->HasSpell(spell) : knownBefore;
            if (!knownBefore && knownAfter && Common::GetInventoryCount(player, formId) > 0) {
                player->RemoveItem(book, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            }

            logger::debug("read_book(spell_tome) 0x{:08X} ('{}')", formId, book->GetName());
            PrintConsole("[WS] Read spell tome " + std::string(book->GetName()));
            return {true, ""};
        }

        const auto* xList = [&]() -> const RE::ExtraDataList* {
            auto* liveEntry = Common::FindLiveEntry(player, formId);
            if (!liveEntry || !liveEntry->extraLists)
                return nullptr;
            for (auto* xl : *liveEntry->extraLists) {
                if (xl)
                    return xl;
            }
            return nullptr;
        }();

        RE::NiMatrix3 rot{};
        rot.SetEulerAnglesXYZ(-0.05f, -0.05f, 1.50f);

        RE::BookMenu::OpenMenuFromBaseForm(book, xList, RE::NiPoint3(), rot, 1.0f, true);

        logger::debug("read_book 0x{:08X} ('{}')", formId, book->GetName());
        PrintConsole("[WS] Read book " + std::string(book->GetName()));
        return {true, ""};
    }

    CommandResult DropItem(RE::FormID formId, int count)
    {
        logger::trace("DropItem enter: formId=0x{:08X} count={}", formId, count);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player not available"};

        auto* form = RE::TESForm::LookupByID<RE::TESBoundObject>(formId);
        if (!form)
            return {false, "Form not found"};

        if (count <= 0)
            return {false, "Count must be positive"};

        const int32_t have = Common::GetInventoryCount(player, formId);
        if (have <= 0)
            return {false, "Item not in inventory"};
        if (count > have)
            return {false, "Not enough items (have " + std::to_string(have) + ")"};

        // ObjectReference.DropObject is the Papyrus equivalent of the vanilla
        // drop-from-inventory action (animation, sound, physics).
        const bool ok = Common::DispatchPlayerMethod(
            player, "ObjectReference", "DropObject",
            static_cast<RE::TESForm*>(form),
            static_cast<std::int32_t>(count));
        if (!ok)
            return {false, "Papyrus dispatch failed"};

        logger::debug("drop 0x{:08X} ('{}') count={}", formId, form->GetName(), count);
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

        const int32_t count = Common::GetInventoryCount(player, formId);
        logger::debug("favorite 0x{:08X} ('{}') invCount={}", formId, form->GetName(), count);

        if (count <= 0)
            return {false, "Item not in inventory"};

        auto* invChanges = player->GetInventoryChanges();
        logger::trace("favorite 0x{:08X}: invChanges={}", formId, static_cast<const void*>(invChanges));
        if (!invChanges)
            return {false, "Inventory changes not available"};

        // Force the engine to create a live InventoryEntryData (with its own
        // engine-managed ExtraDataList) when the item only lives in the base
        // TESContainer.  This avoids handing a hand-rolled ExtraDataList to
        // the engine, which crashes on AE 1.6.629+ where BaseExtraList has a
        // virtual destructor and non-trivial layout.
        auto* liveEntry = Common::MaterializeInventoryEntry(player, form);
        logger::trace("favorite 0x{:08X}: liveEntry={}", formId,
                      static_cast<const void*>(liveEntry));
        if (!liveEntry)
            return {false, "Failed to materialize inventory entry for item"};

        // Pick an ExtraDataList owned by the engine, if any.  SetFavorite
        // attaches an ExtraHotkey to it; when xList is null the engine
        // allocates one itself (safe path — never pass a hand-rolled xList).
        RE::ExtraDataList* xList = nullptr;
        if (liveEntry->extraLists) {
            for (auto* xl : *liveEntry->extraLists) {
                if (xl) {
                    xList = xl;
                    break;
                }
            }
        }
        logger::trace("favorite 0x{:08X}: selected xList={} extraListsPtr={}",
                      formId,
                      static_cast<const void*>(xList),
                      static_cast<const void*>(liveEntry->extraLists));

        const bool wasFavorited = liveEntry->IsFavorited();
        logger::trace("favorite 0x{:08X}: wasFavorited={}", formId, wasFavorited);

        if (wasFavorited) {
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
}