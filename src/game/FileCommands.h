#pragma once

#include "Common.h"

#include <string>

namespace FileCommands
{
    using Common::CommandResult;

    // Produce a base64 PNG preview of a raw DDS texture path (relative to the
    // game Data folder, e.g. "textures/interface/icons/weapons/ironsword.dds").
    // This is the generic primitive for any texture asset — item icons, map
    // marker icons, book art, etc. Returns:
    //   { "mimeType": "image/png", "width": int, "height": int, "imageBase64": string }
    // Heavy (file read + DDS decode + PNG encode + base64): call from the
    // io_context thread, NOT the game thread, to avoid freezing the game.
    CommandResult GetTexturePreview(const std::string& path);

    // Read an arbitrary file from the game's virtual filesystem (BSA + loose
    // files) and return it as base64. `path` is relative to the game Data
    // folder. Returns:
    //   { "mimeType": string, "size": int, "dataBase64": string }
    // Heavy (file read + base64): call from the io_context thread.
    CommandResult GetFileDownload(const std::string& path);
}
