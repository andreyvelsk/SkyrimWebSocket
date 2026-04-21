#pragma once

#include <string>

inline void PrintConsole(const std::string& msg)
{
    if (auto* log = RE::ConsoleLog::GetSingleton())
        log->Print(msg.c_str());
}

// Returns true for weapon types that occupy both hands (two-handed melee,
// bows, crossbows). Staves are one-handed.
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
