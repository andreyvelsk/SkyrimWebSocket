#pragma once

#include <RE/Skyrim.h>

#include <cctype>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace Common
{
    // Format a FormID as "0xXXXXXXXX".
    inline std::string FormIdToString(RE::FormID id)
    {
        return std::format("0x{:08X}", id);
    }

    // Look up a GameSetting string by key (e.g. "sSkillHeavyarmor").
    // Returns an empty string when the key does not exist or is not a string setting.
    std::string GetGMSTString(const char* key);

    // Walk TESWorldSpace::parentWorld chain to the root worldspace.
    // Returns the same pointer when world has no parent.
    RE::TESWorldSpace* ResolveWorldspaceRoot(RE::TESWorldSpace* world);

    // Write worldspace fields (worldspace, worldspaceFormId, parentWorldspace,
    // parentWorldspaceFormId) into a JSON object.  Sets all fields to nullptr
    // when world is null.
    void BuildWorldspaceFields(nlohmann::json& obj, RE::TESWorldSpace* world);

    // Convert a wide (UTF-16) string to UTF-8.
    // Handles the full Basic Multilingual Plane (U+0000..U+FFFF).
    std::string WcsToUtf8(const wchar_t* ws);

    // Convert a string to lowercase ASCII in-place.
    std::string ToLowerAscii(std::string value);

    // Case-insensitive ASCII comparison.
    bool EqualAsciiIgnoreCase(std::string_view lhs, std::string_view rhs);

    // Trim leading/trailing ASCII whitespace from a string_view.
    std::string_view TrimAscii(std::string_view value);

    // Encode raw bytes to a base64 string (RFC 4648, standard alphabet).
    std::string Base64Encode(const std::uint8_t* data, std::size_t len);
}