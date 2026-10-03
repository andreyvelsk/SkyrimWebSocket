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
    std::string_view{"magic"},
    std::string_view{"map"},
    std::string_view{"texture_preview"},
    std::string_view{"file_download"},
    std::string_view{"inventory.models"},          // items carry modelPath + keywords
    std::string_view{"texture_preview.maxSize"},   // texture_preview accepts maxSize
    std::string_view{"screenshots"},               // screenshot_take / screenshot_list / screenshot_get
    std::string_view{"player.records"},            // Player::MiscStats + Player::ActiveEffects
    std::string_view{"debug.timings"},             // Debug::FieldTimings + perf_reset
    std::string_view{"player.discoveries"},        // Player::Discoveries
    std::string_view{"map.local"},                 // local_map_get (navmesh floor plan)
};
