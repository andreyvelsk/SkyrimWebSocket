#include "Common.h"

#include "../../logger.h"

#include <array>
#include <utility>

#include "RE/A/ActorValueList.h"
#include "RE/B/BGSEntryPoint.h"
#include "RE/M/MagicSystem.h"

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

    int32_t GetInventoryCount(RE::PlayerCharacter* player, RE::FormID formId)
    {
        auto inv = player->GetInventory([formId](RE::TESBoundObject& obj) {
            return obj.GetFormID() == formId;
        });
        if (inv.empty())
            return 0;
        return inv.begin()->second.first;
    }

    RE::InventoryEntryData* FindLiveEntry(RE::PlayerCharacter* player, RE::FormID formId)
    {
        auto* invChanges = player->GetInventoryChanges();
        if (!invChanges || !invChanges->entryList)
            return nullptr;
        for (auto* entry : *invChanges->entryList) {
            if (entry && entry->object && entry->object->GetFormID() == formId)
                return entry;
        }
        return nullptr;
    }

    RE::InventoryEntryData* MaterializeInventoryEntry(RE::PlayerCharacter* player,
                                                      RE::TESBoundObject*  form)
    {
        const RE::FormID formId = form->GetFormID();
        if (auto* entry = FindLiveEntry(player, formId))
            return entry;

        // Items that live only in the player's base TESContainer are visible
        // via GetInventory() but have no live InventoryEntryData in
        // InventoryChanges::entryList.  Force the engine to create a proper
        // entry by doing a neutral +1/-1 container transaction.
        player->AddObjectToContainer(form, nullptr, 1, nullptr);
        player->RemoveItem(form, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        return FindLiveEntry(player, formId);
    }

    bool PlayerKnowsSpell(RE::PlayerCharacter* player, RE::SpellItem* spell)
    {
        if (!player || !spell)
            return false;

        // Base NPC spell list (racial abilities etc.)
        if (auto* npc = player->GetActorBase()) {
            if (auto* spellData = npc->GetSpellList()) {
                for (std::uint32_t i = 0; i < spellData->numSpells; ++i) {
                    if (spellData->spells[i] == spell)
                        return true;
                }
            }
        }
        // Spells added at runtime (tomes, AddSpell, powers).
        for (auto* s : player->GetActorRuntimeData().addedSpells) {
            if (s == spell)
                return true;
        }
        return false;
    }

    // ─── Effect helpers ────────────────────────────────────────────────────────

    // Magnitude/duration exactly as the vanilla UI displays them:
    //  - Effect::GetMagnitude()/GetDuration() (native; return 0 when the
    //    effect has the kNoMagnitude/kNoDuration flag)
    //  - perk entry points kModSpellMagnitude / kModSpellDuration applied for
    //    effects flagged kPowerAffectsMagnitude / kPowerAffectsDuration —
    //    the same engine mechanism the UI uses to show perk-modified values
    //  - ×100 when the effect's associated actor value is flagged
    //    kDisplayedEffectMagnitudeTimesOneHundred — percent-style AVs
    //    (Resist Frost, Resist Fire, ...) store magnitude as 0.1 while the
    //    UI shows "10%".
    struct DisplayValues {
        float         magnitude;
        std::uint32_t duration;
    };

    static DisplayValues GetDisplayValues(const RE::Effect* eff, const RE::MagicItem* magic, RE::Actor* caster)
    {
        if (!eff || !eff->baseEffect)
            return { 0.f, 0u };

        const auto& data = eff->baseEffect->data;

        float         mag = eff->GetMagnitude();
        std::uint32_t dur = eff->GetDuration();

        if (caster && magic) {
            if (data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kPowerAffectsMagnitude)) {
                float modified = mag;
                RE::BGSEntryPoint::HandleEntryPoint(
                    RE::BGSEntryPoint::ENTRY_POINTS::ENTRY_POINT::kModSpellMagnitude,
                    caster, const_cast<RE::MagicItem*>(magic), const_cast<RE::Effect*>(eff), &modified);
                mag = modified;
            }
            if (data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kPowerAffectsDuration)) {
                float modified = static_cast<float>(dur);
                RE::BGSEntryPoint::HandleEntryPoint(
                    RE::BGSEntryPoint::ENTRY_POINTS::ENTRY_POINT::kModSpellDuration,
                    caster, const_cast<RE::MagicItem*>(magic), const_cast<RE::Effect*>(eff), &modified);
                dur = static_cast<std::uint32_t>(std::lround(modified));
            }
        }

        for (auto av : { data.resistVariable, data.primaryAV, data.secondaryAV }) {
            auto* avi = RE::ActorValueList::GetActorValueInfo(av);
            if (avi && avi->flags.all(RE::ActorValueInfo::ActorValueFlag::kDisplayedEffectMagnitudeTimesOneHundred)) {
                mag *= 100.f;
                break;
            }
        }
        return { mag, dur };
    }

    // Build a JSON object for a single magic effect using native game data only.
    //  - name:                EffectSetting::GetName()
    //  - magnitude/duration:  Effect::effectItem, rounded the same way the
    //                         vanilla UI displays them (integers)
    //  - descriptionTemplate: EffectSetting::magicItemDescription (DNAM),
    //                         with unresolved <mag>/<dur> tags
    //  - description:         template with <mag>/<dur> substituted
    nlohmann::json BuildEffectJson(const RE::Effect* eff, const RE::MagicItem* magic, RE::Actor* caster)
    {
        nlohmann::json j;
        if (!eff || !eff->baseEffect) {
            j["name"]                = "";
            j["magnitude"]           = 0;
            j["duration"]            = 0u;
            j["descriptionTemplate"] = "";
            j["description"]         = "";
            return j;
        }

        const auto* base = eff->baseEffect;

        j["name"] = base->GetName();

        // The vanilla UI displays magnitude/duration as integers — round the
        // display magnitude the same way instead of exposing engine-internal
        // precision.
        const auto  values     = GetDisplayValues(eff, magic, caster);
        const std::int32_t displayMag = static_cast<std::int32_t>(std::lround(values.magnitude));
        j["magnitude"] = displayMag;
        j["duration"]  = values.duration;

        // Localized description template from the EffectSetting's DNAM field
        // (native data source, same text the in-game UI reads).
        j["descriptionTemplate"] = std::string(base->magicItemDescription.c_str());

        // Resolve the description by substituting <mag> and <dur> placeholders
        std::string resolved = j["descriptionTemplate"].get<std::string>();
        std::size_t pos = 0;
        std::string magStr = std::to_string(displayMag);
        std::string durStr = std::to_string(values.duration);
        while ((pos = resolved.find("<mag>", pos)) != std::string::npos) {
            resolved.replace(pos, 5, magStr);
            pos += magStr.length();
        }
        pos = 0;
        while ((pos = resolved.find("<dur>", pos)) != std::string::npos) {
            resolved.replace(pos, 5, durStr);
            pos += durStr.length();
        }
        j["description"] = resolved;

        return j;
    }

    // Build a JSON array of effects for a MagicItem (spell, enchantment, potion, scroll, etc.).
    nlohmann::json BuildEffectsArray(const RE::MagicItem* magic, RE::Actor* caster)
    {
        nlohmann::json effects = nlohmann::json::array();
        if (magic) {
            for (const auto* eff : magic->effects) {
                if (!eff || !eff->baseEffect)
                    continue;
                effects.push_back(BuildEffectJson(eff, magic, caster));
            }
        }
        return effects;
    }

    // Item-level description built by the engine itself — the exact same call
    // the in-game UI uses (MagicSystem::GetMagicItemDescription). Substitutes
    // <mag>/<dur> for every effect and concatenates them. If the engine returns
    // text with unresolved tags, fall back to joining the per-effect resolved
    // descriptions.
    std::string BuildItemDescription(const RE::MagicItem* magic, RE::Actor* caster)
    {
        if (!magic)
            return "";

        // The engine function takes begin/end tag formats; try the plausible
        // variants and keep the first result with all tags resolved.
        static constexpr std::array<std::pair<const char*, const char*>, 3> kTagFormats{ {
            { "<mag>", "</mag>" },
            { "<%s>", "</%s>" },
            { "<mag", ">" },
        } };

        for (const auto& [beginTag, endTag] : kTagFormats) {
            RE::BSString out;
            RE::MagicSystem::GetMagicItemDescription(out, const_cast<RE::MagicItem*>(magic), beginTag, endTag);
            std::string desc(out.c_str());
            if (!desc.empty() &&
                desc.find("<mag") == std::string::npos &&
                desc.find("<dur") == std::string::npos &&
                desc.find("%s") == std::string::npos) {
                return desc;
            }
        }

        // Fallback: join per-effect descriptions (already resolved).
        std::string joined;
        for (const auto* eff : magic->effects) {
            if (!eff || !eff->baseEffect)
                continue;
            nlohmann::json j = BuildEffectJson(eff, magic, caster);
            if (!joined.empty())
                joined += "\n";
            joined += j["description"].get<std::string>();
        }
        return joined;
    }
}
