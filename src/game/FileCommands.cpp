#include "FileCommands.h"
#include "Common.h"
#include "TextureConverter.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace logger = SKSE::log;

namespace FileCommands
{
    // ─── Private helpers ───────────────────────────────────────────────────

    // Runs TextureConverter and builds the JSON response.
    static CommandResult BuildPreviewResult(const std::string& ddsPath)
    {
        const auto preview = TextureConverter::DdsToPngBase64(ddsPath);
        if (!preview.success)
            return { false, preview.error };

        nlohmann::json data;
        data["mimeType"]    = preview.mimeType;
        data["width"]       = preview.width;
        data["height"]      = preview.height;
        data["imageBase64"] = preview.imageBase64;
        return { true, "", std::move(data) };
    }

    // Maps a lowercase file extension (with leading dot) to a MIME type.
    static std::string MimeTypeFromPath(const std::string& path)
    {
        // clang-format off
        static const std::unordered_map<std::string, std::string> kMap = {
            {".txt",  "text/plain"},
            {".ini",  "text/plain"},
            {".log",  "text/plain"},
            {".swf",  "application/x-shockwave-flash"},
            {".dds",  "application/octet-stream"},
            {".png",  "image/png"},
            {".jpg",  "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".xml",  "application/xml"},
            {".json", "application/json"},
            {".html", "text/html"},
            {".htm",  "text/html"},
            {".css",  "text/css"},
            {".js",   "application/javascript"},
            {".nif",  "application/octet-stream"},
            {".pex",  "application/octet-stream"},
            {".hkx",  "application/octet-stream"},
        };
        // clang-format on
        const auto dot = path.rfind('.');
        if (dot == std::string::npos)
            return "application/octet-stream";
        std::string ext = path.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        auto it = kMap.find(ext);
        return it != kMap.end() ? it->second : "application/octet-stream";
    }

    // ─── Commands ─────────────────────────────────────────────────────────

    CommandResult GetTexturePreview(const std::string& path)
    {
        logger::debug("texture_preview path='{}'", path);
        return BuildPreviewResult(path);
    }

    CommandResult GetFileDownload(const std::string& path)
    {
        logger::debug("file_download path='{}'", path);

        std::string normalized = path;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');

        RE::BSResourceNiBinaryStream stream(normalized);
        if (!stream.good())
            return { false, "Failed to open file: " + path };

        std::vector<std::uint8_t> data;
        std::uint8_t              chunk[65536];
        const std::uint32_t       chunkSize = static_cast<std::uint32_t>(sizeof(chunk));
        while (stream.good()) {
            const std::uint32_t posBefore = stream.tell();
            const bool          ok = stream.read(chunk, chunkSize);
            const std::uint32_t posAfter  = stream.tell();
            const std::uint32_t bytesRead = posAfter - posBefore;
            if (bytesRead == 0)
                break;
            data.insert(data.end(), chunk, chunk + bytesRead);
            if (!ok)
                break; // partial read at EOF — all remaining bytes captured via tell()
        }

        if (data.empty())
            return { false, "File is empty: " + path };

        nlohmann::json resp;
        resp["mimeType"]   = MimeTypeFromPath(path);
        resp["size"]       = data.size();
        resp["dataBase64"] = Common::Base64Encode(data.data(), data.size());
        return { true, "", std::move(resp) };
    }
}