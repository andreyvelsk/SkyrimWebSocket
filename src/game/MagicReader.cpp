#include "MagicReader.h"
#include "Common.h"

#include <format>
#include <unordered_map>

namespace logger = SKSE::log;

namespace MagicReader
{
    // ─── School metadata ──────────────────────────────────────────────────

    struct SchoolInfo
    {
        std::string categoryId;
    };

    // clang-format off
    static const std::unordered_map<RE::ActorValue, SchoolInfo> s_schools = {
        { RE::ActorValue::kDestruction, { "Destruction" } },
        { RE::ActorValue::kAlteration,  { "Alteration"  } },
        { RE::ActorValue::kConjuration, { "Conjuration" } },
        { RE::ActorValue::kIllusion,    { "Illusion"    } },
        { RE::ActorValue::kRestoration, { "Restoration" } },
        { RE::ActorValue::kEnchanting,  { "Enchanting"  } },
    };
    // clang-format on

    // ─── Private helpers ──────────────────────────────────────────────────

    // Returns the localized display name for a magic school ActorValue.
    // Uses ActorValueList → TESActorValueInfo → TESFullName, which goes through
    // Skyrim SE's BSStringPool / string-file system and returns the correct
    // translated name regardless of the current game language.
    static std::string GetSchoolLocalName(RE::ActorValue school, const std::string& fallback)
    {
        auto* avList = RE::ActorValueList::GetSingleton();
        if (!avList)
            return fallback;
        auto* avInfo = RE::ActorValueList::GetActorValueInfo(school);
        if (!avInfo)
            return fallback;
        const char* name = avInfo->GetFullName();
        return (name && *name != '\0') ? name : fallback;
    }

    // Look up a single key in the Scaleform GFx translation table.
    // Translation tables are populated from all loaded Interface/Translations/*.txt
    // files (vanilla Skyrim, SkyUI, and any other mod that ships a translation file).
    // Keys conventionally start with '$'.
    static std::string LookupInterfaceString(const RE::BSFixedStringW& key)
    {
        const auto* mgr = RE::BSScaleformManager::GetSingleton();
        if (!mgr)
            return {};

        // Try to obtain the translator from two possible sources:
        // 1. The GFxLoader state (legacy path)
        // 2. The direct BSScaleformManager::translator member (more reliable)
        // GetState returns GPtr<BSScaleformTranslator>, mgr->translator is raw pointer
        const RE::BSScaleformTranslator* translator = nullptr;
        if (mgr->loader) {
            auto state =
                mgr->loader->GetState<RE::BSScaleformTranslator>(RE::GFxState::StateType::kTranslator);
            if (state) {
                translator = state.get();
            }
        }
        if (!translator) {
            translator = mgr->translator;
        }
        if (!translator)
            return {};

        // Try exact key first (e.g. L"$Shouts")
        {
            const auto it = translator->translator.translationMap.find(key);
            if (it != translator->translator.translationMap.end()) {
                const wchar_t* val = it->second.c_str();
                if (val && *val != L'\0')
                    return Common::WcsToUtf8(val);
            }
        }

        // If key starts with '$', also try without the prefix — some translation
        // file parsers store keys both with and without the sigil.
        {
            const wchar_t* k = key.c_str();
            if (k && k[0] == L'$' && k[1] != L'\0') {
                RE::BSFixedStringW stripped(k + 1);
                const auto it = translator->translator.translationMap.find(stripped);
                if (it != translator->translator.translationMap.end()) {
                    const wchar_t* val = it->second.c_str();
                    if (val && *val != L'\0')
                        return Common::WcsToUtf8(val);
                }
            }
        }

        return {};
    }

    // Try each candidate key in order; return the first non-empty hit, or fallback.
    static std::string GetInterfaceName(
        std::initializer_list<const wchar_t*> keys, const char* fallback)
    {
        for (const wchar_t* k : keys) {
            std::string s = LookupInterfaceString(RE::BSFixedStringW(k));
            if (!s.empty())
                return s;
        }
        return fallback;
    }

    // Resolve a localized category name by trying multiple sources in order:
    //   1. GameSettingCollection (GMST) – most reliable, same as ActorValue path
    //   2. Scaleform translation table   – fallback for mod-provided strings
    //   3. Hardcoded fallback
    // `gmstKeys` is a list of GMST key candidates (e.g. {"sMagicShouts", "sShouts"}).
    static std::string GetLocalizedName(
        std::initializer_list<const char*> gmstKeys,
        std::initializer_list<const wchar_t*> interfaceKeys,
        const char* fallback)
    {
        // 1) Try GameSetting strings first — these are the vanilla localised
        //    strings and Are always available regardless of Scaleform state.
        for (const char* gmstKey : gmstKeys) {
            std::string s = Common::GetGMSTString(gmstKey);
            if (!s.empty())
                return s;
        }

        // 2) Try Scaleform translation table.
        std::string s = GetInterfaceName(interfaceKeys, "");
        if (!s.empty())
            return s;

        // 3) Fallback.
        return fallback;
    }

    static void ReplaceAll(std::string& str, const std::string_view from, const std::string& to)
    {
        for (std::size_t pos = 0; (pos = str.find(from, pos)) != std::string::npos; pos += to.size())
            str.replace(pos, from.size(), to);
    }

    // Format a float: integer when no fractional part, otherwise one decimal place.
    // Matches vanilla Skyrim inventory display convention.
    static std::string FormatMagnitude(float v)
    {
        float intpart;
        if (std::modf(v, &intpart) == 0.f)
            return std::to_string(static_cast<int>(intpart));
        return std::format("{:.1f}", v);
    }

    static nlohmann::json BuildEffectJson(const RE::Effect* eff)
    {
        nlohmann::json j;
        if (!eff || !eff->baseEffect) {
            j["name"]                = "";
            j["magnitude"]           = 0.f;
            j["duration"]            = 0u;
            j["descriptionTemplate"] = "";
            j["description"]         = "";
            return j;
        }

        j["name"]      = eff->baseEffect->GetName();
        j["magnitude"] = eff->effectItem.magnitude;
        j["duration"]  = eff->effectItem.duration;

        // EffectSetting stores the localized description in magicItemDescription (DNAM).
        const auto& desc = eff->baseEffect->magicItemDescription;
        std::string tmpl = desc.empty() ? "" : std::string(desc.c_str());
        j["descriptionTemplate"] = tmpl;

        std::string resolved = tmpl;
        ReplaceAll(resolved, "<mag>", FormatMagnitude(eff->effectItem.magnitude));
        ReplaceAll(resolved, "<dur>", std::to_string(eff->effectItem.duration));
        j["description"] = std::move(resolved);

        return j;
    }

    static nlohmann::json BuildEffectsArray(const RE::MagicItem* magic)
    {
        nlohmann::json effects = nlohmann::json::array();
        if (magic) {
            logger::trace("[BuildEffectsArray] magic=0x{:016X} effects.size()={}",
                reinterpret_cast<std::uintptr_t>(magic), magic->effects.size());
            for (std::uint32_t i = 0; i < magic->effects.size(); ++i) {
                const auto* eff = magic->effects[i];
                logger::trace("[BuildEffectsArray] effect[{}] eff=0x{:016X} baseEffect=0x{:016X}",
                    i,
                    reinterpret_cast<std::uintptr_t>(eff),
                    eff ? reinterpret_cast<std::uintptr_t>(eff->baseEffect) : 0uLL);
                if (!eff || !eff->baseEffect)
                    continue;
                effects.push_back(BuildEffectJson(eff));
            }
        }
        return effects;
    }

    static const char* CastingTypeToString(RE::MagicSystem::CastingType type)
    {
        switch (type) {
            case RE::MagicSystem::CastingType::kConstantEffect: return "ConstantEffect";
            case RE::MagicSystem::CastingType::kFireAndForget:  return "FireAndForget";
            case RE::MagicSystem::CastingType::kConcentration:  return "Concentration";
            case RE::MagicSystem::CastingType::kScroll:         return "Scroll";
            default:                                            return "Unknown";
        }
    }

    static const char* DeliveryToString(RE::MagicSystem::Delivery delivery)
    {
        switch (delivery) {
            case RE::MagicSystem::Delivery::kSelf:           return "Self";
            case RE::MagicSystem::Delivery::kTouch:          return "Touch";
            case RE::MagicSystem::Delivery::kAimed:          return "Aimed";
            case RE::MagicSystem::Delivery::kTargetActor:    return "TargetActor";
            case RE::MagicSystem::Delivery::kTargetLocation: return "TargetLocation";
            default:                                         return "Unknown";
        }
    }

    // Returns which hands (if any) the spell is currently equipped for casting.
    // Checks selectedSpells[] in ACTOR_RUNTIME_DATA — the HUD-visible equipped slot,
    // not currentSpell which is only set while actively casting.
    // Returns nullptr, "left", "right", or "both".
    static nlohmann::json GetEquippedHand(RE::SpellItem* spell, RE::PlayerCharacter* player)
    {
        const auto& rt     = player->GetActorRuntimeData();
        const bool  inLeft  = rt.selectedSpells[RE::Actor::SlotTypes::kLeftHand]  == spell;
        const bool  inRight = rt.selectedSpells[RE::Actor::SlotTypes::kRightHand] == spell;

        if (inLeft && inRight) return "both";
        if (inRight)           return "right";
        if (inLeft)            return "left";
        return nullptr;
    }

    // Builds the JSON object for a single known spell.
    // Must be called on the game thread.
    static nlohmann::json BuildSpellEntry(
        RE::SpellItem*       spell,
        const std::string&   categoryType,
        RE::MagicFavorites*  favorites,
        RE::PlayerCharacter* player)
    {
        nlohmann::json j;

        logger::trace("[BuildSpellEntry] enter spell=0x{:016X} formId=0x{:08X} category={}",
            reinterpret_cast<std::uintptr_t>(spell), spell->GetFormID(), categoryType);

        j["name"]         = spell->GetName();
        j["formId"]       = std::format("0x{:08X}", spell->GetFormID());
        j["categoryType"] = categoryType;

        logger::trace("[BuildSpellEntry] 0x{:08X} name='{}'",
            spell->GetFormID(), static_cast<std::string>(j["name"]));

        // cost: real in-game magicka cost with player skill/perk modifiers applied.
        logger::trace("[BuildSpellEntry] 0x{:08X} calling CalculateMagickaCost(player)", spell->GetFormID());
        j["cost"] = static_cast<int32_t>(spell->CalculateMagickaCost(player));
        logger::trace("[BuildSpellEntry] 0x{:08X} cost={}", spell->GetFormID(), j["cost"].get<int32_t>());

        // costValue: raw base cost — costOverride when explicitly set, otherwise the
        // with-player calculation (calling CalculateMagickaCost(nullptr) is unsafe on
        // some platforms/versions and has been replaced with the player-based call).
        logger::trace("[BuildSpellEntry] 0x{:08X} costOverride={}", spell->GetFormID(), spell->data.costOverride);
        const int32_t costBase = (spell->data.costOverride >= 0)
                                     ? spell->data.costOverride
                                     : static_cast<int32_t>(spell->CalculateMagickaCost(player));
        j["costValue"] = costBase;
        logger::trace("[BuildSpellEntry] 0x{:08X} costValue={}", spell->GetFormID(), costBase);

        // level: minimum school skill required (0=Novice, 25=Apprentice, 50=Adept,
        // 75=Expert, 100=Master).  Taken from the costliest effect's base setting.
        logger::trace("[BuildSpellEntry] 0x{:08X} calling GetCostliestEffectItem", spell->GetFormID());
        int32_t level = 0;
        const auto* costliestEff = spell->GetCostliestEffectItem();
        logger::trace("[BuildSpellEntry] 0x{:08X} costliestEff=0x{:016X}",
            spell->GetFormID(), reinterpret_cast<std::uintptr_t>(costliestEff));
        if (costliestEff && costliestEff->baseEffect)
            level = costliestEff->baseEffect->GetMinimumSkillLevel();
        j["level"] = level;
        logger::trace("[BuildSpellEntry] 0x{:08X} level={}", spell->GetFormID(), level);

        j["castingType"] = CastingTypeToString(spell->data.castingType);
        j["delivery"]    = DeliveryToString(spell->data.delivery);
        j["range"]       = spell->data.range;
        j["chargeTime"]  = spell->data.chargeTime;

        logger::trace("[BuildSpellEntry] 0x{:08X} calling BuildEffectsArray", spell->GetFormID());
        j["effects"]     = BuildEffectsArray(spell);
        logger::trace("[BuildSpellEntry] 0x{:08X} effects done count={}",
            spell->GetFormID(), j["effects"].size());

        // Equipped hand: which casting slot (if any) has this spell ready.
        logger::trace("[BuildSpellEntry] 0x{:08X} calling GetEquippedHand", spell->GetFormID());
        auto hand       = GetEquippedHand(spell, player);
        j["isEquipped"] = !hand.is_null();
        j["equippedHand"] = std::move(hand);
        logger::trace("[BuildSpellEntry] 0x{:08X} isEquipped={}",
            spell->GetFormID(), j["isEquipped"].get<bool>());

        // isActive: currently being cast by the player.
        logger::trace("[BuildSpellEntry] 0x{:08X} calling IsCasting", spell->GetFormID());
        j["isActive"] = player->IsCasting(spell);
        logger::trace("[BuildSpellEntry] 0x{:08X} isActive={}",
            spell->GetFormID(), j["isActive"].get<bool>());

        // Hotkeys: collect all number-key slot indices (0-7) for this spell.
        logger::trace("[BuildSpellEntry] 0x{:08X} building hotkeys favorites=0x{:016X}",
            spell->GetFormID(), reinterpret_cast<std::uintptr_t>(favorites));
        nlohmann::json hotkeys = nlohmann::json::array();
        if (favorites) {
            for (int i = 0; i < 8; ++i) {
                if (favorites->hotkeys[i] == spell)
                    hotkeys.push_back(i);
            }
        }
        j["hotkeys"] = std::move(hotkeys);

        // isFavorite: is this spell marked as a favorite.
        logger::trace("[BuildSpellEntry] 0x{:08X} checking isFavorite", spell->GetFormID());
        bool isFavorite = false;
        if (favorites) {
            for (const auto* fav : favorites->spells) {
                if (fav == spell) {
                    isFavorite = true;
                    break;
                }
            }
        }
        j["isFavorite"] = isFavorite;

        logger::trace("[BuildSpellEntry] 0x{:08X} done", spell->GetFormID());
        return j;
    }

    // Generic per-school reader.
    // Iterates both the NPC base spell list and runtime addedSpells (learned via tomes).
    static nlohmann::json ReadSchool(RE::ActorValue school)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        const std::string& categoryType = s_schools.at(school).categoryId;
        auto*              favorites    = RE::MagicFavorites::GetSingleton();

        logger::trace("[ReadSchool] school={} player=0x{:016X} favorites=0x{:016X}",
            categoryType,
            reinterpret_cast<std::uintptr_t>(player),
            reinterpret_cast<std::uintptr_t>(favorites));

        nlohmann::json result = nlohmann::json::array();

        auto tryAdd = [&](RE::SpellItem* spell) {
            if (!spell)
                return;
            logger::trace("[ReadSchool::{}] candidate spell=0x{:016X} formId=0x{:08X}",
                categoryType,
                reinterpret_cast<std::uintptr_t>(spell),
                spell->GetFormID());
            // Only regular castable spells — skip powers, diseases, abilities, etc.
            const auto spellType = spell->GetSpellType();
            logger::trace("[ReadSchool::{}] 0x{:08X} spellType={}",
                categoryType, spell->GetFormID(), static_cast<int>(spellType));
            if (spellType != RE::MagicSystem::SpellType::kSpell)
                return;
            const auto assocSkill = spell->GetAssociatedSkill();
            logger::trace("[ReadSchool::{}] 0x{:08X} associatedSkill={}",
                categoryType, spell->GetFormID(), static_cast<int>(assocSkill));
            if (assocSkill != school)
                return;
            logger::trace("[ReadSchool::{}] 0x{:08X} passes filter, building entry",
                categoryType, spell->GetFormID());
            result.push_back(BuildSpellEntry(spell, categoryType, favorites, player));
            logger::trace("[ReadSchool::{}] 0x{:08X} entry added OK",
                categoryType, spell->GetFormID());
        };

        // 1) Spells baked into the player's base NPC form.
        auto* npc       = player->GetActorBase();
        auto* spellData = npc ? npc->GetSpellList() : nullptr;
        logger::trace("[ReadSchool::{}] npc=0x{:016X} spellData=0x{:016X}",
            categoryType,
            reinterpret_cast<std::uintptr_t>(npc),
            reinterpret_cast<std::uintptr_t>(spellData));
        if (spellData) {
            logger::trace("[ReadSchool::{}] base numSpells={}", categoryType, spellData->numSpells);
            for (std::uint32_t i = 0; i < spellData->numSpells; ++i)
                tryAdd(spellData->spells[i]);
        }

        // 2) Spells learned at runtime (spell tomes, AddSpell(), console, etc.).
        const auto& addedSpells = player->GetActorRuntimeData().addedSpells;
        logger::trace("[ReadSchool::{}] addedSpells.size()={}", categoryType, addedSpells.size());
        for (auto* spell : addedSpells)
            tryAdd(spell);

        logger::trace("[ReadSchool::{}] returning {} entries", categoryType, result.size());
        return result;
    }

    // ─── Public API ───────────────────────────────────────────────────────

    nlohmann::json ReadCategories()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        std::unordered_map<RE::ActorValue, int32_t> counts;

        auto tryCount = [&](RE::SpellItem* spell) {
            if (!spell)
                return;
            if (spell->GetSpellType() != RE::MagicSystem::SpellType::kSpell)
                return;
            const auto school = spell->GetAssociatedSkill();
            if (s_schools.find(school) == s_schools.end())
                return;
            ++counts[school];
        };

        // 1) Spells baked into the player's base NPC form.
        auto* npc       = player->GetActorBase();
        auto* spellData = npc ? npc->GetSpellList() : nullptr;
        if (spellData) {
            for (std::uint32_t i = 0; i < spellData->numSpells; ++i)
                tryCount(spellData->spells[i]);
        }

        // 2) Spells learned at runtime (spell tomes, AddSpell(), console, etc.).
        for (auto* spell : player->GetActorRuntimeData().addedSpells)
            tryCount(spell);

        nlohmann::json result = nlohmann::json::array();
        for (auto& [school, count] : counts) {
            const auto& info = s_schools.at(school);
            result.push_back({
                { "categoryId", info.categoryId                              },
                { "name",       GetSchoolLocalName(school, info.categoryId)  },
                { "count",      count                                        },
            });
        }

        // Shouts (TESShout, separate from the SpellItem lists).
        // The name is resolved by trying:
        //   1. GameSetting strings (GMST) – e.g. sMagicShouts, sShouts
        //   2. Scaleform translation table – for mod-provided strings
        //   3. Hardcoded fallback ("Shouts")
        if (spellData && spellData->numShouts > 0)
            result.push_back({
                { "categoryId", "Shouts" },
                { "name",       GetLocalizedName(
                                    { "sMagicShouts", "sShouts", "sShoutSchool", "sShoutTab" },
                                    { L"$Shouts", L"$ShoutGroup", L"$ShoutTab",
                                      L"$SHOUTS", L"$MagicShout" },
                                    "Shouts") },
                { "count",      static_cast<int32_t>(spellData->numShouts) },
            });

        // Powers (greater) and lesser powers — SpellItem with matching type.
        int32_t powerCount = 0, lesserPowerCount = 0;
        auto countPower = [&](RE::SpellItem* spell) {
            if (!spell) return;
            switch (spell->GetSpellType()) {
                case RE::MagicSystem::SpellType::kPower:       ++powerCount;       break;
                case RE::MagicSystem::SpellType::kLesserPower: ++lesserPowerCount; break;
                default: break;
            }
        };
        if (spellData) {
            for (std::uint32_t i = 0; i < spellData->numSpells; ++i)
                countPower(spellData->spells[i]);
        }
        for (auto* spell : player->GetActorRuntimeData().addedSpells)
            countPower(spell);

        if (powerCount > 0)
            result.push_back({
                { "categoryId", "Powers" },
                { "name",       GetLocalizedName(
                                    { "sMagicPowers", "sPowers", "sPowerSchool", "sPowerTab" },
                                    { L"$Powers", L"$PowerGroup", L"$PowerTab",
                                      L"$POWERS", L"$MagicPower" },
                                    "Powers") },
                { "count",      powerCount },
            });
        if (lesserPowerCount > 0)
            result.push_back({
                { "categoryId", "LesserPowers" },
                { "name",       GetLocalizedName(
                                    { "sMagicLesserPowers", "sLesserPowers", "sLesserPowerSchool", "sLesserPowerTab" },
                                    { L"$LesserPowers", L"$LesserPowerGroup",
                                      L"$LesserPowerTab", L"$LESSER_POWERS",
                                      L"$MagicLesserPower" },
                                    "Lesser Powers") },
                { "count",      lesserPowerCount },
            });

        return result;
    }

    nlohmann::json ReadDestruction() { return ReadSchool(RE::ActorValue::kDestruction); }
    nlohmann::json ReadAlteration()  { return ReadSchool(RE::ActorValue::kAlteration);  }
    nlohmann::json ReadConjuration() { return ReadSchool(RE::ActorValue::kConjuration); }
    nlohmann::json ReadIllusion()    { return ReadSchool(RE::ActorValue::kIllusion);    }
    nlohmann::json ReadRestoration() { return ReadSchool(RE::ActorValue::kRestoration); }
    nlohmann::json ReadEnchanting()  { return ReadSchool(RE::ActorValue::kEnchanting);  }

    // ─── Shouts ───────────────────────────────────────────────────────────

    // Builds the JSON object for a single known dragon shout.
    static nlohmann::json BuildShoutEntry(
        RE::TESShout*        shout,
        RE::MagicFavorites*  favorites,
        RE::PlayerCharacter* player)
    {
        nlohmann::json j;
        j["name"]   = shout->GetName();
        j["formId"] = std::format("0x{:08X}", shout->GetFormID());

        // Shout description from TESDescription (DNAM field).
        if (auto* desc = shout->As<RE::TESDescription>()) {
            RE::BSString buf;
            desc->GetDescription(buf, shout);
            j["description"] = buf.empty() ? "" : std::string(buf);
        } else {
            j["description"] = "";
        }

        // Words of power (up to 3 variations; word may be null for unused slots).
        nlohmann::json words = nlohmann::json::array();
        for (std::uint32_t i = 0; i < 3; ++i) {
            const auto& var = shout->variations[i];
            if (!var.word)
                continue;
            nlohmann::json w;
            w["name"]         = var.word->GetName();
            w["formId"]       = std::format("0x{:08X}", var.word->GetFormID());
            w["recoveryTime"] = var.recoveryTime;
            w["isKnown"]      = var.word->GetKnown();
            words.push_back(std::move(w));
        }
        j["words"] = std::move(words);

        // isEquipped: this shout is the currently selected voice power.
        const auto& rt      = player->GetActorRuntimeData();
        const bool  equip   = (rt.selectedPower == static_cast<RE::TESForm*>(shout));
        j["isEquipped"]     = equip;

        // Hotkeys: indices (0-7) of hotkey slots assigned to this shout.
        nlohmann::json hotkeys = nlohmann::json::array();
        if (favorites) {
            for (int i = 0; i < 8; ++i) {
                if (i < static_cast<int>(favorites->hotkeys.size()) &&
                    favorites->hotkeys[i] == static_cast<RE::TESForm*>(shout))
                    hotkeys.push_back(i);
            }
        }
        j["hotkeys"] = std::move(hotkeys);

        // isFavorite: is this shout in the magic favorites list.
        bool isFavorite = false;
        if (favorites) {
            for (const auto* fav : favorites->spells) {
                if (fav == static_cast<RE::TESForm*>(shout)) {
                    isFavorite = true;
                    break;
                }
            }
        }
        j["isFavorite"] = isFavorite;

        return j;
    }

    nlohmann::json ReadShouts()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto* favorites = RE::MagicFavorites::GetSingleton();
        nlohmann::json result = nlohmann::json::array();

        auto* npc       = player->GetActorBase();
        auto* spellData = npc ? npc->GetSpellList() : nullptr;
        if (!spellData)
            return result;

        for (std::uint32_t i = 0; i < spellData->numShouts; ++i) {
            auto* shout = spellData->shouts[i];
            if (!shout)
                continue;
            result.push_back(BuildShoutEntry(shout, favorites, player));
        }
        return result;
    }

    // ─── Powers & Lesser Powers ───────────────────────────────────────────

    // Builds the JSON object for a single known power (greater or lesser).
    static nlohmann::json BuildPowerEntry(
        RE::SpellItem*       spell,
        RE::MagicFavorites*  favorites,
        RE::PlayerCharacter* player)
    {
        nlohmann::json j;
        j["name"]   = spell->GetName();
        j["formId"] = std::format("0x{:08X}", spell->GetFormID());

        j["spellType"] = (spell->GetSpellType() == RE::MagicSystem::SpellType::kLesserPower)
                             ? "LesserPower"
                             : "Power";

        // Powers always cost 0 magicka (they may have a cost of 0 in the data, but
        // CalculateMagickaCost will reflect that).
        j["cost"]    = static_cast<int32_t>(spell->CalculateMagickaCost(player));
        j["effects"] = BuildEffectsArray(spell);

        // isEquipped: this power is the currently selected voice power.
        const auto& rt  = player->GetActorRuntimeData();
        j["isEquipped"] = (rt.selectedPower == static_cast<RE::TESForm*>(spell));

        // Hotkeys: indices (0-7) of hotkey slots assigned to this power.
        nlohmann::json hotkeys = nlohmann::json::array();
        if (favorites) {
            for (int i = 0; i < 8; ++i) {
                if (i < static_cast<int>(favorites->hotkeys.size()) &&
                    favorites->hotkeys[i] == static_cast<RE::TESForm*>(spell))
                    hotkeys.push_back(i);
            }
        }
        j["hotkeys"] = std::move(hotkeys);

        // isFavorite: is this power in the magic favorites list.
        bool isFavorite = false;
        if (favorites) {
            for (const auto* fav : favorites->spells) {
                if (fav == static_cast<RE::TESForm*>(spell)) {
                    isFavorite = true;
                    break;
                }
            }
        }
        j["isFavorite"] = isFavorite;

        return j;
    }

    // Generic power reader filtered by spell type.
    static nlohmann::json ReadPowersByType(RE::MagicSystem::SpellType type)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto* favorites = RE::MagicFavorites::GetSingleton();
        nlohmann::json result = nlohmann::json::array();

        auto tryAdd = [&](RE::SpellItem* spell) {
            if (!spell)
                return;
            if (spell->GetSpellType() != type)
                return;
            result.push_back(BuildPowerEntry(spell, favorites, player));
        };

        // 1) Powers baked into the player's base NPC form.
        auto* npc       = player->GetActorBase();
        auto* spellData = npc ? npc->GetSpellList() : nullptr;
        if (spellData) {
            for (std::uint32_t i = 0; i < spellData->numSpells; ++i)
                tryAdd(spellData->spells[i]);
        }

        // 2) Powers added at runtime (AddSpell(), mods, console, etc.).
        for (auto* spell : player->GetActorRuntimeData().addedSpells)
            tryAdd(spell);

        return result;
    }

    nlohmann::json ReadPowers()       { return ReadPowersByType(RE::MagicSystem::SpellType::kPower);       }
    nlohmann::json ReadLesserPowers() { return ReadPowersByType(RE::MagicSystem::SpellType::kLesserPower); }
}
