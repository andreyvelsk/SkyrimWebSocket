#include "MagicReader.h"

#include <array>
#include <format>
#include <unordered_map>
#include <cctype>
#include <algorithm>
#include <set>

namespace Magic
{
    // clang-format off
    static const std::unordered_map<std::string, const char*> s_categoryGMSTKeys = {
        { "All",          "" },
        { "Destruction",   "" },
        { "Alteration",    "" },
        { "Conjuration",   "" },
        { "Illusion",      "" },
        { "Restoration",   "" },
        { "Favorites",     "" },
    };
    // clang-format on

    // Helper: build base JSON for a spell-like object
    static nlohmann::json BuildBaseSpell(RE::MagicItem* spell)
    {
        nlohmann::json j;
        if (!spell) {
            j["name"] = "";
            j["formId"] = nullptr;
            return j;
        }
        j["name"] = spell->GetName();
        j["formId"] = std::format("0x{:08X}", spell->GetFormID());
        j["effects"] = nlohmann::json::array();
        for (const auto* eff : spell->effects) {
            if (!eff || !eff->baseEffect)
                continue;
            nlohmann::json e;
            e["name"] = eff->baseEffect->GetName();
            e["magnitude"] = eff->effectItem.magnitude;
            e["duration"] = eff->effectItem.duration;
            e["descriptionTemplate"] = std::string(eff->baseEffect->magicItemDescription.c_str());
            // description resolving is left to client to substitute <mag>/<dur>
            e["description"] = e["descriptionTemplate"];
            j["effects"].push_back(std::move(e));
        }
        return j;
    }

    // Convert string to lower-case (ASCII).  Used for heuristic classification
    // of effect names when runtime metadata is unavailable.
    static std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    // Heuristic: determine which magic school a spell most likely belongs to.
    // This uses localized effect/spell names and is therefore a best-effort
    // fallback when the engine's explicit school field is not accessible.
    static std::string DetermineSpellSchool(const RE::MagicItem* magic)
    {
        if (!magic)
            return "Alteration"; // safe default

        auto checkName = [&](const std::string& raw) {
            const std::string name = ToLower(raw);
            if (name.find("fire") != std::string::npos || name.find("frost") != std::string::npos ||
                name.find("shock") != std::string::npos || name.find("damage") != std::string::npos ||
                name.find("drain") != std::string::npos)
                return std::string("Destruction");

            if (name.find("restore") != std::string::npos || name.find("heal") != std::string::npos ||
                name.find("regener") != std::string::npos || name.find("cure") != std::string::npos)
                return std::string("Restoration");

            if (name.find("summon") != std::string::npos || name.find("conjure") != std::string::npos ||
                name.find("reanimate") != std::string::npos || name.find("bound") != std::string::npos)
                return std::string("Conjuration");

            if (name.find("invis") != std::string::npos || name.find("calm") != std::string::npos ||
                name.find("frenzy") != std::string::npos || name.find("fear") != std::string::npos ||
                name.find("muffle") != std::string::npos || name.find("silence") != std::string::npos ||
                name.find("charm") != std::string::npos)
                return std::string("Illusion");

            if (name.find("shield") != std::string::npos || name.find("telekinesis") != std::string::npos ||
                name.find("waterbreath") != std::string::npos || name.find("transmute") != std::string::npos)
                return std::string("Alteration");

            return std::string();
        };

        // Inspect effects first
        for (const auto* eff : magic->effects) {
            if (!eff || !eff->baseEffect)
                continue;
            auto s = checkName(eff->baseEffect->GetName());
            if (!s.empty())
                return s;
        }

        // Fallback to the spell name
        auto s = checkName(magic->GetName());
        if (!s.empty())
            return s;

        // Default
        return "Alteration";
    }

    // Returns spells known to player filtered by school.  "All" returns
    // the complete set.  This is intentionally independent from
    // ReadAllSpells() to avoid repeated JSON parsing.
    static nlohmann::json ReadSpellsBySchool(const std::string& school)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        nlohmann::json out = nlohmann::json::array();
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler)
            return out;

        std::set<std::uint32_t> seen;

        auto pushIfMatches = [&](RE::MagicItem* magic) {
            if (!magic)
                return;
            if (!player->HasSpell(magic))
                return;
            const auto fid = magic->GetFormID();
            if (seen.find(fid) != seen.end())
                return;
            seen.insert(fid);

            if (school == "All") {
                out.push_back(BuildBaseSpell(magic));
                return;
            }

            const auto s = DetermineSpellSchool(magic);
            if (s == school)
                out.push_back(BuildBaseSpell(magic));
        };

        for (auto* magic : dataHandler->GetFormArray<RE::SpellItem>())
            pushIfMatches(magic);

        for (auto* magic : dataHandler->GetFormArray<RE::MagicItem>())
            pushIfMatches(magic);

        return out;
    }

    nlohmann::json ReadCategories()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        // Build category counts using per-school resolvers so counts reflect
        // the heuristic classification implemented in ReadSpellsBySchool().
        const std::vector<std::string> cats = { "All", "Destruction", "Alteration", "Conjuration", "Illusion", "Restoration" };
        nlohmann::json result = nlohmann::json::array();
        for (const auto& cat : cats) {
            auto arr = ReadSpellsBySchool(cat);
            if (arr.empty())
                continue;
            result.push_back({
                { "categoryId", cat },
                { "name", cat },
                { "count", static_cast<int>(arr.size()) },
            });
        }
        return result;
    }

    nlohmann::json ReadAllSpells()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        nlohmann::json result = nlohmann::json::array();

        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler)
            return result;

        for (auto* magic : dataHandler->GetFormArray<RE::SpellItem>()) {
            if (!magic)
                continue;
            if (player->HasSpell(magic))
                result.push_back(BuildBaseSpell(magic));
        }

        for (auto* magic : dataHandler->GetFormArray<RE::MagicItem>()) {
            if (!magic)
                continue;
            bool found = false;
            const auto fid = std::format("0x{:08X}", magic->GetFormID());
            for (const auto& item : result) {
                if (item.contains("formId") && item["formId"] == fid) {
                    found = true;
                    break;
                }
            }
            if (found)
                continue;
            if (player->HasSpell(magic))
                result.push_back(BuildBaseSpell(magic));
        }

        return result;
    }

    nlohmann::json ReadFavorites()
    {
        return nlohmann::json::array();
    }

    std::function<nlohmann::json()> MakeCategoryResolver(const std::string& categoryId)
    {
        // Return per-school resolvers that call the filtered reader. "All"
        // returns the complete known-spells list.
        if (categoryId == "All")
            return []() { return ReadSpellsBySchool("All"); };
        if (categoryId == "Destruction")
            return []() { return ReadSpellsBySchool("Destruction"); };
        if (categoryId == "Alteration")
            return []() { return ReadSpellsBySchool("Alteration"); };
        if (categoryId == "Conjuration")
            return []() { return ReadSpellsBySchool("Conjuration"); };
        if (categoryId == "Illusion")
            return []() { return ReadSpellsBySchool("Illusion"); };
        if (categoryId == "Restoration")
            return []() { return ReadSpellsBySchool("Restoration"); };
        if (categoryId == "Favorites")
            return []() { return ReadFavorites(); };
        return []() { return nlohmann::json::array(); };
    }
}
