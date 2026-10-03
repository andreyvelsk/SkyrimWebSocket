#pragma once

#include "Common.h"

#include <cstddef>
#include <cstdint>
#include <string>

// In-game screenshots through the engine's own screenshot feature
// (the PrintScreen handler). The engine writes files named after
// `sScreenShotBaseName:Display` (default "ScreenShot") into the game folder.
namespace ScreenshotCommands
{
    using Common::CommandResult;

    // Game thread. Sets `bAllowScreenShot:Display` for this run and queues one
    // screenshot, exactly as if the player pressed PrintScreen. The file
    // appears a frame or two later. Returns { "queued": bool }.
    CommandResult TakeScreenshot();

    // Any thread (file system only). Screenshot files in the game folder,
    // newest first:
    //   { "directory": string, "baseName": string,
    //     "files": [ { "name": string, "size": int, "modified": int (unix s) } ] }
    CommandResult ListScreenshots(std::size_t limit);

    // io_context thread (file read + optional decode/resize/JPEG encode).
    // `name` must be one of the names ListScreenshots returns.
    // maxSize > 0: decode, scale so the longest side is <= maxSize, return JPEG.
    // maxSize == 0: return the file bytes unchanged.
    //   { "name", "mimeType", "width", "height", "size", "dataBase64" }
    CommandResult GetScreenshot(const std::string& name, std::uint32_t maxSize);
}
