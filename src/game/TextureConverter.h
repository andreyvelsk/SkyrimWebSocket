#pragma once

#include <cstdint>
#include <string>

namespace TextureConverter
{
    // Result of converting a texture asset into a web-displayable image.
    struct Preview
    {
        bool          success     = false;
        std::string   error;         // human-readable error; empty on success
        std::string   mimeType;      // "image/png" on success
        std::uint32_t width      = 0;
        std::uint32_t height     = 0;
        std::string   imageBase64;   // base64-encoded PNG bytes
    };

    // Reads a DDS texture from the game's virtual filesystem (BSA archives and
    // loose files), decodes it to RGBA8 (DXT1/DXT3/DXT5 or uncompressed
    // 24/32-bit) and encodes it as a PNG. The alpha channel is preserved, so
    // textures with transparent backgrounds stay transparent.
    //
    // `path` is a game data path such as
    // "textures/interface/icons/weapons/ironsword.dds". Backslashes are
    // accepted and normalised. Must be called on the game thread.
    //
    // This is the single reusable entry point for any texture asset: item
    // inventory icons, map marker icons, book art, etc. New commands only need
    // to resolve their own path and call this function.
    Preview DdsToPngBase64(const std::string& path);
}
