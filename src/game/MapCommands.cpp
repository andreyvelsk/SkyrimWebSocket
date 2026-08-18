#include "MapCommands.h"
#include "Common.h"
#include "PlayerPosition.h"
#include "../Utils.h"

#include <format>

namespace logger = SKSE::log;

namespace MapCommands
{
    // ─── Private helpers ───────────────────────────────────────────────────

    // Build a JSON payload describing a map-marker ref (matches the per-entry
    // shape produced by MapMarkers::ReadMapMarkers).
    static nlohmann::json BuildMarkerPayload(RE::TESObjectREFR* ref, RE::MapMarkerData* data)
    {
        using Flag = RE::MapMarkerData::Flag;
        nlohmann::json out;
        out["refId"]         = std::format("0x{:08X}", ref->GetFormID());
        const char* fullName = data->locationName.GetFullName();
        out["name"]          = fullName ? fullName : "";
        const auto typeId    = static_cast<std::uint32_t>(data->type.underlying());
        out["typeId"]        = typeId;
        out["x"]             = ref->GetPositionX();
        out["y"]             = ref->GetPositionY();
        out["isVisible"]     = data->flags.any(Flag::kVisible);
        out["canFastTravel"] = data->flags.any(Flag::kCanTravelTo);
        return out;
    }

    // A validated fast-travel destination, or an error describing why it is invalid.
    struct FastTravelTarget
    {
        RE::TESObjectREFR* ref   = nullptr;
        RE::MapMarkerData* data  = nullptr;
        std::string        error;  // non-empty on failure
    };

    // Resolve a formId into a travelable map-marker reference, enforcing the
    // marker-side pre-flight checks (existence, ExtraMapMarker, visibility,
    // fast-travel flag, disabled/deleted state, worldspace gate).
    static FastTravelTarget ResolveFastTravelTarget(RE::FormID formId)
    {
        FastTravelTarget target;

        auto* form = RE::TESForm::LookupByID(formId);
        if (!form) {
            logger::info("[FastTravel] step1: form lookup FAILED");
            target.error = std::format("No form with id 0x{:08X}", formId);
            return target;
        }
        logger::info("[FastTravel] step1: form ok, type={}", static_cast<int>(form->GetFormType()));

        auto* ref = form->As<RE::TESObjectREFR>();
        if (!ref) {
            target.error = std::format("Form 0x{:08X} is not a reference", formId);
            return target;
        }

        auto* extra = ref->extraList.GetByType<RE::ExtraMapMarker>();
        if (!extra || !extra->mapData) {
            target.error = std::format("Reference 0x{:08X} is not a map marker", formId);
            return target;
        }

        auto* data = extra->mapData;
        using Flag = RE::MapMarkerData::Flag;

        if (!data->flags.any(Flag::kVisible)) {
            target.error = "Marker is not discovered yet (cannot fast-travel to a hidden marker)";
            return target;
        }
        if (!data->flags.any(Flag::kCanTravelTo)) {
            target.error = "Marker is flagged as non-fast-travel (canFastTravel=false)";
            return target;
        }
        if (ref->IsDisabled()) {
            target.error = "Marker reference is disabled";
            return target;
        }
        if (ref->IsDeleted()) {
            target.error = "Marker reference is deleted";
            return target;
        }
        logger::info("[FastTravel] step3: marker flags & state ok");

        // Worldspace gate: some worldspaces (DLC interiors, etc.) forbid
        // fast travel entirely.
        if (auto* parentCell = ref->GetParentCell()) {
            if (auto* world = parentCell->GetRuntimeData().worldSpace) {
                using WFlag = RE::TESWorldSpace::Flag;
                if (world->flags.any(WFlag::kCantFastTravel)) {
                    target.error = "Marker's worldspace forbids fast travel";
                    return target;
                }
                logger::info("[FastTravel] step4: target worldspace='{}' editorId='{}'",
                             world->GetName() ? world->GetName() : "<null>",
                             world->GetFormEditorID() ? world->GetFormEditorID() : "<null>");
            }
        }

        target.ref  = ref;
        target.data = data;
        return target;
    }

    // Enforce the player-side fast-travel pre-flight checks (combat, interior
    // kCanTravelFromHere, exterior worldspace kCantFastTravel).  Returns an
    // empty string when the player is allowed to travel.
    static std::string ValidatePlayerCanTravel(RE::PlayerCharacter* player)
    {
        if (player->IsInCombat())
            return "Cannot fast-travel while in combat";
        logger::info("[FastTravel] step5: player ok, inCombat=false");

        auto* playerCell = player->GetParentCell();
        if (!playerCell)
            return "Cannot determine player's current cell";

        if (playerCell->IsInteriorCell()) {
            using CellFlag = RE::TESObjectCELL::Flag;
            if (!playerCell->cellFlags.any(CellFlag::kCanTravelFromHere)) {
                RE::SendHUDMessage::ShowHUDMessage(
                    "Cannot fast travel from this location", nullptr, true);
                return "Cannot fast-travel from this location";
            }
        } else if (auto* playerWorld = player->GetWorldspace()) {
            using WFlag = RE::TESWorldSpace::Flag;
            if (playerWorld->flags.any(WFlag::kCantFastTravel)) {
                RE::SendHUDMessage::ShowHUDMessage(
                    "Cannot fast travel from this location", nullptr, true);
                return "Cannot fast-travel from this location";
            }
            logger::info("[FastTravel] step5b: player worldspace='{}' editorId='{}'",
                         playerWorld->GetName() ? playerWorld->GetName() : "<null>",
                         playerWorld->GetFormEditorID() ? playerWorld->GetFormEditorID() : "<null>");
        }
        return {};
    }

    // ─── Commands ─────────────────────────────────────────────────────────

    CommandResult SetPlayerMarker(float a_x, float a_y, float a_z)
    {
        auto* ref = PlayerPosition::GetPlayerMarkerRef();
        if (!ref) {
            return {false,
                    "Player marker ref not initialized — open the world map at least once before placing a marker"};
        }

        // Make the marker visible (and fast-travel-enabled, which matches
        // vanilla behavior when the player drops a marker on the world map).
        // The ref + its ExtraMapMarker / MapMarkerData are pre-created by
        // the engine; we just toggle the bits and move the ref.
        if (auto* extra = ref->extraList.GetByType<RE::ExtraMapMarker>(); extra && extra->mapData) {
            using Flag = RE::MapMarkerData::Flag;
            extra->mapData->flags.set(Flag::kVisible);
            extra->mapData->flags.set(Flag::kCanTravelTo);
        }

        ref->SetPosition(a_x, a_y, a_z);

        CommandResult result;
        result.success = true;
        result.data    = PlayerPosition::BuildPlayerMarkerJson(ref);
        PrintConsole(std::format("[WS] Player marker set to ({:.1f}, {:.1f}, {:.1f})",
                                 a_x, a_y, a_z));
        return result;
    }

    CommandResult ClearPlayerMarker()
    {
        auto* ref = PlayerPosition::GetPlayerMarkerRef();
        if (!ref) {
            // No ref means there is no marker to clear — return the
            // canonical "not set" payload as a successful no-op.
            CommandResult result;
            result.success = true;
            result.data    = PlayerPosition::BuildPlayerMarkerJson(nullptr);
            return result;
        }

        if (auto* extra = ref->extraList.GetByType<RE::ExtraMapMarker>(); extra && extra->mapData) {
            using Flag = RE::MapMarkerData::Flag;
            extra->mapData->flags.reset(Flag::kVisible);
            extra->mapData->flags.reset(Flag::kCanTravelTo);
        }

        CommandResult result;
        result.success = true;
        result.data    = PlayerPosition::BuildPlayerMarkerJson(ref);
        PrintConsole("[WS] Player marker cleared");
        return result;
    }

    CommandResult FastTravelToMarker(RE::FormID formId)
    {
        logger::info("[FastTravel] BEGIN formId=0x{:08X}", formId);

        auto target = ResolveFastTravelTarget(formId);
        if (!target.error.empty())
            return {false, target.error};

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {false, "Player singleton unavailable"};

        const std::string playerError = ValidatePlayerCanTravel(player);
        if (!playerError.empty())
            return {false, playerError};

        // Trigger a *real* fast travel via Papyrus `Game.FastTravel(akMarker)`.
        // Unlike `player.moveto`, this dispatches into the engine's full
        // fast-travel pipeline: fade animation, in-game time advancement,
        // random-encounter rolls, weather reset, autosave, follower transfer
        // and the `PlayerFlags::fastTraveling` bit.  The Papyrus static is
        // implemented natively in the runtime, so this works cross-runtime
        // (SE/AE/VR/GOG) without depending on Address Library IDs.
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm)
            return {false, "Papyrus VM unavailable"};

        auto* fnArgs = RE::MakeFunctionArguments(static_cast<RE::TESObjectREFR*>(target.ref));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        const bool dispatched = vm->DispatchStaticCall(RE::BSFixedString("Game"),
                                                       RE::BSFixedString("FastTravel"),
                                                       fnArgs,
                                                       callback);
        if (!dispatched)
            return {false, "Failed to dispatch Game.FastTravel via Papyrus VM"};
        logger::info("[FastTravel] step6: Game.FastTravel dispatched");

        CommandResult result;
        result.success = true;
        result.data    = BuildMarkerPayload(target.ref, target.data);
        PrintConsole(std::format("[WS] Fast-travel to 0x{:08X} ({})",
                                 formId,
                                 result.data.value("name", std::string{"unnamed"})));
        logger::info("[FastTravel] END success");
        return result;
    }
}