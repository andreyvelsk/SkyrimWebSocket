#include "InventoryRegistry.h"
#include <algorithm>

namespace InventoryRegistry
{
    ItemType DetermineItemType(RE::TESForm* item)
    {
        if (!item)
            return ItemType::kUnknown;

        auto formType = item->GetFormType();

        switch (formType) {
            case RE::FormType::Weapon:
                return ItemType::kWeapon;
            case RE::FormType::Armor:
                return ItemType::kArmor;
            case RE::FormType::Ammo:
                return ItemType::kAmmo;
            case RE::FormType::Potion:
                return ItemType::kPotion;
            case RE::FormType::Book:
                return ItemType::kBook;
            case RE::FormType::Ingredient:
                return ItemType::kIngredient;
            case RE::FormType::Misc:
                return ItemType::kMisc;
            case RE::FormType::Spell:
                return ItemType::kSpell;
            default:
                return ItemType::kUnknown;
        }
    }

    WeaponType DetermineWeaponType(RE::TESObjectWEAP* weapon)
    {
        if (!weapon)
            return WeaponType::kUnknown;

        using Type = RE::WEAPON_TYPE;
        auto type = weapon->GetWeaponType();

        switch (type) {
            case Type::kBow:
                return WeaponType::kBow;
            case Type::kCrossbow:
                return WeaponType::kCrossbow;
            case Type::kOneHandSword:
            case Type::kOneHandDagger:
            case Type::kOneHandAxe:
            case Type::kOneHandMace:
                return WeaponType::kOneHanded;
            case Type::kTwoHandSword:
            case Type::kTwoHandAxe:
            case Type::kStaff:
                return WeaponType::kTwoHanded;
            default:
                return WeaponType::kUnknown;
        }
    }

    ArmorType DetermineArmorType(RE::TESObjectARMO* armor)
    {
        if (!armor)
            return ArmorType::kUnknown;

        using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
        auto partMask = armor->GetSlotMask();

        // Проверяем в порядке приоритета
        if (partMask & Slot::kHead)
            return ArmorType::kHead;
        if (partMask & Slot::kBody)
            return ArmorType::kChest;
        if (partMask & Slot::kHands)
            return ArmorType::kGloves;
        if (partMask & Slot::kFeet)
            return ArmorType::kBoots;
        if (partMask & Slot::kShield)
            return ArmorType::kShield;

        return ArmorType::kUnknown;
    }

    std::string GetArmorTypeString(ArmorType type)
    {
        switch (type) {
            case ArmorType::kHead:
                return "head";
            case ArmorType::kChest:
                return "chest";
            case ArmorType::kGloves:
                return "gloves";
            case ArmorType::kBoots:
                return "boots";
            case ArmorType::kShield:
                return "shield";
            default:
                return "unknown";
        }
    }

    std::string GetItemTypeString(ItemType type)
    {
        switch (type) {
            case ItemType::kWeapon:
                return "weapon";
            case ItemType::kArmor:
                return "armor";
            case ItemType::kAmmo:
                return "ammo";
            case ItemType::kPotion:
                return "potion";
            case ItemType::kFood:
                return "food";
            case ItemType::kBook:
                return "book";
            case ItemType::kIngredient:
                return "ingredient";
            case ItemType::kMisc:
                return "misc";
            case ItemType::kSpell:
                return "spell";
            default:
                return "unknown";
        }
    }

    std::string GetWeaponTypeString(WeaponType type)
    {
        switch (type) {
            case WeaponType::kOneHanded:
                return "oneHanded";
            case WeaponType::kTwoHanded:
                return "twoHanded";
            case WeaponType::kBow:
                return "bow";
            case WeaponType::kCrossbow:
                return "crossbow";
            default:
                return "unknown";
        }
    }

    float GetItemDurability(RE::ExtraDataList* extraList)
    {
        if (!extraList)
            return -1.f;

        auto healthExtra = extraList->GetByType<RE::ExtraHealth>();
        if (healthExtra && healthExtra->health > 0.f)
            return healthExtra->health;

        return -1.f;
    }

    float GetEnchantmentCharge(RE::ExtraDataList* extraList)
    {
        if (!extraList)
            return -1.f;

        auto chargeExtra = extraList->GetByType<RE::ExtraCharge>();
        if (chargeExtra && chargeExtra->charge >= 0.f)
            return chargeExtra->charge * 100.f;  // Нормализуем к 0-100

        return -1.f;
    }

    std::optional<ItemInfo> GetItemInfo(RE::TESForm* item, int count, RE::ExtraDataList* extraList)
    {
        if (!item)
            return std::nullopt;

        ItemInfo info;
        info.name       = item->GetName();
        info.itemType   = DetermineItemType(item);
        info.weight     = item->GetWeight();
        info.formId     = item->GetFormID();
        info.count      = count;
        info.value      = item->GetGoldValue();
        info.isQuestItem = false;
        info.isEquipped = false;
        info.durability = GetItemDurability(extraList);
        info.keywords   = "";

        // Пытаемся получить дополнительную информацию в зависимости от типа
        if (auto book = item->As<RE::TESObjectBOOK>()) {
            info.isQuestItem = book->IsQuestItem();
        } else if (auto wep = item->As<RE::TESObjectWEAP>()) {
            info.isQuestItem = wep->IsQuestItem();
        } else if (auto arm = item->As<RE::TESObjectARMO>()) {
            info.isQuestItem = arm->IsQuestItem();
        }

        return info;
    }

    nlohmann::json WeaponInfoToJson(RE::TESObjectWEAP* weapon, int count, RE::ExtraDataList* extraList)
    {
        auto baseInfo = GetItemInfo(weapon, count, extraList);
        if (!baseInfo)
            return nullptr;

        nlohmann::json j;
        j["name"]          = baseInfo->name;
        j["itemType"]      = "weapon";
        j["weight"]        = baseInfo->weight;
        j["formId"]        = baseInfo->formId;
        j["count"]         = baseInfo->count;
        j["value"]         = baseInfo->value;
        j["isQuestItem"]   = baseInfo->isQuestItem;
        j["durability"]    = baseInfo->durability;

        WeaponType wepType = DetermineWeaponType(weapon);
        j["weaponType"]    = GetWeaponTypeString(wepType);
        j["damage"]        = weapon->GetAttackDamage();
        j["attackSpeed"]   = weapon->speed;
        j["reach"]         = weapon->reach;

        // Информация об чаре
        if (auto ench = weapon->enchantment) {
            nlohmann::json enchJson;
            enchJson["name"]  = ench->GetName();
            enchJson["cost"]  = ench->GetCastingCost(nullptr);
            enchJson["charge"] = GetEnchantmentCharge(extraList);
            j["enchantment"]  = enchJson;
        } else {
            j["enchantment"] = nullptr;
        }

        return j;
    }

    nlohmann::json ArmorInfoToJson(RE::TESObjectARMO* armor, int count, RE::ExtraDataList* extraList)
    {
        auto baseInfo = GetItemInfo(armor, count, extraList);
        if (!baseInfo)
            return nullptr;

        nlohmann::json j;
        j["name"]          = baseInfo->name;
        j["itemType"]      = "armor";
        j["weight"]        = baseInfo->weight;
        j["formId"]        = baseInfo->formId;
        j["count"]         = baseInfo->count;
        j["value"]         = baseInfo->value;
        j["isQuestItem"]   = baseInfo->isQuestItem;
        j["durability"]    = baseInfo->durability;

        ArmorType armorType = DetermineArmorType(armor);
        j["armorType"]      = GetArmorTypeString(armorType);
        j["armor"]          = armor->GetArmorRating();

        // Информация об чаре
        if (auto ench = armor->enchantment) {
            nlohmann::json enchJson;
            enchJson["name"]   = ench->GetName();
            enchJson["cost"]   = ench->GetCastingCost(nullptr);
            enchJson["charge"] = GetEnchantmentCharge(extraList);
            j["enchantment"]   = enchJson;
        } else {
            j["enchantment"] = nullptr;
        }

        return j;
    }

    nlohmann::json PotionInfoToJson(RE::AlchemyItem* potion, int count, RE::ExtraDataList* extraList)
    {
        auto baseInfo = GetItemInfo(potion, count, extraList);
        if (!baseInfo)
            return nullptr;

        nlohmann::json j;
        j["name"]        = baseInfo->name;
        j["itemType"]    = "potion";
        j["weight"]      = baseInfo->weight;
        j["formId"]      = baseInfo->formId;
        j["count"]       = baseInfo->count;
        j["value"]       = baseInfo->value;
        j["isQuestItem"] = baseInfo->isQuestItem;

        j["isPoison"]    = potion->GetRecipeFilter(nullptr) == RE::AlchemyItem::RecipeFilter::kPoison;

        // Эффекты зелья
        nlohmann::json effects = nlohmann::json::array();
        for (size_t i = 0; i < potion->effects.size(); ++i) {
            auto& effect = potion->effects[i];
            if (effect) {
                nlohmann::json effJson;
                auto* mgef = effect->baseEffect;
                if (mgef) {
                    effJson["name"]     = mgef->GetName();
                    effJson["magnitude"] = effect->magnitude;
                    effJson["duration"] = effect->duration;
                    effJson["area"]     = effect->area;
                    effects.push_back(effJson);
                }
            }
        }
        j["effects"] = effects;

        return j;
    }

    nlohmann::json BookInfoToJson(RE::TESObjectBOOK* book, int count)
    {
        auto baseInfo = GetItemInfo(book, count);
        if (!baseInfo)
            return nullptr;

        nlohmann::json j;
        j["name"]        = baseInfo->name;
        j["itemType"]    = "book";
        j["weight"]      = baseInfo->weight;
        j["formId"]      = baseInfo->formId;
        j["count"]       = baseInfo->count;
        j["value"]       = baseInfo->value;
        j["isQuestItem"] = baseInfo->isQuestItem;

        j["isSkillBook"] = book->TeachesSkill();
        if (auto skillTaught = book->GetSkill()) {
            j["skill"] = static_cast<uint32_t>(skillTaught);
        }

        if (auto quest = book->GetQuest()) {
            j["relatedQuest"] = quest->GetName();
        }

        return j;
    }

    nlohmann::json AmmoInfoToJson(RE::TESAmmo* ammo, int count)
    {
        auto baseInfo = GetItemInfo(ammo, count);
        if (!baseInfo)
            return nullptr;

        nlohmann::json j;
        j["name"]        = baseInfo->name;
        j["itemType"]    = "ammo";
        j["weight"]      = baseInfo->weight;
        j["formId"]      = baseInfo->formId;
        j["count"]       = baseInfo->count;
        j["value"]       = baseInfo->value;
        j["isQuestItem"] = baseInfo->isQuestItem;
        j["damage"]      = ammo->GetDamage();

        return j;
    }

    nlohmann::json IngredientInfoToJson(RE::IngredientItem* ingredient, int count)
    {
        auto baseInfo = GetItemInfo(ingredient, count);
        if (!baseInfo)
            return nullptr;

        nlohmann::json j;
        j["name"]        = baseInfo->name;
        j["itemType"]    = "ingredient";
        j["weight"]      = baseInfo->weight;
        j["formId"]      = baseInfo->formId;
        j["count"]       = baseInfo->count;
        j["value"]       = baseInfo->value;
        j["isQuestItem"] = baseInfo->isQuestItem;

        // Эффекты ингредиента
        nlohmann::json effects = nlohmann::json::array();
        for (size_t i = 0; i < ingredient->effects.size(); ++i) {
            auto& effect = ingredient->effects[i];
            if (effect) {
                nlohmann::json effJson;
                auto* mgef = effect->baseEffect;
                if (mgef) {
                    effJson["name"] = mgef->GetName();
                    effects.push_back(effJson);
                }
            }
        }
        j["effects"] = effects;

        return j;
    }

    nlohmann::json MiscInfoToJson(RE::TESObjectMISC* misc, int count)
    {
        auto baseInfo = GetItemInfo(misc, count);
        if (!baseInfo)
            return nullptr;

        nlohmann::json j;
        j["name"]        = baseInfo->name;
        j["itemType"]    = "misc";
        j["weight"]      = baseInfo->weight;
        j["formId"]      = baseInfo->formId;
        j["count"]       = baseInfo->count;
        j["value"]       = baseInfo->value;
        j["isQuestItem"] = baseInfo->isQuestItem;

        return j;
    }

    nlohmann::json SpellInfoToJson(RE::SpellItem* spell)
    {
        nlohmann::json j;
        j["name"]       = spell->GetName();
        j["itemType"]   = "spell";
        j["formId"]     = spell->GetFormID();
        j["cost"]       = spell->GetMagickaCost(nullptr);
        j["castType"]   = static_cast<uint32_t>(spell->GetSpellType());

        // Эффекты заклинания
        nlohmann::json effects = nlohmann::json::array();
        for (size_t i = 0; i < spell->effects.size(); ++i) {
            auto& effect = spell->effects[i];
            if (effect) {
                nlohmann::json effJson;
                auto* mgef = effect->baseEffect;
                if (mgef) {
                    effJson["name"]     = mgef->GetName();
                    effJson["magnitude"] = effect->magnitude;
                    effJson["duration"] = effect->duration;
                    effJson["area"]     = effect->area;
                    effects.push_back(effJson);
                }
            }
        }
        j["effects"] = effects;

        return j;
    }

    std::optional<nlohmann::json> GetItemJson(RE::TESForm* item, RE::ExtraDataList* extraList)
    {
        if (!item)
            return std::nullopt;

        auto itemType = DetermineItemType(item);

        switch (itemType) {
            case ItemType::kWeapon:
                if (auto wep = item->As<RE::TESObjectWEAP>())
                    return WeaponInfoToJson(wep, 1, extraList);
                break;
            case ItemType::kArmor:
                if (auto arm = item->As<RE::TESObjectARMO>())
                    return ArmorInfoToJson(arm, 1, extraList);
                break;
            case ItemType::kAmmo:
                if (auto ammo = item->As<RE::TESAmmo>())
                    return AmmoInfoToJson(ammo, 1);
                break;
            case ItemType::kPotion:
                if (auto pot = item->As<RE::AlchemyItem>())
                    return PotionInfoToJson(pot, 1, extraList);
                break;
            case ItemType::kFood:
                if (auto food = item->As<RE::TESObjectFOOD>()) {
                    auto baseInfo = GetItemInfo(food, 1);
                    if (!baseInfo)
                        return std::nullopt;
                    nlohmann::json j;
                    j["name"]        = baseInfo->name;
                    j["itemType"]    = "food";
                    j["weight"]      = baseInfo->weight;
                    j["formId"]      = baseInfo->formId;
                    j["count"]       = baseInfo->count;
                    j["value"]       = baseInfo->value;
                    j["isQuestItem"] = baseInfo->isQuestItem;
                    return j;
                }
                break;
            case ItemType::kBook:
                if (auto book = item->As<RE::TESObjectBOOK>())
                    return BookInfoToJson(book, 1);
                break;
            case ItemType::kIngredient:
                if (auto ing = item->As<RE::IngredientItem>())
                    return IngredientInfoToJson(ing, 1);
                break;
            case ItemType::kMisc:
                if (auto misc = item->As<RE::TESObjectMISC>())
                    return MiscInfoToJson(misc, 1);
                break;
            case ItemType::kSpell:
                if (auto spell = item->As<RE::SpellItem>())
                    return SpellInfoToJson(spell);
                break;
            default:
                break;
        }

        return std::nullopt;
    }

    nlohmann::json GetInventoryByTypeJson(ItemType filter, RE::Actor* actor)
    {
        if (!actor)
            actor = RE::PlayerCharacter::GetSingleton();

        if (!actor)
            return nullptr;

        nlohmann::json result = nlohmann::json::array();
        auto*          inv    = actor->GetInventory();

        if (!inv)
            return result;

        for (auto& [item, data] : *inv) {
            if (!item)
                continue;

            auto itemType = DetermineItemType(item);
            if (filter != ItemType::kUnknown && itemType != filter)
                continue;

            auto count       = data.first;
            auto& extraDataList = data.second;

            auto itemJson = GetItemJson(item, extraDataList.empty() ? nullptr : &extraDataList[0]);
            if (itemJson) {
                result.push_back(itemJson.value());
            }
        }

        return result;
    }

    nlohmann::json GetInventoryJson(RE::Actor* actor)
    {
        if (!actor)
            actor = RE::PlayerCharacter::GetSingleton();

        if (!actor)
            return nullptr;

        nlohmann::json result;
        result["type"]  = "inventory";
        result["actor"] = actor->GetDisplayFullName();

        // Группируем по типам предметов
        result["weapons"]   = GetInventoryByTypeJson(ItemType::kWeapon, actor);
        result["armor"]     = GetInventoryByTypeJson(ItemType::kArmor, actor);
        result["ammo"]      = GetInventoryByTypeJson(ItemType::kAmmo, actor);
        result["potions"]   = GetInventoryByTypeJson(ItemType::kPotion, actor);
        result["food"]      = GetInventoryByTypeJson(ItemType::kFood, actor);
        result["books"]     = GetInventoryByTypeJson(ItemType::kBook, actor);
        result["ingredients"] = GetInventoryByTypeJson(ItemType::kIngredient, actor);
        result["misc"]      = GetInventoryByTypeJson(ItemType::kMisc, actor);

        // Получаем известные заклинания отдельно
        nlohmann::json spells = nlohmann::json::array();
        auto npc = actor->GetActorBase();
        if (npc) {
            auto* spellList = static_cast<RE::TESSpellList*>(npc);
            if (spellList && spellList->actorEffects) {
                auto* spellsData = spellList->actorEffects->spells;
                auto count = spellList->actorEffects->numSpells;
                
                for (std::uint32_t i = 0; i < count; ++i) {
                    if (auto spell = spellsData[i]) {
                        auto spellJson = SpellInfoToJson(spell);
                        spells.push_back(spellJson);
                    }
                }
            }
        }
        result["spells"] = spells;

        return result;
    }
}
