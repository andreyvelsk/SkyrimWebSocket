#pragma once

#include <array>
#include <string_view>

// Compile-time list of features supported by this plugin version.
// When implementing a new feature, add its identifier here in the same commit.
// Client apps can query this via { "type": "query", "fields": { "features": "App::Features" } }
// and use the list to conditionally show/hide UI elements.
//
// Naming convention:
//   "module"           — top-level domain (e.g. "inventory")
//   "module.subfeature" — specific capability within a module (e.g. "map.customMark")
inline constexpr std::array kAppFeatures = {
    std::string_view{"player"},
    std::string_view{"player.hotkeys"},
    std::string_view{"player.quests"},
    std::string_view{"inventory"},
    std::string_view{"inventory.preview"},
    std::string_view{"magic"},
    std::string_view{"map"},
};
