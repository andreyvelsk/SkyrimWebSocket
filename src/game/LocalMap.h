#pragma once

#include "Common.h"

// Floor plan of the player's surroundings, built from the navmesh (the
// walkable surface the AI uses). Interiors: the current cell. Exteriors:
// the player's cell and its eight neighbours.
namespace LocalMap
{
    using Common::CommandResult;

    // local_map_get. Game thread. data:
    //   { key, isInterior, name, cellFormId, worldspace,
    //     minX, minY, minZ, maxX, maxY, maxZ,
    //     vertexCount, triangleCount, truncated,
    //     vertices:  base64, Int32 LE x,y,z per vertex (world units),
    //     triangles: base64, Uint32 LE a,b,c per triangle,
    //     edges:     base64, Uint8 per triangle; bit i = edge i (v[i]→v[i+1])
    //                has no neighbour (a wall or drop),
    //     doors:     [ { x, y, z, name } ]  load doors and where they lead }
    // key: "c:<cell formId>" inside, "w:<worldspace formId>:<cellX>:<cellY>"
    // outside. Clients re-request when their key for the player changes.
    CommandResult Read();
}
