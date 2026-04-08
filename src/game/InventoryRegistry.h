#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace InventoryRegistry
{
    enum class ItemType
    {
        kWeapon,     // Сражающееся оружие (мечи, топоры, луки и т.д.)
        kArmor,      // Броня (шлемы, нагрудники, перчатки, сапоги, щиты)
        kAmmo,       // Боеприпасы (стрелы, болты)
        kPotion,     // Зелья и яды
        kFood,       // Еда и напитки
        kBook,       // Книги и свитки
        kIngredient, // Ингредиенты
        kMisc,       // Разное (ключи, драгоценности и т.д.)
        kSpell,      // Способности и заклинания
        kUnknown
    };

    enum class WeaponType
    {
        kOneHanded,
        kTwoHanded,
        kBow,
        kCrossbow,
        kUnknown
    };

    enum class ArmorType
    {
        kHead,       // Шлем
        kChest,      // Нагрудник
        kGloves,     // Перчатки
        kBoots,      // Сапоги
        kShield,     // Щит
        kUnknown
    };

    // Базовая информация о предмете
    struct ItemInfo
    {
        std::string name;           // Название предмета
        ItemType    itemType;       // Тип предмета
        float       weight;         // Вес
        uint32_t    formId;         // FormID
        int32_t     count;          // Количество в инвентаре
        float       value;          // Стоимость
        bool        isQuestItem;    // Является ли квестовым предметом
        bool        isEquipped;     // Надет ли предмет
        float       durability;     // Прочность (0.0-100.0), -1 если отсутствует
        std::string keywords;       // Список ключевых слов через запятую
    };

    // Информация об оружии
    struct WeaponInfo : ItemInfo
    {
        WeaponType weaponType;
        float      damage;          // Урон
        float      attackSpeed;     // Скорость атаки
        float      reach;           // Дальность
        float      enchantmentCharge; // Заряд чара (0.0-100.0), -1 если отсутствует
        float      enchantmentValue;  // Стоимость заклинания, -1 если отсутствует
    };

    // Информация о броне
    struct ArmorInfo : ItemInfo
    {
        ArmorType  armorType;
        float      armor;           // Класс защиты
        float      enchantmentCharge; // Заряд чара (0.0-100.0), -1 если отсутствует
        float      enchantmentValue;  // Стоимость заклинания, -1 если отсутствует
    };

    // Информация о зелье/яде
    struct PotionInfo : ItemInfo
    {
        bool       isPoison;        // true = яд, false = зелье
        std::string effects;        // Список эффектов через запятую
    };

    // Информация о книге
    struct BookInfo : ItemInfo
    {
        bool       isSkillBook;     // Это ли книга навыка
        std::string skillOrQuest;   // Какой навык или квест (если применимо)
        uint32_t   pageCount;       // Количество страниц
    };

    // Получить полный инвентарь
    nlohmann::json GetInventoryJson(RE::Actor* actor = nullptr);

    // Получить инвентарь определённого типа
    nlohmann::json GetInventoryByTypeJson(ItemType type, RE::Actor* actor = nullptr);

    // Получить информацию об одном предмете
    std::optional<nlohmann::json> GetItemJson(RE::TESForm* item, RE::ExtraDataList* extraList = nullptr);

    // Получить общую информацию о предмете для любого типа
    std::optional<ItemInfo> GetItemInfo(RE::TESForm* item, int count = 1, RE::ExtraDataList* extraList = nullptr);

    // Определить тип предмета
    ItemType DetermineItemType(RE::TESForm* item);
}
