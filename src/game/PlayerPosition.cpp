#include "PlayerPosition.h"
#include "Common.h"

#include "../../logger.h"

namespace PlayerPosition
{
    RE::TESWorldSpace* ResolvePlayerWorldspace()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nullptr;

        auto* world = player->GetWorldspace();
        if (world)
            return world;

        auto* cell = player->GetParentCell();
        if (!cell)
            return nullptr;

        // A - cell's own worldSpace (may be set for some interiors).
        world = cell->GetRuntimeData().worldSpace;
        if (world)
            return world;

        // B - ExtraPersistentCell on the player.
        if (auto* xPersist = player->extraList.GetByType<RE::ExtraPersistentCell>()) {
            if (xPersist->persistentCell) {
                world = xPersist->persistentCell->GetRuntimeData().worldSpace;
                if (world)
                    return world;
            }
        }

        // C - walk the location hierarchy via worldLocMarker.
        RE::BGSLocation* loc = cell->GetLocation();
        while (loc) {
            auto* markerRef = loc->worldLocMarker.get().get();
            if (markerRef) {
                world = markerRef->GetWorldspace();
                if (world)
                    return world;
            }
            loc = loc->parentLoc;
        }

        // D - TES::worldSpace (the game's own tracked current worldspace).
        if (auto* tes = RE::TES::GetSingleton()) {
            world = tes->GetRuntimeData2().worldSpace;
            if (world)
                return world;
        }

        // E - brute-force scan of all worldspaces.
        if (auto* dh = RE::TESDataHandler::GetSingleton()) {
            const auto& worlds = dh->GetFormArray<RE::TESWorldSpace>();

            // E1 - match by ExtraPersistentCell::persistentCell pointer.
            if (auto* xPersist = player->extraList.GetByType<RE::ExtraPersistentCell>()) {
                if (xPersist->persistentCell) {
                    for (auto* ws : worlds) {
                        if (ws && ws->persistentCell == xPersist->persistentCell) {
                            return ws;
                        }
                    }
                }
            }

            // E2 - match by location in worldspace's locationMap.
            for (auto* curLoc = cell->GetLocation(); curLoc; curLoc = curLoc->parentLoc) {
                const RE::FormID locId = curLoc->GetFormID();
                if (!locId)
                    continue;
                for (auto* ws : worlds) {
                    if (ws && ws->locationMap.contains(locId)) {
                        return ws;
                    }
                }
            }
        }

        return nullptr;
    }

    nlohmann::json ReadPosition()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::object();

        nlohmann::json pos;
        pos["x"]     = player->GetPositionX();
        pos["y"]     = player->GetPositionY();
        pos["z"]     = player->GetPositionZ();
        pos["angle"] = player->GetAngleZ();

        auto* world = player->GetWorldspace();
        if (world) {
            Common::BuildWorldspaceFields(pos, world);
        } else {
            // Interior cell — worldspace is null, but we can still resolve
            // the parentWorldspace via the same fallback chain used by the
            // map-markers query.
            pos["worldspace"]       = nullptr;
            pos["worldspaceFormId"] = nullptr;

            if (auto* resolved = ResolvePlayerWorldspace()) {
                Common::BuildWorldspaceFields(pos, resolved);
                // Keep worldspace fields null — only parentWorldspace is meaningful.
                pos["worldspace"]       = nullptr;
                pos["worldspaceFormId"] = nullptr;
            } else {
                pos["parentWorldspace"]       = nullptr;
                pos["parentWorldspaceFormId"] = nullptr;
            }
        }

        auto* cell = player->GetParentCell();
        if (cell) {
            const char* cedid = cell->GetFormEditorID();
            pos["cell"]       = cedid ? std::string(cedid) : std::string();
            pos["cellFormId"] = Common::FormIdToString(cell->GetFormID());
            pos["isInterior"] = cell->IsInteriorCell();
        } else {
            pos["cell"]       = nullptr;
            pos["cellFormId"] = nullptr;
            pos["isInterior"] = false;
        }

        return pos;
    }

    nlohmann::json ReadExteriorPosition()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::object();

        nlohmann::json out;
        auto* world = player->GetWorldspace();
        auto* cell  = player->GetParentCell();

        // Player is in a top-level exterior worldspace (Tamriel, Solstheim, etc.) —
        // return live coordinates directly.
        if (world && !world->parentWorld && cell && !cell->IsInteriorCell()) {
            out["x"] = player->GetPositionX();
            out["y"] = player->GetPositionY();
            out["z"] = player->GetPositionZ();
            Common::BuildWorldspaceFields(out, world);
            return out;
        }

        // Player is in an interior cell or a city sub-worldspace.
        // Resolve the BGSLocation's world-map marker reference.
        RE::BGSLocation*   loc       = cell ? cell->GetLocation() : nullptr;
        RE::TESObjectREFR* markerRef = nullptr;
        while (loc && !markerRef) {
            markerRef = loc->worldLocMarker.get().get();
            if (!markerRef)
                loc = loc->parentLoc;
        }

        if (markerRef) {
            out["x"] = markerRef->GetPositionX();
            out["y"] = markerRef->GetPositionY();
            out["z"] = markerRef->GetPositionZ();
            Common::BuildWorldspaceFields(out, markerRef->GetWorldspace());
        } else {
            out["x"] = nullptr;
            out["y"] = nullptr;
            out["z"] = nullptr;
            Common::BuildWorldspaceFields(out, nullptr);
        }

        return out;
    }

    RE::TESObjectREFR* GetPlayerMarkerRef()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nullptr;

        auto& info = player->GetInfoRuntimeData();
        return info.playerMapMarker.get().get();
    }

    nlohmann::json BuildPlayerMarkerJson(RE::TESObjectREFR* ref)
    {
        nlohmann::json out;

        if (!ref) {
            out["isSet"] = false;
            out["x"]                      = nullptr;
            out["y"]                      = nullptr;
            out["z"]                      = nullptr;
            out["worldspace"]             = nullptr;
            out["worldspaceFormId"]       = nullptr;
            out["parentWorldspace"]       = nullptr;
            out["parentWorldspaceFormId"] = nullptr;
            return out;
        }

        auto*      extra     = ref->extraList.GetByType<RE::ExtraMapMarker>();
        const bool hasData   = extra && extra->mapData;
        const bool isVisible = hasData && extra->mapData->flags.any(RE::MapMarkerData::Flag::kVisible);

        out["isSet"] = isVisible;
        out["x"]     = ref->GetPositionX();
        out["y"]     = ref->GetPositionY();
        out["z"]     = ref->GetPositionZ();

        if (auto* world = ref->GetWorldspace()) {
            Common::BuildWorldspaceFields(out, world);
        } else {
            out["worldspace"]             = nullptr;
            out["worldspaceFormId"]       = nullptr;
            out["parentWorldspace"]       = nullptr;
            out["parentWorldspaceFormId"] = nullptr;
        }

        return out;
    }

    nlohmann::json ReadPlayerMarker()
    {
        return BuildPlayerMarkerJson(GetPlayerMarkerRef());
    }
}