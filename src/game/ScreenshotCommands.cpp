#include "ScreenshotCommands.h"
#include "Common.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

// stb_image: decoder for PNG/BMP/JPEG screenshots. Implementation lives in
// this translation unit only. Memory-only (no stdio) and only the formats the
// engine can write.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_JPEG
#include "../../third_party/stb/stb_image.h"

// Declarations only; the implementation is compiled in TextureConverter.cpp.
#include "../../third_party/stb/stb_image_write.h"

namespace logger = SKSE::log;
namespace fs     = std::filesystem;

namespace ScreenshotCommands
{
    namespace
    {
        constexpr int kJpegQuality = 82;

        std::string Lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return static_cast<char>(::tolower(c)); });
            return s;
        }

        // Folder that holds SkyrimSE.exe (the engine writes screenshots there).
        fs::path GameDirectory()
        {
            std::vector<wchar_t> buf(1024);
            const auto len = REX::W32::GetModuleFileNameW(nullptr, buf.data(), static_cast<std::uint32_t>(buf.size()));
            if (len == 0 || len >= buf.size())
                return fs::current_path();
            return fs::path(std::wstring(buf.data(), len)).parent_path();
        }

        // "ScreenShot" unless the INI says otherwise. May contain a folder part.
        std::string BaseName()
        {
            auto* ini = RE::INISettingCollection::GetSingleton();
            if (ini) {
                if (auto* setting = ini->GetSetting("sScreenShotBaseName:Display")) {
                    const char* value = setting->GetString();
                    if (value && *value)
                        return value;
                }
            }
            return "ScreenShot";
        }

        bool IsImageExtension(const std::string& extLower)
        {
            return extLower == ".png" || extLower == ".bmp" || extLower == ".jpg" || extLower == ".jpeg";
        }

        std::string MimeType(const std::string& extLower)
        {
            if (extLower == ".png")
                return "image/png";
            if (extLower == ".bmp")
                return "image/bmp";
            return "image/jpeg";
        }

        struct Entry
        {
            fs::path      path;
            std::string   name;  // relative to the game folder, '/' separators
            std::uintmax_t size = 0;
            std::int64_t  modified = 0;  // unix seconds
        };

        std::vector<Entry> Scan()
        {
            std::vector<Entry> out;
            const fs::path root = GameDirectory();

            // The base name can point into a sub-folder ("Screens\\Shot").
            std::string base = BaseName();
            std::replace(base.begin(), base.end(), '\\', '/');
            const fs::path basePath(base);
            const fs::path dir = basePath.has_parent_path() ? root / basePath.parent_path() : root;
            const std::string prefix = Lower(basePath.filename().string());

            std::error_code ec;
            if (!fs::is_directory(dir, ec))
                return out;

            for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
                 !ec && it != end; it.increment(ec)) {
                const auto& de = *it;
                std::error_code fec;
                if (!de.is_regular_file(fec))
                    continue;
                const std::string file = de.path().filename().string();
                const std::string lower = Lower(file);
                if (lower.rfind(prefix, 0) != 0)
                    continue;
                if (!IsImageExtension(Lower(de.path().extension().string())))
                    continue;

                Entry e;
                e.path = de.path();
                e.name = fs::relative(de.path(), root, fec).generic_string();
                if (fec || e.name.empty())
                    e.name = file;
                e.size = de.file_size(fec);
                const auto ftime = de.last_write_time(fec);
                if (!fec) {
                    const auto sys = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
                    e.modified = std::chrono::duration_cast<std::chrono::seconds>(sys.time_since_epoch()).count();
                }
                out.push_back(std::move(e));
            }

            std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
                return a.modified != b.modified ? a.modified > b.modified : a.name > b.name;
            });
            return out;
        }

        bool ReadFile(const fs::path& path, std::vector<std::uint8_t>& out)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return false;
            in.seekg(0, std::ios::end);
            const auto size = in.tellg();
            if (size <= 0)
                return false;
            out.resize(static_cast<std::size_t>(size));
            in.seekg(0, std::ios::beg);
            in.read(reinterpret_cast<char*>(out.data()), size);
            return static_cast<bool>(in);
        }

        // Area-average downscale of an RGB image (good quality, no ringing).
        std::vector<std::uint8_t> Downscale(const std::uint8_t* src, int sw, int sh, int dw, int dh)
        {
            std::vector<std::uint8_t> dst(static_cast<std::size_t>(dw) * dh * 3);
            const double sx = static_cast<double>(sw) / dw;
            const double sy = static_cast<double>(sh) / dh;
            for (int y = 0; y < dh; ++y) {
                const int y0 = static_cast<int>(y * sy);
                const int y1 = std::max(y0 + 1, std::min(sh, static_cast<int>((y + 1) * sy)));
                for (int x = 0; x < dw; ++x) {
                    const int x0 = static_cast<int>(x * sx);
                    const int x1 = std::max(x0 + 1, std::min(sw, static_cast<int>((x + 1) * sx)));
                    std::uint32_t r = 0, g = 0, b = 0, n = 0;
                    for (int yy = y0; yy < y1; ++yy) {
                        const std::uint8_t* row = src + (static_cast<std::size_t>(yy) * sw + x0) * 3;
                        for (int xx = x0; xx < x1; ++xx, row += 3) {
                            r += row[0];
                            g += row[1];
                            b += row[2];
                            ++n;
                        }
                    }
                    std::uint8_t* p = dst.data() + (static_cast<std::size_t>(y) * dw + x) * 3;
                    p[0] = static_cast<std::uint8_t>(r / n);
                    p[1] = static_cast<std::uint8_t>(g / n);
                    p[2] = static_cast<std::uint8_t>(b / n);
                }
            }
            return dst;
        }

        void AppendBytes(void* ctx, void* data, int size)
        {
            auto* out = static_cast<std::vector<std::uint8_t>*>(ctx);
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            out->insert(out->end(), bytes, bytes + size);
        }
    }

    CommandResult TakeScreenshot()
    {
        auto* ini = RE::INISettingCollection::GetSingleton();
        if (ini) {
            if (auto* allow = ini->GetSetting("bAllowScreenShot:Display"))
                allow->data.b = true;
        }

        auto* controls = RE::MenuControls::GetSingleton();
        if (!controls)
            return { false, "MenuControls not available" };

        const bool queued = controls->QueueScreenshot();
        logger::info("screenshot_take queued={}", queued);

        nlohmann::json data;
        data["queued"] = queued;
        return { true, "", std::move(data) };
    }

    CommandResult ListScreenshots(std::size_t limit)
    {
        const auto entries = Scan();

        nlohmann::json files = nlohmann::json::array();
        for (std::size_t i = 0; i < entries.size() && i < limit; ++i) {
            nlohmann::json f;
            f["name"]     = entries[i].name;
            f["size"]     = entries[i].size;
            f["modified"] = entries[i].modified;
            files.push_back(std::move(f));
        }

        nlohmann::json data;
        data["directory"] = GameDirectory().generic_string();
        data["baseName"]  = BaseName();
        data["total"]     = entries.size();
        data["files"]     = std::move(files);
        return { true, "", std::move(data) };
    }

    CommandResult GetScreenshot(const std::string& name, std::uint32_t maxSize)
    {
        // Only files the listing would return: no path tricks.
        const auto entries = Scan();
        const auto it = std::find_if(entries.begin(), entries.end(),
                                     [&](const Entry& e) { return e.name == name; });
        if (it == entries.end())
            return { false, "Not a screenshot: " + name };

        std::vector<std::uint8_t> bytes;
        if (!ReadFile(it->path, bytes))
            return { false, "Failed to read: " + name };

        const std::string ext = Lower(it->path.extension().string());

        nlohmann::json data;
        data["name"] = it->name;

        if (maxSize == 0) {
            data["mimeType"]   = MimeType(ext);
            data["size"]       = bytes.size();
            data["dataBase64"] = Common::Base64Encode(bytes.data(), bytes.size());
            return { true, "", std::move(data) };
        }

        int w = 0, h = 0, comp = 0;
        std::uint8_t* rgb = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &comp, 3);
        if (!rgb)
            return { false, std::string("Decode failed: ") + (stbi_failure_reason() ? stbi_failure_reason() : "unknown") };

        int dw = w, dh = h;
        const int longest = std::max(w, h);
        if (longest > static_cast<int>(maxSize)) {
            const double k = static_cast<double>(maxSize) / longest;
            dw = std::max(1, static_cast<int>(w * k + 0.5));
            dh = std::max(1, static_cast<int>(h * k + 0.5));
        }

        std::vector<std::uint8_t> pixels;
        if (dw != w || dh != h)
            pixels = Downscale(rgb, w, h, dw, dh);
        else
            pixels.assign(rgb, rgb + static_cast<std::size_t>(w) * h * 3);
        stbi_image_free(rgb);

        std::vector<std::uint8_t> jpeg;
        jpeg.reserve(static_cast<std::size_t>(dw) * dh / 4);
        if (!stbi_write_jpg_to_func(AppendBytes, &jpeg, dw, dh, 3, pixels.data(), kJpegQuality) || jpeg.empty())
            return { false, "JPEG encode failed" };

        data["mimeType"]   = "image/jpeg";
        data["width"]      = dw;
        data["height"]     = dh;
        data["size"]       = jpeg.size();
        data["dataBase64"] = Common::Base64Encode(jpeg.data(), jpeg.size());
        return { true, "", std::move(data) };
    }
}
