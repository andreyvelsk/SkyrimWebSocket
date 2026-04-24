#include "GameWriter.h"
#include "../Utils.h"

#include <algorithm>
#include <format>

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
    [[maybe_unused]] static RE::ExtraDataList* FindWornExtraDataList(RE::InventoryEntryData* entry, bool leftHand)
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
    [[maybe_unused]] static RE::ExtraDataList* FindUnwornExtraDataList(RE::InventoryEntryData* entry)
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
    [[maybe_unused]] static void DoUnequipWeapon(RE::ActorEquipManager* equipMgr,
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

    // Dispatches a Papyrus method call on the player.  Returns true when the
    // call was queued on the VM.  Delegating to Papyrus lets the engine run
    // the same equip/unequip/drop logic that the vanilla UI uses — no more
    // hand-rolled hand-swapping, ExtraDataList hunting, or 2H↔1H corner cases.
    template <typename... Args>
    static bool DispatchPlayerMethod(RE::PlayerCharacter* player,
                                     const char*          className,
                                     const char*          methodName,
                                     Args&&...            args)
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            logger::error("Papyrus VM unavailable");
            return false;
        }
        auto* policy = vm->GetObjectHandlePolicy();
        if (!policy) {
            logger::error("Papyrus VM handle policy unavailable");
            return false;
        }
        const auto handle = policy->GetHandleForObject(
            static_cast<RE::VMTypeID>(player->GetFormType()), player);

        auto* fnArgs = RE::MakeFunctionArguments(std::forward<Args>(args)...);
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        return vm->DispatchMethodCall(handle,
                                      RE::BSFixedString(className),
                                      RE::BSFixedString(methodName),
                                      fnArgs,
                                      callback);
    }

    // Papyrus Actor.EquipItemEx aiEquipSlot values.
    static constexpr std::int32_t kEquipSlotRightHand = 0;  // also default for non-weapons
    static constexpr std::int32_t kEquipSlotLeftHand  = 1;
    static constexpr std::int32_t kEquipSlotBothHands = 2;

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

        const int32_t itemCount = GetInventoryCount(player, formId);
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
        std::int32_t slotArg = kEquipSlotRightHand;
        if (ft == RE::FormType::Weapon) {
            const auto* weap = form->As<RE::TESObjectWEAP>();
            const bool  twoHanded = weap && IsWeaponTwoHanded(weap->GetWeaponType());
            if (twoHanded && hand == "left")
                return {false, "Two-handed weapon can only be equipped in the right hand"};
            if (twoHanded)
                slotArg = kEquipSlotBothHands;
            else if (hand == "left")
                slotArg = kEquipSlotLeftHand;
        }
        // For armor and ammo aiEquipSlot is ignored by the engine, so leave
        // it at the right-hand default.

        logger::trace("EquipItem 0x{:08X}: dispatching Actor.EquipItemEx slot={}", formId, slotArg);
        const bool ok = DispatchPlayerMethod(
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

        if (GetInventoryCount(player, formId) <= 0)
            return {false, "Item not in inventory"};

        // For weapons we select the specific hand slot via UnequipItemEx.
        // For everything else we call UnequipItem (slot is irrelevant and
        // the engine will unequip wherever the item is currently worn).
        const auto ft = form->GetFormType();
        bool ok = false;
        if (ft == RE::FormType::Weapon) {
            std::int32_t slotArg = (hand == "left") ? kEquipSlotLeftHand : kEquipSlotRightHand;
            logger::trace("UnequipItem 0x{:08X}: dispatching Actor.UnequipItemEx slot={}", formId, slotArg);
            ok = DispatchPlayerMethod(
                player, "Actor", "UnequipItemEx",
                static_cast<RE::TESForm*>(form),
                slotArg,
                /*abPreventEquipping=*/false);
        } else {
            logger::trace("UnequipItem 0x{:08X}: dispatching Actor.UnequipItem", formId);
            ok = DispatchPlayerMethod(
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

        const int32_t invCnt = GetInventoryCount(player, formId);
        logger::trace("UseItem 0x{:08X} ('{}') invCount={} formType={}",
                      formId, form->GetName(), invCnt, static_cast<int>(form->GetFormType()));
        if (invCnt <= 0)
            return {false, "Item not in inventory"};

        if (!IsConsumable(form->GetFormType()))
            return {false, "Item is not consumable (use 'equip' for weapons/apparel)"};

        // Actor.EquipItem on a potion/food/scroll triggers consumption (or
        // equip-for-casting for scrolls), mirroring vanilla UI behaviour.
        const bool ok = DispatchPlayerMethod(
            player, "Actor", "EquipItem",
            static_cast<RE::TESForm*>(form),
            /*abPreventRemoval=*/false,
            /*abSilent=*/false);
        if (!ok)
            return {false, "Papyrus dispatch failed"};

        logger::debug("use 0x{:08X} ('{}')", formId, form->GetName());
        PrintConsole("[WS] Use " + std::string(form->GetName()));
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

        const int32_t have = GetInventoryCount(player, formId);
        if (have <= 0)
            return {false, "Item not in inventory"};
        if (count > have)
            return {false, "Not enough items (have " + std::to_string(have) + ")"};

        // ObjectReference.DropObject is the Papyrus equivalent of the vanilla
        // drop-from-inventory action (animation, sound, physics).
        const bool ok = DispatchPlayerMethod(
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

        const int32_t count = GetInventoryCount(player, formId);
        logger::debug("favorite 0x{:08X} ('{}') invCount={}", formId, form->GetName(), count);

        if (count <= 0)
            return {false, "Item not in inventory"};

        auto* invChanges = player->GetInventoryChanges();
        logger::trace("favorite 0x{:08X}: invChanges={}", formId, static_cast<const void*>(invChanges));
        if (!invChanges)
            return {false, "Inventory changes not available"};

        // Items that live only in the player's base TESContainer (e.g. starting
        // iron dagger, freshly levelled gear) are visible via GetInventory() but
        // have no live InventoryEntryData in InventoryChanges::entryList — the
        // engine lazily allocates entries only on mutation.  Force the engine
        // to create a proper entry (with its own ExtraDataList managed by the
        // engine) by doing a neutral +1/-1 container transaction.  This way we
        // never hand a hand-rolled ExtraDataList to the engine, which avoids
        // crashes on AE 1.6.629+ where BaseExtraList has a virtual destructor
        // and non-trivial layout.
        auto* liveEntry = FindLiveEntry(player, formId);
        logger::trace("favorite 0x{:08X}: initial FindLiveEntry={}", formId,
                      static_cast<const void*>(liveEntry));
        if (!liveEntry) {
            logger::trace("favorite 0x{:08X}: forcing entry creation via AddObjectToContainer/RemoveItem", formId);
            player->AddObjectToContainer(form, nullptr, 1, nullptr);
            player->RemoveItem(form, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);

            liveEntry = FindLiveEntry(player, formId);
            logger::trace("favorite 0x{:08X}: after force, FindLiveEntry={}", formId,
                          static_cast<const void*>(liveEntry));
            if (!liveEntry)
                return {false, "Failed to materialize inventory entry for item"};
        }

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

    CommandResult EquipSpell(RE::FormID formId, const std::string& hand)
    {
        logger::trace("EquipSpell enter: formId=0x{:08X} hand='{}'", formId, hand);
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

    // ─── Hotkeys ──────────────────────────────────────────────────────────

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

    // Returns true if the player currently knows the given spell/shout/power.
    static bool PlayerKnowsSpell(RE::PlayerCharacter* player, RE::SpellItem* spell)
    {
        if (!player || !spell)
            return false;

        // Base NPC spell list (racial abilities etc.)
        if (auto* npc = player->GetActorBase()) {
            if (auto* spellData = npc->GetSpellList()) {
                for (std::uint32_t i = 0; i < spellData->numSpells; ++i) {
                    if (spellData->spells[i] == spell)
                        return true;
                }
            }
        }
        // Spells added at runtime (tomes, AddSpell, powers).
        for (auto* s : player->GetActorRuntimeData().addedSpells) {
            if (s == spell)
                return true;
        }
        return false;
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

    // Returns the magic favorite currently bound to a slot (0..7), or nullptr.
    static RE::TESForm* GetMagicHotkey(RE::MagicFavorites* favorites, std::uint8_t slotIdx)
    {
        if (!favorites || slotIdx >= favorites->hotkeys.size())
            return nullptr;
        return favorites->hotkeys[slotIdx];
    }

    // Result of scanning the inventory for a hotkey binding.
    struct ItemHotkeyRef
    {
        RE::InventoryEntryData* entry   = nullptr;
        RE::ExtraDataList*      xList   = nullptr;
        RE::ExtraHotkey*        extra   = nullptr;
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

        // Path A: spell/shout/power hotkey.
        if (auto* spell = form->As<RE::SpellItem>()) {
            if (!IsHotkeyableSpell(spell))
                return {false, "Spell type cannot be hotkeyed"};
            if (!PlayerKnowsSpell(player, spell))
                return {false, "Spell not known by player"};

            // Clear any previous binding for this slot (magic or item).
            ClearSlotBinding(player, favorites, slotIdx);

            // The game shows only favorited forms in the hotkey overlay, so
            // ensure it is favorited first.  SetFavorite is idempotent.
            if (std::find(favorites->spells.begin(), favorites->spells.end(),
                          static_cast<RE::TESForm*>(spell)) == favorites->spells.end()) {
                favorites->SetFavorite(spell);
            }
            favorites->hotkeys[slotIdx] = spell;

            PrintConsole(std::format("[WS] Hotkey {} <- spell {}", slot, spell->GetName()));
            return {true, ""};
        }

        // Path B: inventory item hotkey.
        auto* bound = form->As<RE::TESBoundObject>();
        if (!bound)
            return {false, "Form is neither a spell nor a bound object"};

        auto* liveEntry = FindLiveEntry(player, formId);
        if (!liveEntry || GetInventoryCount(player, formId) <= 0)
            return {false, "Item not in inventory"};

        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges)
            return {false, "Inventory changes not available"};

        // Clear any previous binding for this slot (magic or different item).
        ClearSlotBinding(player, favorites, slotIdx);

        // Locate any existing ExtraHotkey on this item so we can mutate it
        // in place (items can only carry one ExtraHotkey).
        RE::ExtraDataList* targetXList = nullptr;
        RE::ExtraHotkey*   targetExtra = nullptr;
        if (liveEntry->extraLists) {
            for (auto* xl : *liveEntry->extraLists) {
                if (!xl) continue;
                if (auto* xh = xl->GetByType<RE::ExtraHotkey>()) {
                    targetXList = xl;
                    targetExtra = xh;
                    break;
                }
            }
        }

        // If the item is not favorited yet, call SetFavorite to allocate an
        // ExtraDataList with a fresh ExtraHotkey (kUnbound) attached.
        if (!targetExtra) {
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
                        targetXList = xl;
                        targetExtra = xh;
                        break;
                    }
                }
            }
        }

        if (!targetExtra)
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

        auto* handler = mc->favoritesHandler.get();
        const bool handled = handler->ProcessButton(event);

        // ButtonEvent::Create allocates via the game's heap; release it.
        RE::free(event);

        PrintConsole(std::format("[WS] Hotkey {} triggered (handled={})",
                                 slot, handled ? "true" : "false"));
        return {true, ""};
    }
}
