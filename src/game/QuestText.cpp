#include "QuestText.h"

#include <cctype>

namespace logger = SKSE::log;

namespace QuestText
{
    namespace
    {
        std::string_view TrimAscii(std::string_view value)
        {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
                value.remove_prefix(1);
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
                value.remove_suffix(1);
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

        bool IsAliasTokenHead(std::string_view head)
        {
            head = TrimAscii(head);
            return head.size() >= 5 &&
                EqualAsciiIgnoreCase(head.substr(0, 5), "Alias") &&
                (head.size() == 5 || head[5] == '.');
        }

        RE::BGSBaseAlias* FindAliasByName(RE::TESQuest* quest, std::string_view name)
        {
            name = TrimAscii(name);
            if (!quest || name.empty())
                return nullptr;

            for (auto* alias : quest->aliases) {
                if (!alias)
                    continue;
                const char* aliasName = alias->aliasName.c_str();
                if (aliasName && EqualAsciiIgnoreCase(aliasName, name))
                    return alias;
            }
            return nullptr;
        }

        std::string RefDisplayName(RE::TESObjectREFR* ref)
        {
            if (!ref)
                return {};

            const char* name = ref->GetDisplayFullName();
            if (name && *name)
                return name;

            if (auto* base = ref->GetBaseObject()) {
                name = base->GetName();
                if (name && *name)
                    return name;
                const char* editorId = base->GetFormEditorID();
                if (editorId && *editorId)
                    return editorId;
            }

            const char* editorId = ref->GetFormEditorID();
            if (editorId && *editorId)
                return editorId;
            return {};
        }

        std::string FormDisplayName(RE::TESForm* form)
        {
            if (!form)
                return {};
            if (auto* ref = form->As<RE::TESObjectREFR>())
                return RefDisplayName(ref);
            const char* name = form->GetName();
            if (name && *name)
                return name;
            const char* editorId = form->GetFormEditorID();
            if (editorId && *editorId)
                return editorId;
            return {};
        }

        std::string ResolveAliasDisplayName(RE::TESQuest* quest,
                                            RE::BGSBaseAlias* alias,
                                            std::uint32_t instanceID)
        {
            if (!quest || !alias)
                return {};

            // Try to get ref from ref alias
            if (alias->GetType() == RE::BGSBaseAlias::Type::kReference) {
                auto* refAlias = static_cast<RE::BGSRefAlias*>(alias);
                if (refAlias) {
                    RE::TESObjectREFR* ref = nullptr;
                    if (refAlias->GetReference(ref) && ref) {
                        if (auto name = RefDisplayName(ref); !name.empty())
                            return name;
                    }
                }
            }

            // Fallback to alias name
            const char* aliasName = alias->aliasName.c_str();
            return aliasName ? std::string(aliasName) : std::string();
        }
    }

    const char* QuestTypeName(RE::QUEST_DATA::Type type)
    {
        switch (type) {
        case RE::QUEST_DATA::Type::kMainQuest:        return "MainQuest";
        case RE::QUEST_DATA::Type::kMagesGuild:       return "MagesGuild";
        case RE::QUEST_DATA::Type::kThievesGuild:     return "ThievesGuild";
        case RE::QUEST_DATA::Type::kDarkBrotherhood:  return "DarkBrotherhood";
        case RE::QUEST_DATA::Type::kCompanionsQuest:  return "Companions";
        case RE::QUEST_DATA::Type::kMiscellaneous:    return "Miscellaneous";
        case RE::QUEST_DATA::Type::kDaedric:          return "Daedric";
        case RE::QUEST_DATA::Type::kSideQuest:        return "SideQuest";
        case RE::QUEST_DATA::Type::kCivilWar:         return "CivilWar";
        case RE::QUEST_DATA::Type::kDLC01_Vampire:    return "DLC01_Vampire";
        case RE::QUEST_DATA::Type::kDLC02_Dragonborn: return "DLC02_Dragonborn";
        default:                                      return "None";
        }
    }

    const char* ObjectiveStateName(RE::QUEST_OBJECTIVE_STATE state)
    {
        switch (state) {
        case RE::QUEST_OBJECTIVE_STATE::kDormant:            return "Dormant";
        case RE::QUEST_OBJECTIVE_STATE::kDisplayed:          return "Displayed";
        case RE::QUEST_OBJECTIVE_STATE::kCompleted:          return "Completed";
        case RE::QUEST_OBJECTIVE_STATE::kCompletedDisplayed: return "CompletedDisplayed";
        case RE::QUEST_OBJECTIVE_STATE::kFailed:             return "Failed";
        case RE::QUEST_OBJECTIVE_STATE::kFailedDisplayed:    return "FailedDisplayed";
        default:                                             return "Unknown";
        }
    }

    bool IsObjectiveCompleted(RE::QUEST_OBJECTIVE_STATE state)
    {
        return state == RE::QUEST_OBJECTIVE_STATE::kCompleted ||
            state == RE::QUEST_OBJECTIVE_STATE::kCompletedDisplayed;
    }

    bool IsObjectiveFailed(RE::QUEST_OBJECTIVE_STATE state)
    {
        return state == RE::QUEST_OBJECTIVE_STATE::kFailed ||
            state == RE::QUEST_OBJECTIVE_STATE::kFailedDisplayed;
    }

    bool IsObjectiveVisibleInJournal(RE::QUEST_OBJECTIVE_STATE state)
    {
        return state == RE::QUEST_OBJECTIVE_STATE::kDisplayed ||
            state == RE::QUEST_OBJECTIVE_STATE::kCompleted ||
            state == RE::QUEST_OBJECTIVE_STATE::kCompletedDisplayed ||
            state == RE::QUEST_OBJECTIVE_STATE::kFailed ||
            state == RE::QUEST_OBJECTIVE_STATE::kFailedDisplayed;
    }

    std::uint32_t FindObjectiveInstanceID(RE::PlayerCharacter* player,
                                          RE::BGSQuestObjective* objective)
    {
        if (!player || !objective || REL::Module::IsVR())
            return 0;

        const auto base = reinterpret_cast<std::uintptr_t>(player);
        const std::size_t off = REL::Module::IsAE() ? 0x588 : 0x580;
        const auto& instances =
            *reinterpret_cast<const RE::BSTArray<RE::BGSInstancedQuestObjective>*>(base + off);

        for (const auto& inst : instances) {
            if (inst.Objective == objective &&
                inst.InstanceState == RE::QUEST_OBJECTIVE_STATE::kDisplayed) {
                return inst.instanceID;
            }
        }
        for (const auto& inst : instances) {
            if (inst.Objective == objective)
                return inst.instanceID;
        }
        return 0;
    }

    std::string ResolveText(RE::TESQuest* quest,
                            std::string_view raw,
                            std::uint32_t instanceID)
    {
        std::string out;
        out.reserve(raw.size());

        std::size_t i = 0;
        while (i < raw.size()) {
            const auto start = raw.find('<', i);
            if (start == std::string_view::npos) {
                out.append(raw.substr(i));
                break;
            }

            out.append(raw.substr(i, start - i));
            const auto end = raw.find('>', start + 1);
            if (end == std::string_view::npos) {
                out.append(raw.substr(start));
                break;
            }

            const auto token = raw.substr(start + 1, end - start - 1);
            const auto eq = token.find('=');
            bool replaced = false;
            if (eq != std::string_view::npos) {
                const auto head = TrimAscii(token.substr(0, eq));
                const auto name = TrimAscii(token.substr(eq + 1));

                if (IsAliasTokenHead(head)) {
                    if (auto* alias = FindAliasByName(quest, name)) {
                        std::string replacement = ResolveAliasDisplayName(quest, alias, instanceID);
                        if (!replacement.empty()) {
                            logger::debug("[QuestText] resolved token '<{}>' -> '{}' (quest=0x{:08X}, instance={})",
                                          std::string(token), replacement,
                                          quest ? quest->GetFormID() : 0,
                                          instanceID);
                            out += replacement;
                            replaced = true;
                        }
                    }
                }
            }

            if (!replaced) {
                out.push_back('<');
                out.append(token);
                out.push_back('>');
            }
            i = end + 1;
        }

        return out;
    }

    std::string ResolveQuestName(RE::TESQuest* quest,
                                 std::uint32_t instanceID)
    {
        if (!quest)
            return {};

        const char* raw = quest->GetFullName();
        if (!raw || !*raw)
            return {};
        return ResolveText(quest, raw, instanceID ? instanceID : quest->currentInstanceID);
    }
}