#include "PlayerRecords.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace logger = SKSE::log;

namespace PlayerRecords
{
    namespace
    {
        struct StatDef
        {
            std::string_view category;
            std::string_view name;  // Engine (English) stat name used by QueryStat
        };

        // Stat names from the Creation Kit "QueryStat" list, in journal order.
        // Text stats (Favorite Weapon/Spell/School/Shout) are not numbers and
        // are left out.
        // clang-format off
        constexpr std::array kStats = {
            StatDef{ "General", "Locations Discovered" }, StatDef{ "General", "Dungeons Cleared" },
            StatDef{ "General", "Days Passed" }, StatDef{ "General", "Hours Slept" },
            StatDef{ "General", "Hours Waiting" }, StatDef{ "General", "Standing Stones Found" },
            StatDef{ "General", "Gold Found" }, StatDef{ "General", "Most Gold Carried" },
            StatDef{ "General", "Chests Looted" }, StatDef{ "General", "Skill Increases" },
            StatDef{ "General", "Skill Books Read" }, StatDef{ "General", "Food Eaten" },
            StatDef{ "General", "Training Sessions" }, StatDef{ "General", "Books Read" },
            StatDef{ "General", "Horses Owned" }, StatDef{ "General", "Houses Owned" },
            StatDef{ "General", "Stores Invested In" }, StatDef{ "General", "Barters" },
            StatDef{ "General", "Persuasions" }, StatDef{ "General", "Bribes" },
            StatDef{ "General", "Intimidations" }, StatDef{ "General", "Diseases Contracted" },
            StatDef{ "General", "Days as a Vampire" }, StatDef{ "General", "Days as a Werewolf" },
            StatDef{ "General", "Necks Bitten" }, StatDef{ "General", "Vampirism Cures" },
            StatDef{ "General", "Werewolf Transformations" }, StatDef{ "General", "Mauls" },

            StatDef{ "Quest", "Quests Completed" }, StatDef{ "Quest", "Misc Objectives Completed" },
            StatDef{ "Quest", "Main Quests Completed" }, StatDef{ "Quest", "Side Quests Completed" },
            StatDef{ "Quest", "The Companions Quests Completed" },
            StatDef{ "Quest", "College of Winterhold Quests Completed" },
            StatDef{ "Quest", "Thieves' Guild Quests Completed" },
            StatDef{ "Quest", "The Dark Brotherhood Quests Completed" },
            StatDef{ "Quest", "Civil War Quests Completed" }, StatDef{ "Quest", "Daedric Quests Completed" },
            StatDef{ "Quest", "Dawnguard Quests Completed" }, StatDef{ "Quest", "Dragonborn Quests Completed" },
            StatDef{ "Quest", "Questlines Completed" },

            StatDef{ "Combat", "People Killed" }, StatDef{ "Combat", "Animals Killed" },
            StatDef{ "Combat", "Creatures Killed" }, StatDef{ "Combat", "Undead Killed" },
            StatDef{ "Combat", "Daedra Killed" }, StatDef{ "Combat", "Automatons Killed" },
            StatDef{ "Combat", "Critical Strikes" }, StatDef{ "Combat", "Sneak Attacks" },
            StatDef{ "Combat", "Backstabs" }, StatDef{ "Combat", "Weapons Disarmed" },
            StatDef{ "Combat", "Brawls Won" }, StatDef{ "Combat", "Bunnies Slaughtered" },

            StatDef{ "Magic", "Spells Learned" }, StatDef{ "Magic", "Dragon Souls Collected" },
            StatDef{ "Magic", "Words Of Power Learned" }, StatDef{ "Magic", "Words Of Power Unlocked" },
            StatDef{ "Magic", "Shouts Learned" }, StatDef{ "Magic", "Shouts Unlocked" },
            StatDef{ "Magic", "Shouts Mastered" }, StatDef{ "Magic", "Times Shouted" },

            StatDef{ "Crafting", "Soul Gems Used" }, StatDef{ "Crafting", "Souls Trapped" },
            StatDef{ "Crafting", "Magic Items Made" }, StatDef{ "Crafting", "Weapons Improved" },
            StatDef{ "Crafting", "Weapons Made" }, StatDef{ "Crafting", "Armor Improved" },
            StatDef{ "Crafting", "Armor Made" }, StatDef{ "Crafting", "Potions Mixed" },
            StatDef{ "Crafting", "Potions Used" }, StatDef{ "Crafting", "Poisons Mixed" },
            StatDef{ "Crafting", "Poisons Used" }, StatDef{ "Crafting", "Ingredients Harvested" },
            StatDef{ "Crafting", "Ingredients Eaten" }, StatDef{ "Crafting", "Nirnroots Found" },
            StatDef{ "Crafting", "Wings Plucked" },

            StatDef{ "Crime", "Total Lifetime Bounty" }, StatDef{ "Crime", "Largest Bounty" },
            StatDef{ "Crime", "Locks Picked" }, StatDef{ "Crime", "Pockets Picked" },
            StatDef{ "Crime", "Items Pickpocketed" }, StatDef{ "Crime", "Times Jailed" },
            StatDef{ "Crime", "Days Jailed" }, StatDef{ "Crime", "Fines Paid" },
            StatDef{ "Crime", "Jail Escapes" }, StatDef{ "Crime", "Items Stolen" },
            StatDef{ "Crime", "Assaults" }, StatDef{ "Crime", "Murders" },
            StatDef{ "Crime", "Horses Stolen" }, StatDef{ "Crime", "Trespasses" },
            StatDef{ "Crime", "Eastmarch Bounty" }, StatDef{ "Crime", "Falkreath Bounty" },
            StatDef{ "Crime", "Haafingar Bounty" }, StatDef{ "Crime", "Hjaalmarch Bounty" },
            StatDef{ "Crime", "The Pale Bounty" }, StatDef{ "Crime", "The Reach Bounty" },
            StatDef{ "Crime", "The Rift Bounty" }, StatDef{ "Crime", "Tribal Orcs Bounty" },
            StatDef{ "Crime", "Whiterun Bounty" }, StatDef{ "Crime", "Winterhold Bounty" },
        };
        // clang-format on

        constexpr std::array<std::string_view, 6> kCategories = { "General", "Quest", "Combat", "Magic", "Crafting", "Crime" };
        constexpr auto kRefreshInterval = std::chrono::seconds(3);

        std::mutex                                   s_mutex;
        std::unordered_map<std::string, std::int32_t> s_values;
        std::chrono::steady_clock::time_point        s_lastRefresh{};

        // Receives one Game.QueryStat result on the Papyrus VM thread.
        class StatCallback final : public RE::BSScript::IStackCallbackFunctor
        {
        public:
            explicit StatCallback(std::string a_name) : name(std::move(a_name)) {}

            void operator()(RE::BSScript::Variable a_result) override
            {
                if (!a_result.IsInt())
                    return;
                std::scoped_lock lock(s_mutex);
                s_values[name] = a_result.GetSInt();
            }

            void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

        private:
            std::string name;
        };

        void RefreshAsync()
        {
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!vm)
                return;
            const RE::BSFixedString className("Game");
            const RE::BSFixedString fnName("QueryStat");
            for (const auto& stat : kStats) {
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback =
                    RE::make_smart<StatCallback>(std::string(stat.name));
                auto* args = RE::MakeFunctionArguments(RE::BSFixedString(std::string(stat.name)));
                vm->DispatchStaticCall(className, fnName, args, callback);
            }
        }
    }

    nlohmann::json ReadMiscStats()
    {
        const auto now = std::chrono::steady_clock::now();
        bool refresh = false;
        {
            std::scoped_lock lock(s_mutex);
            if (now - s_lastRefresh >= kRefreshInterval) {
                s_lastRefresh = now;
                refresh = true;
            }
        }
        if (refresh) {
            try {
                RefreshAsync();
            } catch (...) {
                logger::warn("[PlayerRecords] QueryStat dispatch failed");
            }
        }

        nlohmann::json result = nlohmann::json::object();
        for (const auto category : kCategories)
            result[std::string(category)] = nlohmann::json::array();

        std::scoped_lock lock(s_mutex);
        for (const auto& stat : kStats) {
            const auto it = s_values.find(std::string(stat.name));
            if (it == s_values.end())
                continue;
            result[std::string(stat.category)].push_back({ { "name", stat.name }, { "value", it->second } });
        }
        return result;
    }

    nlohmann::json ReadActiveEffects()
    {
        nlohmann::json result = nlohmann::json::array();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return result;
        auto* target = player->AsMagicTarget();
        auto* list   = target ? target->GetActiveEffectList() : nullptr;
        if (!list)
            return result;

        for (auto* effect : *list) {
            if (!effect)
                continue;
            if (effect->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled))
                continue;
            auto* base = effect->GetBaseObject();
            if (!base || base->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI))
                continue;

            const char* name   = base->GetFullName();
            const char* source = effect->spell ? effect->spell->GetFullName() : nullptr;
            const float duration  = effect->duration;
            const float elapsed   = effect->elapsedSeconds;

            nlohmann::json j;
            j["name"]        = name ? name : "";
            j["source"]      = source ? source : "";
            j["magnitude"]   = effect->magnitude;
            j["duration"]    = duration;
            j["elapsed"]     = elapsed;
            j["remaining"]   = duration > 0.f ? std::max(0.f, duration - elapsed) : 0.f;
            j["detrimental"] = base->IsDetrimental() || base->IsHostile();
            result.push_back(std::move(j));
        }
        return result;
    }
}
