#include "LocalMap.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <format>
#include <limits>
#include <string>
#include <vector>

#include "../../logger.h"

namespace LocalMap
{
    namespace
    {
        // A large exterior 3×3 block stays well under this; the cap only
        // guards against odd modded cells.
        constexpr std::size_t kMaxTriangles = 150000;
        constexpr std::uint16_t kNoNeighbour = 0xFFFF;

        struct Builder
        {
            std::vector<std::int32_t>  vertices;   // x, y, z
            std::vector<std::uint32_t> triangles;  // a, b, c
            std::vector<std::uint8_t>  edges;      // open-edge bits per triangle
            float minX = std::numeric_limits<float>::max();
            float minY = std::numeric_limits<float>::max();
            float minZ = std::numeric_limits<float>::max();
            float maxX = std::numeric_limits<float>::lowest();
            float maxY = std::numeric_limits<float>::lowest();
            float maxZ = std::numeric_limits<float>::lowest();
            bool  truncated = false;

            void Add(const RE::BSNavmesh& a_mesh)
            {
                const auto vertexCount = a_mesh.vertices.size();
                if (vertexCount == 0)
                    return;
                const auto base = static_cast<std::uint32_t>(vertices.size() / 3);
                for (const auto& v : a_mesh.vertices) {
                    const auto& p = v.location;
                    vertices.push_back(static_cast<std::int32_t>(std::lround(p.x)));
                    vertices.push_back(static_cast<std::int32_t>(std::lround(p.y)));
                    vertices.push_back(static_cast<std::int32_t>(std::lround(p.z)));
                    minX = std::min(minX, p.x);
                    minY = std::min(minY, p.y);
                    minZ = std::min(minZ, p.z);
                    maxX = std::max(maxX, p.x);
                    maxY = std::max(maxY, p.y);
                    maxZ = std::max(maxZ, p.z);
                }

                using Flag = RE::BSNavmeshTriangle::TriangleFlag;
                constexpr Flag kLinks[3] = { Flag::kEdge0_Link, Flag::kEdge1_Link, Flag::kEdge2_Link };
                const auto triangleCount = a_mesh.triangles.size();
                for (const auto& t : a_mesh.triangles) {
                    if (triangles.size() / 3 >= kMaxTriangles) {
                        truncated = true;
                        return;
                    }
                    if (t.triangleFlags.all(Flag::kDeleted))
                        continue;
                    if (t.vertices[0] >= vertexCount || t.vertices[1] >= vertexCount || t.vertices[2] >= vertexCount)
                        continue;
                    std::uint8_t open = 0;
                    for (int i = 0; i < 3; ++i) {
                        const bool hasNeighbour = t.triangles[i] != kNoNeighbour && t.triangles[i] < triangleCount;
                        // A link edge continues into the next navmesh (cell border).
                        if (!hasNeighbour && !t.triangleFlags.all(kLinks[i]))
                            open |= static_cast<std::uint8_t>(1u << i);
                    }
                    triangles.push_back(base + t.vertices[0]);
                    triangles.push_back(base + t.vertices[1]);
                    triangles.push_back(base + t.vertices[2]);
                    edges.push_back(open);
                }
            }

            void AddCell(RE::TESObjectCELL* a_cell)
            {
                if (!a_cell)
                    return;
                auto* array = a_cell->GetRuntimeData().navMeshes;
                if (!array)
                    return;
                for (const auto& mesh : array->navMeshes) {
                    if (mesh)
                        Add(*mesh);
                }
            }
        };

        template <class T>
        std::string ToBase64(const std::vector<T>& a_values)
        {
            // x86 is little-endian: the bytes are already in wire order.
            return Common::Base64Encode(reinterpret_cast<const std::uint8_t*>(a_values.data()),
                                        a_values.size() * sizeof(T));
        }

        std::string DestinationName(RE::TESObjectREFR* a_linked)
        {
            if (!a_linked)
                return {};
            if (auto* cell = a_linked->GetParentCell(); cell && cell->IsInteriorCell()) {
                const char* name = cell->GetFullName();
                if (name && *name)
                    return name;
            }
            if (auto* world = a_linked->GetWorldspace()) {
                const char* name = world->GetFullName();
                if (name && *name)
                    return name;
            }
            return {};
        }

        void CollectDoors(RE::TESObjectCELL* a_cell, nlohmann::json& a_out)
        {
            if (!a_cell)
                return;
            a_cell->ForEachReference([&](RE::TESObjectREFR* ref) {
                if (!ref || ref->IsDisabled() || ref->IsDeleted())
                    return RE::BSContainer::ForEachResult::kContinue;
                auto* base = ref->GetBaseObject();
                if (!base || base->GetFormType() != RE::FormType::Door)
                    return RE::BSContainer::ForEachResult::kContinue;
                auto* teleport = ref->extraList.GetByType<RE::ExtraTeleport>();
                if (!teleport || !teleport->teleportData)
                    return RE::BSContainer::ForEachResult::kContinue;
                const auto linked = teleport->teleportData->linkedDoor.get();
                const auto pos = ref->GetPosition();
                a_out.push_back({
                    { "x", std::lround(pos.x) },
                    { "y", std::lround(pos.y) },
                    { "z", std::lround(pos.z) },
                    { "name", DestinationName(linked.get()) },
                });
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }
    }

    CommandResult Read()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* cell   = player ? player->GetParentCell() : nullptr;
        if (!cell)
            return { false, "Player has no cell" };

        Builder        builder;
        nlohmann::json doors = nlohmann::json::array();
        nlohmann::json data;

        if (cell->IsInteriorCell()) {
            builder.AddCell(cell);
            CollectDoors(cell, doors);
            const char* name   = cell->GetFullName();
            data["key"]        = std::format("c:{:08X}", cell->GetFormID());
            data["isInterior"] = true;
            data["name"]       = name ? name : "";
            data["worldspace"] = nullptr;
        } else {
            auto* coords = cell->GetCoordinates();
            auto* world  = player->GetWorldspace();
            if (!coords || !world)
                return { false, "Exterior cell without coordinates" };
            const int cx = coords->cellX;
            const int cy = coords->cellY;
            if (auto* tes = RE::TES::GetSingleton()) {
                tes->ForEachCell([&](RE::TESObjectCELL* c) {
                    if (!c || !c->IsExteriorCell() || !c->IsAttached())
                        return;
                    auto* cc = c->GetCoordinates();
                    if (!cc || std::abs(cc->cellX - cx) > 1 || std::abs(cc->cellY - cy) > 1)
                        return;
                    builder.AddCell(c);
                    CollectDoors(c, doors);
                });
            }
            const char* name   = world->GetFullName();
            data["key"]        = std::format("w:{:08X}:{}:{}", world->GetFormID(), cx, cy);
            data["isInterior"] = false;
            data["name"]       = name ? name : "";
            const char* editorId = world->GetFormEditorID();
            data["worldspace"] = editorId ? editorId : "";
        }

        const auto triangleCount = builder.triangles.size() / 3;
        if (triangleCount == 0)
            return { false, "No navmesh here" };

        data["cellFormId"]    = std::format("0x{:08X}", cell->GetFormID());
        data["minX"]          = std::lround(builder.minX);
        data["minY"]          = std::lround(builder.minY);
        data["minZ"]          = std::lround(builder.minZ);
        data["maxX"]          = std::lround(builder.maxX);
        data["maxY"]          = std::lround(builder.maxY);
        data["maxZ"]          = std::lround(builder.maxZ);
        data["vertexCount"]   = builder.vertices.size() / 3;
        data["triangleCount"] = triangleCount;
        data["truncated"]     = builder.truncated;
        data["vertices"]      = ToBase64(builder.vertices);
        data["triangles"]     = ToBase64(builder.triangles);
        data["edges"]         = ToBase64(builder.edges);
        data["doors"]         = std::move(doors);

        logger::debug("[LocalMap] {} triangles, {} vertices", triangleCount, builder.vertices.size() / 3);
        return { true, "", std::move(data) };
    }
}
