#pragma once

#include <array>
#include <string>
#include <utility>

inline void PrintConsole(const std::string& msg)
{
    if (auto* log = RE::ConsoleLog::GetSingleton())
        log->Print(msg.c_str());
}

// Returns true for weapon types that occupy both hands (two-handed melee,
// bows, crossbows). Staves are one-handed.
// ─── Shared enum-to-string tables ───────────────────────────────────────────

using BOS = RE::BGSBipedObjectForm::BipedObjectSlot;

// Ordered: first match wins (loop with HasPartOf / == comparison).
inline constexpr std::array<std::pair<RE::WEAPON_TYPE, const char*>, 10> kWeaponTypeNames = {{
    { RE::WEAPON_TYPE::kHandToHandMelee, "HandToHandMelee" },
    { RE::WEAPON_TYPE::kOneHandSword,    "OneHandSword"    },
    { RE::WEAPON_TYPE::kOneHandDagger,   "OneHandDagger"   },
    { RE::WEAPON_TYPE::kOneHandAxe,      "OneHandAxe"      },
    { RE::WEAPON_TYPE::kOneHandMace,     "OneHandMace"     },
    { RE::WEAPON_TYPE::kTwoHandSword,    "TwoHandSword"    },
    { RE::WEAPON_TYPE::kTwoHandAxe,      "TwoHandAxe"      },
    { RE::WEAPON_TYPE::kBow,             "Bow"             },
    { RE::WEAPON_TYPE::kStaff,           "Staff"           },
    { RE::WEAPON_TYPE::kCrossbow,        "Crossbow"        },
}};

inline constexpr std::array<std::pair<BOS, const char*>, 13> kBodySlotNames = {{
    { BOS::kHead,     "Head"     },
    { BOS::kHair,     "Hair"     },
    { BOS::kBody,     "Body"     },
    { BOS::kHands,    "Hands"    },
    { BOS::kForearms, "Forearms" },
    { BOS::kAmulet,   "Amulet"   },
    { BOS::kRing,     "Ring"     },
    { BOS::kFeet,     "Feet"     },
    { BOS::kCalves,   "Calves"   },
    { BOS::kShield,   "Shield"   },
    { BOS::kTail,     "Tail"     },
    { BOS::kLongHair, "LongHair" },
    { BOS::kCirclet,  "Circlet"  },
}};

// ─────────────────────────────────────────────────────────────────────────────

inline bool IsWeaponTwoHanded(RE::WEAPON_TYPE type)
{
    switch (type) {
        case RE::WEAPON_TYPE::kTwoHandSword:
        case RE::WEAPON_TYPE::kTwoHandAxe:
        case RE::WEAPON_TYPE::kBow:
        case RE::WEAPON_TYPE::kCrossbow:
            return true;
        default:
            return false;
    }
}
