#include "Common.h"

#include "../../logger.h"

namespace Common
{
    std::string GetGMSTString(const char* key)
    {
        auto* gmst = RE::GameSettingCollection::GetSingleton();
        if (!gmst)
            return {};
        auto* setting = gmst->GetSetting(key);
        if (!setting)
            return {};
        const char* str = setting->GetString();
        return (str && *str != '\0') ? str : "";
    }

    RE::TESWorldSpace* ResolveWorldspaceRoot(RE::TESWorldSpace* world)
    {
        if (!world)
            return nullptr;
        auto* root = world;
        while (root->parentWorld)
            root = root->parentWorld;
        return root;
    }

    void BuildWorldspaceFields(nlohmann::json& obj, RE::TESWorldSpace* world)
    {
        if (world) {
            const char* edid = world->GetFormEditorID();
            obj["worldspace"]       = edid ? std::string(edid) : std::string();
            obj["worldspaceFormId"] = FormIdToString(world->GetFormID());

            auto* root = ResolveWorldspaceRoot(world);
            const char* rootEdid = root->GetFormEditorID();
            obj["parentWorldspace"]       = rootEdid ? std::string(rootEdid) : std::string();
            obj["parentWorldspaceFormId"] = FormIdToString(root->GetFormID());
        } else {
            obj["worldspace"]             = nullptr;
            obj["worldspaceFormId"]       = nullptr;
            obj["parentWorldspace"]       = nullptr;
            obj["parentWorldspaceFormId"] = nullptr;
        }
    }

    std::string WcsToUtf8(const wchar_t* ws)
    {
        if (!ws) return {};
        std::string out;
        for (; *ws; ++ws) {
            const auto c = static_cast<unsigned>(*ws);
            if (c < 0x80u) {
                out += static_cast<char>(c);
            } else if (c < 0x800u) {
                out += static_cast<char>(0xC0u | (c >> 6u));
                out += static_cast<char>(0x80u | (c & 0x3Fu));
            } else {
                out += static_cast<char>(0xE0u | (c >> 12u));
                out += static_cast<char>(0x80u | ((c >> 6u) & 0x3Fu));
                out += static_cast<char>(0x80u | (c & 0x3Fu));
            }
        }
        return out;
    }

    std::string ToLowerAscii(std::string value)
    {
        for (char& ch : value)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return value;
    }

    bool EqualAsciiIgnoreCase(std::string_view lhs, std::string_view rhs)
    {
        if (lhs.size() != rhs.size())
            return false;
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            const auto l = static_cast<unsigned char>(lhs[i]);
            const auto r = static_cast<unsigned char>(rhs[i]);
            if (std::tolower(l) != std::tolower(r))
                return false;
        }
        return true;
    }

    std::string_view TrimAscii(std::string_view value)
    {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
            value.remove_prefix(1);
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
            value.remove_suffix(1);
        return value;
    }

    std::string Base64Encode(const std::uint8_t* data, std::size_t len)
    {
        static constexpr char kAlphabet[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string out;
        out.reserve(((len + 2) / 3) * 4);
        for (std::size_t i = 0; i < len; i += 3) {
            const std::uint32_t n = (data[i] << 16) |
                                    ((i + 1 < len) ? (data[i + 1] << 8) : 0) |
                                    ((i + 2 < len) ? data[i + 2] : 0);
            out.push_back(kAlphabet[(n >> 18) & 63]);
            out.push_back(kAlphabet[(n >> 12) & 63]);
            out.push_back((i + 1 < len) ? kAlphabet[(n >> 6) & 63] : '=');
            out.push_back((i + 2 < len) ? kAlphabet[n & 63] : '=');
        }
        return out;
    }
}
