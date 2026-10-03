#include "TextureConverter.h"
#include "Common.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

// stb_image_write: single-file PNG/JPEG/BMP encoder. The implementation is
// included here (the only translation unit that needs it) and the header is
// shared by no other code, so there are no ODR concerns.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../third_party/stb/stb_image_write.h"

// bcdec: BC4/BC5/BC7 block decoders (MIT / Unlicense). Skyrim SE ships many
// textures as BC7 (DX10 header); texture replacer mods nearly always do.
#define BCDEC_IMPLEMENTATION
#define BCDEC_STATIC
#include "../../third_party/bcdec/bcdec.h"

namespace
{
    constexpr std::uint32_t kDdsMagic   = 0x20534444u;  // "DDS "
    constexpr std::uint32_t kDdspfFourCC = 0x4u;

    constexpr std::uint32_t kFourCC_DXT1 = 0x31545844u;  // "DXT1"
    constexpr std::uint32_t kFourCC_DXT3 = 0x33545844u;  // "DXT3"
    constexpr std::uint32_t kFourCC_DXT5 = 0x35545844u;  // "DXT5"
    constexpr std::uint32_t kFourCC_DX10 = 0x30315844u;  // "DX10": extended header follows
    constexpr std::uint32_t kFourCC_ATI1 = 0x31495441u;  // "ATI1" (BC4)
    constexpr std::uint32_t kFourCC_BC4U = 0x55344342u;  // "BC4U"
    constexpr std::uint32_t kFourCC_ATI2 = 0x32495441u;  // "ATI2" (BC5)
    constexpr std::uint32_t kFourCC_BC5U = 0x55354342u;  // "BC5U"
    constexpr std::uint32_t kDdsdMipMapCount = 0x20000u;

    // ─── Byte helpers ──────────────────────────────────────────────────────

    std::uint32_t ReadU32(const std::uint8_t* p)
    {
        std::uint32_t v;
        std::memcpy(&v, p, sizeof(v));  // x86 little-endian
        return v;
    }

    // ─── Color helpers ─────────────────────────────────────────────────────

    struct RGBA
    {
        std::uint8_t r, g, b, a;
    };

    RGBA Expand565(std::uint16_t v)
    {
        RGBA c{};
        c.r = static_cast<std::uint8_t>(((v >> 11) & 0x1F) * 255 / 31);
        c.g = static_cast<std::uint8_t>(((v >> 5) & 0x3F) * 255 / 63);
        c.b = static_cast<std::uint8_t>((v & 0x1F) * 255 / 31);
        c.a = 255;
        return c;
    }

    RGBA Lerp(const RGBA& a, const RGBA& b, int num, int den)
    {
        RGBA c{};
        c.r = static_cast<std::uint8_t>((a.r * num + b.r * (den - num)) / den);
        c.g = static_cast<std::uint8_t>((a.g * num + b.g * (den - num)) / den);
        c.b = static_cast<std::uint8_t>((a.b * num + b.b * (den - num)) / den);
        c.a = 255;
        return c;
    }

    // Decodes the 8-byte DXT1 color block. When `oneBitAlpha` is true the
    // c0 <= c1 case produces a fully transparent texel (DXT1 punch-through);
    // otherwise it is always the 4-color interpolation (DXT3/DXT5 color block).
    void DecodeColorBlock(const std::uint8_t* block, bool oneBitAlpha, RGBA out[16])
    {
        const std::uint16_t c0 = static_cast<std::uint16_t>(block[0] | (block[1] << 8));
        const std::uint16_t c1 = static_cast<std::uint16_t>(block[2] | (block[3] << 8));
        const std::uint32_t idx = ReadU32(block + 4);

        RGBA colors[4];
        colors[0] = Expand565(c0);
        colors[1] = Expand565(c1);
        if (oneBitAlpha && c0 <= c1) {
            colors[2].r = static_cast<std::uint8_t>((colors[0].r + colors[1].r) / 2);
            colors[2].g = static_cast<std::uint8_t>((colors[0].g + colors[1].g) / 2);
            colors[2].b = static_cast<std::uint8_t>((colors[0].b + colors[1].b) / 2);
            colors[2].a = 255;
            colors[3]   = RGBA{ 0, 0, 0, 0 };
        } else {
            colors[2] = Lerp(colors[0], colors[1], 2, 3);  // (2*c0 + 1*c1)/3
            colors[3] = Lerp(colors[0], colors[1], 1, 3);  // (1*c0 + 2*c1)/3
        }

        for (int i = 0; i < 16; ++i)
            out[i] = colors[(idx >> (i * 2)) & 0x3];
    }

    void DecodeDxt1Block(const std::uint8_t* block, RGBA out[16])
    {
        DecodeColorBlock(block, /*oneBitAlpha=*/true, out);
    }

    void DecodeDxt3Block(const std::uint8_t* block, RGBA out[16])
    {
        DecodeColorBlock(block + 8, /*oneBitAlpha=*/false, out);
        for (int i = 0; i < 16; ++i) {
            const std::uint8_t nib = static_cast<std::uint8_t>((block[i >> 1] >> ((i & 1) * 4)) & 0xF);
            out[i].a = static_cast<std::uint8_t>((nib << 4) | nib);
        }
    }

    void DecodeDxt5Alpha(const std::uint8_t* block, std::uint8_t alpha[16])
    {
        const std::uint8_t a0 = block[0];
        const std::uint8_t a1 = block[1];

        std::uint64_t idx = 0;
        for (int i = 0; i < 6; ++i)
            idx |= static_cast<std::uint64_t>(block[2 + i]) << (8 * i);

        std::uint8_t alphas[8];
        alphas[0] = a0;
        alphas[1] = a1;
        if (a0 > a1) {
            for (int i = 2; i < 8; ++i)
                alphas[i] = static_cast<std::uint8_t>(((8 - i) * a0 + (i - 1) * a1) / 7);
        } else {
            for (int i = 2; i < 6; ++i)
                alphas[i] = static_cast<std::uint8_t>(((6 - i) * a0 + (i - 1) * a1) / 5);
            alphas[6] = 0;
            alphas[7] = 255;
        }

        for (int i = 0; i < 16; ++i)
            alpha[i] = alphas[(idx >> (i * 3)) & 0x7];
    }

    void DecodeDxt5Block(const std::uint8_t* block, RGBA out[16])
    {
        DecodeColorBlock(block + 8, /*oneBitAlpha=*/false, out);
        std::uint8_t alpha[16];
        DecodeDxt5Alpha(block, alpha);
        for (int i = 0; i < 16; ++i)
            out[i].a = alpha[i];
    }

    // ─── Uncompressed DDS (24/32-bit) ─────────────────────────────────────

    std::uint8_t ExtractMasked(std::uint32_t value, std::uint32_t mask)
    {
        if (mask == 0)
            return 255;
        unsigned int shift = 0;
        while (((mask >> shift) & 1u) == 0u)
            ++shift;
        const std::uint32_t m = mask >> shift;
        const std::uint32_t v = (value & mask) >> shift;
        return static_cast<std::uint8_t>((v * 255u) / m);
    }

    void DecodeUncompressedBlock(const std::uint8_t* src,
                                 std::uint32_t         bytesPerPixel,
                                 std::uint32_t         rMask,
                                 std::uint32_t         gMask,
                                 std::uint32_t         bMask,
                                 std::uint32_t         aMask,
                                 std::uint32_t         pixelCount,
                                 RGBA*                 out)
    {
        for (std::uint32_t i = 0; i < pixelCount; ++i) {
            std::uint32_t value = 0;
            std::memcpy(&value, src + i * bytesPerPixel, bytesPerPixel);
            out[i].r = ExtractMasked(value, rMask);
            out[i].g = ExtractMasked(value, gMask);
            out[i].b = ExtractMasked(value, bMask);
            out[i].a = ExtractMasked(value, aMask);
        }
    }

    // ─── DDS format description ────────────────────────────────────────────

    enum class DdsFormat
    {
        kDxt1,
        kDxt3,
        kDxt5,
        kBc4,
        kBc5,
        kBc7,
        kUncompressed,
        kUnsupported,
    };

    struct DdsInfo
    {
        DdsFormat    format = DdsFormat::kUnsupported;
        std::uint32_t width  = 0;
        std::uint32_t height = 0;
        std::uint32_t bytesPerPixel = 0;
        std::uint32_t rMask = 0, gMask = 0, bMask = 0, aMask = 0;
        std::uint32_t mipCount = 1;
        bool          dx10 = false;  // a 20-byte DXGI header follows the 128-byte header
    };

    // DXGI_FORMAT values found in game textures.
    void ApplyDxgiFormat(DdsInfo& info, std::uint32_t dxgi)
    {
        switch (dxgi) {
            case 70: case 71: case 72: info.format = DdsFormat::kDxt1; break;  // BC1
            case 73: case 74: case 75: info.format = DdsFormat::kDxt3; break;  // BC2
            case 76: case 77: case 78: info.format = DdsFormat::kDxt5; break;  // BC3
            case 79: case 80:          info.format = DdsFormat::kBc4; break;
            case 82: case 83:          info.format = DdsFormat::kBc5; break;
            case 97: case 98: case 99: info.format = DdsFormat::kBc7; break;
            case 27: case 28: case 29:  // R8G8B8A8
                info.format = DdsFormat::kUncompressed;
                info.bytesPerPixel = 4;
                info.rMask = 0x000000FFu; info.gMask = 0x0000FF00u; info.bMask = 0x00FF0000u; info.aMask = 0xFF000000u;
                break;
            case 87: case 90: case 91:  // B8G8R8A8
                info.format = DdsFormat::kUncompressed;
                info.bytesPerPixel = 4;
                info.rMask = 0x00FF0000u; info.gMask = 0x0000FF00u; info.bMask = 0x000000FFu; info.aMask = 0xFF000000u;
                break;
            case 88: case 92: case 93:  // B8G8R8X8
                info.format = DdsFormat::kUncompressed;
                info.bytesPerPixel = 4;
                info.rMask = 0x00FF0000u; info.gMask = 0x0000FF00u; info.bMask = 0x000000FFu; info.aMask = 0;
                break;
            default: info.format = DdsFormat::kUnsupported; break;
        }
    }

    // Parses the 128-byte DDS header (already validated magic).
    DdsInfo ParseHeader(const std::uint8_t* h)
    {
        DdsInfo info;
        info.height = ReadU32(h + 12);
        info.width  = ReadU32(h + 16);
        if (info.width == 0 || info.height == 0)
            return info;

        const std::uint32_t flags    = ReadU32(h + 8);
        const std::uint32_t pfFlags  = ReadU32(h + 80);
        const std::uint32_t fourCC   = ReadU32(h + 84);
        const std::uint32_t bitCount = ReadU32(h + 88);
        if ((flags & kDdsdMipMapCount) != 0)
            info.mipCount = std::max<std::uint32_t>(1, ReadU32(h + 28));

        if ((pfFlags & kDdspfFourCC) != 0) {
            switch (fourCC) {
                case kFourCC_DXT1: info.format = DdsFormat::kDxt1; break;
                case kFourCC_DXT3: info.format = DdsFormat::kDxt3; break;
                case kFourCC_DXT5: info.format = DdsFormat::kDxt5; break;
                case kFourCC_ATI1:
                case kFourCC_BC4U: info.format = DdsFormat::kBc4; break;
                case kFourCC_ATI2:
                case kFourCC_BC5U: info.format = DdsFormat::kBc5; break;
                case kFourCC_DX10: info.dx10 = true; break;  // format from the DXGI header
                default:           info.format = DdsFormat::kUnsupported; break;
            }
        } else if (bitCount == 24 || bitCount == 32) {
            info.format        = DdsFormat::kUncompressed;
            info.bytesPerPixel = bitCount / 8;
            info.rMask         = ReadU32(h + 92);
            info.gMask         = ReadU32(h + 96);
            info.bMask         = ReadU32(h + 100);
            info.aMask         = ReadU32(h + 104);
        }
        return info;
    }

    // Bytes per 4×4 block (0 for uncompressed formats).
    std::uint32_t BlockBytes(DdsFormat format)
    {
        switch (format) {
            case DdsFormat::kDxt1:
            case DdsFormat::kBc4: return 8;
            case DdsFormat::kDxt3:
            case DdsFormat::kDxt5:
            case DdsFormat::kBc5:
            case DdsFormat::kBc7: return 16;
            default: return 0;
        }
    }

    // Size in bytes of one mip level of the given dimensions.
    std::size_t MipSize(const DdsInfo& info, std::uint32_t width, std::uint32_t height)
    {
        if (info.format == DdsFormat::kUncompressed)
            return static_cast<std::size_t>(width) * height * info.bytesPerPixel;
        const std::uint32_t bytes = BlockBytes(info.format);
        const std::uint32_t bw    = (width + 3) / 4;
        const std::uint32_t bh    = (height + 3) / 4;
        return static_cast<std::size_t>(bw) * bh * bytes;
    }

    // ─── DDS → RGBA decoding pipeline ──────────────────────────────────────

    // Reads and validates the DDS header + mip-0 pixel data from the stream.
    // On failure, fills result.error and returns false.
    static bool ReadDdsData(RE::BSResourceNiBinaryStream& stream, const std::string& path,
                            std::uint32_t maxSize, DdsInfo& info, std::vector<std::uint8_t>& block,
                            TextureConverter::Preview& result)
    {
        std::array<std::uint8_t, 128> header{};
        if (!stream.read(header.data(), static_cast<std::uint32_t>(header.size()))) {
            result.error = "Failed to read DDS header: " + path;
            return false;
        }
        if (ReadU32(header.data()) != kDdsMagic) {
            result.error = "Not a DDS texture: " + path;
            return false;
        }

        info = ParseHeader(header.data());
        if (info.dx10) {
            std::array<std::uint8_t, 20> dx10{};
            if (!stream.read(dx10.data(), static_cast<std::uint32_t>(dx10.size()))) {
                result.error = "Failed to read DDS DX10 header: " + path;
                return false;
            }
            ApplyDxgiFormat(info, ReadU32(dx10.data()));
        }
        if (info.format == DdsFormat::kUnsupported) {
            result.error = "Unsupported DDS pixel format: " + path;
            return false;
        }

        // Start from the smallest mip that is still at least maxSize: a
        // 128 px preview of a 2048 px texture reads 1/256 of the pixels.
        std::uint32_t level = 0;
        if (maxSize > 0) {
            while (level + 1 < info.mipCount) {
                const std::uint32_t nw = std::max<std::uint32_t>(1, info.width >> (level + 1));
                const std::uint32_t nh = std::max<std::uint32_t>(1, info.height >> (level + 1));
                if (std::max(nw, nh) < maxSize)
                    break;
                ++level;
            }
        }
        std::size_t skip = 0;
        for (std::uint32_t l = 0; l < level; ++l)
            skip += MipSize(info, std::max<std::uint32_t>(1, info.width >> l), std::max<std::uint32_t>(1, info.height >> l));
        if (skip > 0) {
            std::vector<std::uint8_t> scratch(std::min<std::size_t>(skip, 1u << 20));
            while (skip > 0) {
                const auto n = static_cast<std::uint32_t>(std::min(skip, scratch.size()));
                if (!stream.read(scratch.data(), n)) {
                    result.error = "Failed to skip DDS mip levels: " + path;
                    return false;
                }
                skip -= n;
            }
            info.width  = std::max<std::uint32_t>(1, info.width >> level);
            info.height = std::max<std::uint32_t>(1, info.height >> level);
        }

        const std::size_t mipSize = MipSize(info, info.width, info.height);
        if (mipSize == 0) {
            result.error = "Invalid DDS dimensions: " + path;
            return false;
        }

        block.resize(mipSize);
        if (!stream.read(block.data(), static_cast<std::uint32_t>(mipSize))) {
            result.error = "Failed to read DDS pixel data: " + path;
            return false;
        }
        return true;
    }

    // Decodes DDS compressed/uncompressed blocks to an RGBA8 pixel array.
    static void DecodeDdsToRGBA(const DdsInfo& info, const std::vector<std::uint8_t>& block,
                                std::vector<RGBA>& rgba)
    {
        const std::uint32_t bw = (info.width + 3) / 4;
        const std::uint32_t bh = (info.height + 3) / 4;

        for (std::uint32_t by = 0; by < bh; ++by) {
            for (std::uint32_t bx = 0; bx < bw; ++bx) {
                RGBA decoded[16]{};
                switch (info.format) {
                    case DdsFormat::kDxt1:
                        DecodeDxt1Block(block.data() + (by * bw + bx) * 8, decoded);
                        break;
                    case DdsFormat::kDxt3:
                        DecodeDxt3Block(block.data() + (by * bw + bx) * 16, decoded);
                        break;
                    case DdsFormat::kDxt5:
                        DecodeDxt5Block(block.data() + (by * bw + bx) * 16, decoded);
                        break;
                    case DdsFormat::kBc7:
                        bcdec_bc7(block.data() + (by * bw + bx) * 16, decoded, 4 * sizeof(RGBA));
                        break;
                    case DdsFormat::kBc4: {
                        std::uint8_t r[16];
                        bcdec_bc4(block.data() + (by * bw + bx) * 8, r, 4);
                        for (int i = 0; i < 16; ++i)
                            decoded[i] = RGBA{ r[i], r[i], r[i], 255 };
                        break;
                    }
                    case DdsFormat::kBc5: {
                        std::uint8_t rg[32];
                        bcdec_bc5(block.data() + (by * bw + bx) * 16, rg, 8);
                        for (int i = 0; i < 16; ++i)
                            decoded[i] = RGBA{ rg[i * 2], rg[i * 2 + 1], 255, 255 };
                        break;
                    }
                    default: break;
                }

                for (std::uint32_t py = 0; py < 4; ++py) {
                    const std::uint32_t y = by * 4 + py;
                    if (y >= info.height) break;
                    for (std::uint32_t px = 0; px < 4; ++px) {
                        const std::uint32_t x = bx * 4 + px;
                        if (x >= info.width) break;
                        rgba[y * info.width + x] = decoded[py * 4 + px];
                    }
                }
            }
        }

        if (info.format == DdsFormat::kUncompressed) {
            DecodeUncompressedBlock(block.data(), info.bytesPerPixel, info.rMask,
                                    info.gMask, info.bMask, info.aMask,
                                    info.width * info.height, rgba.data());
        }
    }

    // Halves the image (2x2 box filter) until the longest side is <= maxSize.
    static void DownsampleToFit(std::vector<RGBA>& rgba, std::uint32_t& width,
                                std::uint32_t& height, std::uint32_t maxSize)
    {
        if (maxSize == 0)
            return;
        while ((width > maxSize || height > maxSize) && width >= 2 && height >= 2) {
            const std::uint32_t nw = width / 2;
            const std::uint32_t nh = height / 2;
            std::vector<RGBA>   out(static_cast<std::size_t>(nw) * nh);
            for (std::uint32_t y = 0; y < nh; ++y) {
                for (std::uint32_t x = 0; x < nw; ++x) {
                    const RGBA& a = rgba[(2 * y) * width + 2 * x];
                    const RGBA& b = rgba[(2 * y) * width + 2 * x + 1];
                    const RGBA& c = rgba[(2 * y + 1) * width + 2 * x];
                    const RGBA& d = rgba[(2 * y + 1) * width + 2 * x + 1];
                    out[y * nw + x] = RGBA{
                        static_cast<std::uint8_t>((a.r + b.r + c.r + d.r + 2) / 4),
                        static_cast<std::uint8_t>((a.g + b.g + c.g + d.g + 2) / 4),
                        static_cast<std::uint8_t>((a.b + b.b + c.b + d.b + 2) / 4),
                        static_cast<std::uint8_t>((a.a + b.a + c.a + d.a + 2) / 4)
                    };
                }
            }
            rgba   = std::move(out);
            width  = nw;
            height = nh;
        }
    }

    // Encodes RGBA8 pixels to an in-memory PNG via stb_image_write.
    // Returns the PNG bytes; caller must free with STBIW_FREE.
    static unsigned char* EncodePng(const std::vector<RGBA>& rgba,
                                    std::uint32_t width, std::uint32_t height, int& outLen)
    {
        return stbi_write_png_to_mem(
            reinterpret_cast<const unsigned char*>(rgba.data()),
            static_cast<int>(width * 4),
            static_cast<int>(width),
            static_cast<int>(height),
            4,
            &outLen);
    }
}

namespace TextureConverter
{
    Preview DdsToPngBase64(const std::string& path, std::uint32_t maxSize)
    {
        Preview result;
        if (path.empty()) {
            result.error = "Empty texture path";
            return result;
        }

        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');

        RE::BSResourceNiBinaryStream stream(normalized);
        if (!stream.good()) {
            result.error = "Failed to open texture: " + path;
            return result;
        }

        DdsInfo info;
        std::vector<std::uint8_t> block;
        if (!ReadDdsData(stream, path, maxSize, info, block, result))
            return result;

        std::vector<RGBA> rgba(info.width * info.height);
        DecodeDdsToRGBA(info, block, rgba);

        std::uint32_t outWidth  = info.width;
        std::uint32_t outHeight = info.height;
        DownsampleToFit(rgba, outWidth, outHeight, maxSize);

        int            pngLen = 0;
        unsigned char* png    = EncodePng(rgba, outWidth, outHeight, pngLen);
        if (!png || pngLen <= 0) {
            result.error = "Failed to encode PNG: " + path;
            return result;
        }

        result.success     = true;
        result.mimeType    = "image/png";
        result.width       = outWidth;
        result.height      = outHeight;
        result.imageBase64 = Common::Base64Encode(png, static_cast<std::size_t>(pngLen));
        STBIW_FREE(png);
        return result;
    }
}
