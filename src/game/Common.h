#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <cctype>
#include <cstdint>
#include <format>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace Common
{
    // ─── Shared command result ─────────────────────────────────────────────

    struct CommandResult
    {
        bool           success;
        std::string    error;  // empty on success
        nlohmann::json data;   // optional result payload (null when absent)
    };

    // ─── Papyrus dispatch helpers (templates, must be in header) ───────────

    // Dispatch a Papyrus method call on the player reference.
    // Returns true when the call was queued on the VM.
    template <typename... Args>
    inline bool DispatchPlayerMethod(RE::PlayerCharacter* player,
                                     const char*          className,
                                     const char*          methodName,
                                     Args...              args)
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            SKSE::log::error("Papyrus VM unavailable");
            return false;
        }
        auto* policy = vm->GetObjectHandlePolicy();
        if (!policy) {
            SKSE::log::error("Papyrus VM handle policy unavailable");
            return false;
        }
        const auto handle = policy->GetHandleForObject(
            static_cast<RE::VMTypeID>(player->GetFormType()), player);
        auto* fnArgs = RE::MakeFunctionArguments(std::move(args)...);
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        return vm->DispatchMethodCall(handle,
                                      RE::BSFixedString(className),
                                      RE::BSFixedString(methodName),
                                      fnArgs,
                                      callback);
    }

    // Dispatch a Papyrus method call on an arbitrary form.
    template <typename FormT, typename... Args>
    inline bool DispatchFormMethod(FormT*      form,
                                   const char* className,
                                   const char* methodName,
                                   Args...     args)
    {
        if (!form) return false;
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            SKSE::log::error("Papyrus VM unavailable");
            return false;
        }
        auto* policy = vm->GetObjectHandlePolicy();
        if (!policy) {
            SKSE::log::error("Papyrus VM handle policy unavailable");
            return false;
        }
        const auto handle = policy->GetHandleForObject(
            static_cast<RE::VMTypeID>(form->GetFormType()), form);
        auto* fnArgs = RE::MakeFunctionArguments(std::move(args)...);
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        return vm->DispatchMethodCall(handle,
                                      RE::BSFixedString(className),
                                      RE::BSFixedString(methodName),
                                      fnArgs,
                                      callback);
    }

    // ─── Shared game helpers ───────────────────────────────────────────────
    // Format a FormID as "0xXXXXXXXX".
    inline std::string FormIdToString(RE::FormID id)
    {
        return std::format("0x{:08X}", id);
    }

    // Look up a GameSetting string by key (e.g. "sSkillHeavyarmor").
    // Returns an empty string when the key does not exist or is not a string setting.
    std::string GetGMSTString(const char* key);

    // Walk TESWorldSpace::parentWorld chain to the root worldspace.
    // Returns the same pointer when world has no parent.
    RE::TESWorldSpace* ResolveWorldspaceRoot(RE::TESWorldSpace* world);

    // Write worldspace fields (worldspace, worldspaceFormId, parentWorldspace,
    // parentWorldspaceFormId) into a JSON object.  Sets all fields to nullptr
    // when world is null.
    void BuildWorldspaceFields(nlohmann::json& obj, RE::TESWorldSpace* world);

    // Convert a wide (UTF-16) string to UTF-8.
    // Handles the full Basic Multilingual Plane (U+0000..U+FFFF).
    std::string WcsToUtf8(const wchar_t* ws);

    // Convert a string to lowercase ASCII in-place.
    std::string ToLowerAscii(std::string value);

    // Case-insensitive ASCII comparison.
    bool EqualAsciiIgnoreCase(std::string_view lhs, std::string_view rhs);

    // Trim leading/trailing ASCII whitespace from a string_view.
    std::string_view TrimAscii(std::string_view value);

    // Encode raw bytes to a base64 string (RFC 4648, standard alphabet).
    std::string Base64Encode(const std::uint8_t* data, std::size_t len);

    // Returns the count of an item in the player's inventory, or 0 if not found.
    int32_t GetInventoryCount(RE::PlayerCharacter* player, RE::FormID formId);

    // Finds the live InventoryEntryData for a given formId from the player's
    // InventoryChanges. Returns nullptr if not found.
    RE::InventoryEntryData* FindLiveEntry(RE::PlayerCharacter* player, RE::FormID formId);

    // Returns the live InventoryEntryData for a form, forcing the engine to
    // allocate one via a neutral +1/-1 container transaction when the item
    // only lives in the player's base TESContainer (e.g. starting gear).
    // Returns nullptr when the entry cannot be materialized.
    RE::InventoryEntryData* MaterializeInventoryEntry(RE::PlayerCharacter* player,
                                                      RE::TESBoundObject*  form);

    // Returns true if the player currently knows the given spell/shout/power.
    bool PlayerKnowsSpell(RE::PlayerCharacter* player, RE::SpellItem* spell);

    // ─── Effect helpers ────────────────────────────────────────────────────────

    // Build a JSON object for a single magic effect using native game data.
    // Uses MagicSystem::GetMagicItemDescription for the resolved description
    // (same path the in-game UI uses). Magnitude is formatted to match the
    // vanilla inventory display: integer when whole, one decimal place otherwise.
    nlohmann::json BuildEffectJson(const RE::Effect* eff);

    // Build a JSON array of effects for a MagicItem (spell, enchantment, potion, scroll, etc.).
    nlohmann::json BuildEffectsArray(const RE::MagicItem* magic);
}