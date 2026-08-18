#include "Common.h"

#include "../../logger.h"

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
}
