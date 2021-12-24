#include "../botpch.h"
#include "playerbot.h"
#include "PlayerbotAIConfig.h"
#include "RandomItemMgr.h"

#include "DBCStore.h"
#include "../../modules/Bots/ahbot/AhBot.h"
#include "DatabaseEnv.h"
#include "PlayerbotAI.h"

#include "../../modules/Bots/ahbot/AhBotConfig.h"
#include "ServerFacade.h"
#include "strategy/values/LootValues.h"

char * strstri (const char* str1, const char* str2);

uint64 BotEquipKey::GetKey()
{
    return level + 100 * clazz + 10000 * slot + 1000000 * quality;
}

class RandomItemGuildTaskPredicate : public RandomItemPredicate
{
public:
    virtual bool Apply(ItemPrototype const* proto)
    {
        if (proto->Bonding == BIND_WHEN_PICKED_UP ||
                proto->Bonding == BIND_QUEST_ITEM ||
                proto->Bonding == BIND_WHEN_USE)
            return false;

        if (proto->Quality < ITEM_QUALITY_NORMAL)
            return false;

        if ((proto->Class == ITEM_CLASS_ARMOR || proto->Class == ITEM_CLASS_WEAPON) && proto->Quality >= ITEM_QUALITY_RARE)
            return true;

        if (proto->Class == ITEM_CLASS_TRADE_GOODS || proto->Class == ITEM_CLASS_CONSUMABLE)
            return true;

        return false;
    }
};

class RandomItemGuildTaskRewardPredicate : public RandomItemPredicate
{
public:
    RandomItemGuildTaskRewardPredicate(bool equip, bool rare) { this->equip = equip; this->rare = rare;}

    virtual bool Apply(ItemPrototype const* proto)
    {
        if (proto->Bonding == BIND_WHEN_PICKED_UP ||
                proto->Bonding == BIND_QUEST_ITEM ||
                proto->Bonding == BIND_WHEN_USE)
            return false;

        if (proto->Class == ITEM_CLASS_QUEST)
            return false;

        if (equip)
        {
            uint32 desiredQuality = rare ? ITEM_QUALITY_RARE : ITEM_QUALITY_UNCOMMON;
            if (proto->Quality < desiredQuality || proto->Quality >= ITEM_QUALITY_EPIC)
                return false;

            if (proto->Class == ITEM_CLASS_ARMOR || proto->Class == ITEM_CLASS_WEAPON)
                return true;
        }
        else
        {
            uint32 desiredQuality = rare ? ITEM_QUALITY_UNCOMMON : ITEM_QUALITY_NORMAL;
            if (proto->Quality < desiredQuality || proto->Quality >= ITEM_QUALITY_RARE)
                return false;

            if (proto->Class == ITEM_CLASS_TRADE_GOODS || proto->Class == ITEM_CLASS_CONSUMABLE)
                return true;
        }

        return false;
    }

private:
    bool equip;
    bool rare;
};

RandomItemMgr::RandomItemMgr()
{
    predicates[RANDOM_ITEM_GUILD_TASK] = new RandomItemGuildTaskPredicate();
    predicates[RANDOM_ITEM_GUILD_TASK_REWARD_EQUIP_GREEN] = new RandomItemGuildTaskRewardPredicate(true, false);
    predicates[RANDOM_ITEM_GUILD_TASK_REWARD_EQUIP_BLUE] = new RandomItemGuildTaskRewardPredicate(true, true);
    predicates[RANDOM_ITEM_GUILD_TASK_REWARD_TRADE] = new RandomItemGuildTaskRewardPredicate(false, false);
    predicates[RANDOM_ITEM_GUILD_TASK_REWARD_TRADE_RARE] = new RandomItemGuildTaskRewardPredicate(false, true);

    viableSlots[EQUIPMENT_SLOT_HEAD].insert(INVTYPE_HEAD);
    viableSlots[EQUIPMENT_SLOT_NECK].insert(INVTYPE_NECK);
    viableSlots[EQUIPMENT_SLOT_SHOULDERS].insert(INVTYPE_SHOULDERS);
    viableSlots[EQUIPMENT_SLOT_BODY].insert(INVTYPE_BODY);
    viableSlots[EQUIPMENT_SLOT_CHEST].insert(INVTYPE_CHEST);
    viableSlots[EQUIPMENT_SLOT_CHEST].insert(INVTYPE_ROBE);
    viableSlots[EQUIPMENT_SLOT_WAIST].insert(INVTYPE_WAIST);
    viableSlots[EQUIPMENT_SLOT_LEGS].insert(INVTYPE_LEGS);
    viableSlots[EQUIPMENT_SLOT_FEET].insert(INVTYPE_FEET);
    viableSlots[EQUIPMENT_SLOT_WRISTS].insert(INVTYPE_WRISTS);
    viableSlots[EQUIPMENT_SLOT_HANDS].insert(INVTYPE_HANDS);
    viableSlots[EQUIPMENT_SLOT_FINGER1].insert(INVTYPE_FINGER);
    viableSlots[EQUIPMENT_SLOT_FINGER2].insert(INVTYPE_FINGER);
    viableSlots[EQUIPMENT_SLOT_TRINKET1].insert(INVTYPE_TRINKET);
    viableSlots[EQUIPMENT_SLOT_TRINKET2].insert(INVTYPE_TRINKET);
    viableSlots[EQUIPMENT_SLOT_MAINHAND].insert(INVTYPE_WEAPON);
    viableSlots[EQUIPMENT_SLOT_MAINHAND].insert(INVTYPE_2HWEAPON);
    viableSlots[EQUIPMENT_SLOT_MAINHAND].insert(INVTYPE_WEAPONMAINHAND);
    viableSlots[EQUIPMENT_SLOT_OFFHAND].insert(INVTYPE_WEAPON);
    viableSlots[EQUIPMENT_SLOT_OFFHAND].insert(INVTYPE_2HWEAPON);
    viableSlots[EQUIPMENT_SLOT_OFFHAND].insert(INVTYPE_SHIELD);
    viableSlots[EQUIPMENT_SLOT_OFFHAND].insert(INVTYPE_WEAPONMAINHAND);
    viableSlots[EQUIPMENT_SLOT_OFFHAND].insert(INVTYPE_HOLDABLE);
    viableSlots[EQUIPMENT_SLOT_RANGED].insert(INVTYPE_RANGED);
    viableSlots[EQUIPMENT_SLOT_RANGED].insert(INVTYPE_THROWN);
    viableSlots[EQUIPMENT_SLOT_RANGED].insert(INVTYPE_RANGEDRIGHT);
    viableSlots[EQUIPMENT_SLOT_RANGED].insert(INVTYPE_RELIC);
    viableSlots[EQUIPMENT_SLOT_TABARD].insert(INVTYPE_TABARD);
    viableSlots[EQUIPMENT_SLOT_BACK].insert(INVTYPE_CLOAK);

    weightStatLink["sta"] = ITEM_MOD_STAMINA;
    weightStatLink["str"] = ITEM_MOD_STRENGTH;
    weightStatLink["agi"] = ITEM_MOD_AGILITY;
    weightStatLink["int"] = ITEM_MOD_INTELLECT;
    weightStatLink["spi"] = ITEM_MOD_SPIRIT;

#ifdef MANGOSBOT_TWO
    weightStatLink["splpwr"] = ITEM_MOD_SPELL_POWER;
    weightStatLink["atkpwr"] = ITEM_MOD_ATTACK_POWER;
    weightStatLink["feratkpwr"] = ITEM_MOD_FERAL_ATTACK_POWER;

    weightStatLink["exprtng"] = ITEM_MOD_EXPERTISE_RATING;
    weightStatLink["critstrkrtng"] = ITEM_MOD_CRIT_RATING;
    weightStatLink["hitrtng"] = ITEM_MOD_HIT_RATING;
    weightStatLink["hastertng"] = ITEM_MOD_HASTE_RATING;
    weightStatLink["armorpenrtng"] = ITEM_MOD_ARMOR_PENETRATION_RATING;
    
    weightStatLink["defrtng"] = ITEM_MOD_DEFENSE_SKILL_RATING;
    weightStatLink["dodgertng"] = ITEM_MOD_DODGE_RATING;
    weightStatLink["block"] = ITEM_MOD_BLOCK_VALUE;
    weightStatLink["blockrtng"] = ITEM_MOD_BLOCK_RATING;
    weightStatLink["parryrtng"] = ITEM_MOD_PARRY_RATING;
    
    weightStatLink["manargn"] = ITEM_MOD_MANA_REGENERATION;

    weightRatingLink["exprtng"] = CR_EXPERTISE;
    weightRatingLink["critstrkrtng"] = CR_CRIT_MELEE;
    weightRatingLink["hitrtng"] = CR_HIT_MELEE;
    weightRatingLink["hastertng"] = CR_HASTE_MELEE;
    weightRatingLink["armorpenrtng"] = CR_ARMOR_PENETRATION;

    weightRatingLink["defrtng"] = CR_DEFENSE_SKILL;
    weightRatingLink["dodgertng"] = CR_DODGE;
    weightRatingLink["blockrtng"] = CR_BLOCK;
    weightRatingLink["parryrtng"] = CR_PARRY;
#endif

#ifdef MANGOSBOT_ONE
    //weightStatLink["splpwr"] = ITEM_MOD_SPELL_POWER;
    //weightStatLink["atkpwr"] = ITEM_MOD_ATTACK_POWER;
    //weightStatLink["feratkpwr"] = ITEM_MOD_FERAL_ATTACK_POWER;

    weightStatLink["exprtng"] = ITEM_MOD_EXPERTISE_RATING;
    weightStatLink["critstrkrtng"] = ITEM_MOD_CRIT_RATING;
    weightStatLink["spellcritstrkrtng"] = ITEM_MOD_CRIT_SPELL_RATING;
    weightStatLink["hitrtng"] = ITEM_MOD_HIT_RATING;
    weightStatLink["spellhitrtng"] = ITEM_MOD_HIT_SPELL_RATING;
    weightStatLink["hastertng"] = ITEM_MOD_HASTE_RATING;
    weightStatLink["spellhastertng"] = ITEM_MOD_HASTE_SPELL_RATING;
    //weightStatLink["armorpenrtng"] = ITEM_MOD_ARMOR_PENETRATION_RATING;

    weightStatLink["defrtng"] = ITEM_MOD_DEFENSE_SKILL_RATING;
    weightStatLink["dodgertng"] = ITEM_MOD_DODGE_RATING;
    //weightStatLink["block"] = ITEM_MOD_BLOCK_VALUE;
    weightStatLink["blockrtng"] = ITEM_MOD_BLOCK_RATING;
    weightStatLink["parryrtng"] = ITEM_MOD_PARRY_RATING;

    //weightStatLink["manargn"] = ITEM_MOD_MANA_REGENERATION;

    weightRatingLink["exprtng"] = CR_EXPERTISE;
    weightRatingLink["critstrkrtng"] = CR_CRIT_MELEE;
    weightRatingLink["hitrtng"] = CR_HIT_MELEE;
    weightRatingLink["hastertng"] = CR_HASTE_MELEE;
    //weightRatingLink["armorpenrtng"] = CR_ARMOR_PENETRATION;

    weightRatingLink["defrtng"] = CR_DEFENSE_SKILL;
    weightRatingLink["dodgertng"] = CR_DODGE;
    weightRatingLink["blockrtng"] = CR_BLOCK;
    weightRatingLink["parryrtng"] = CR_PARRY;
#endif
}

void RandomItemMgr::Init()
{
    BuildItemInfoCache();
    BuildEquipCache();
    BuildAmmoCache();
    BuildPotionCache();
    BuildFoodCache();
    BuildTradeCache();
}

void RandomItemMgr::InitAfterAhBot()
{
    BuildRandomItemCache();
    //BuildRarityCache();
}

RandomItemMgr::~RandomItemMgr()
{
    for (map<RandomItemType, RandomItemPredicate*>::iterator i = predicates.begin(); i != predicates.end(); ++i)
        delete i->second;

    predicates.clear();
}

bool RandomItemMgr::HandleConsoleCommand(ChatHandler* handler, char const* args)
{
    if (!args || !*args)
    {
        sLog.outError( "Usage: rnditem");
        return false;
    }

    return false;
}

RandomItemList RandomItemMgr::Query(uint32 level, RandomItemType type, RandomItemPredicate* predicate)
{
    RandomItemList &list = randomItemCache[(level - 1) / 10][type];

    RandomItemList result;
    for (RandomItemList::iterator i = list.begin(); i != list.end(); ++i)
    {
        uint32 itemId = *i;
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
            continue;

        if (predicate && !predicate->Apply(proto))
            continue;

        result.push_back(itemId);
    }

    return result;
}

void RandomItemMgr::BuildRandomItemCache()
{
    QueryResult* results = PlayerbotDatabase.PQuery("select lvl, type, item from ai_playerbot_rnditem_cache");
    if (results)
    {
        sLog.outString("Loading random item cache");
        int count = 0;
        do
        {
            Field* fields = results->Fetch();
            uint32 level = fields[0].GetUInt32();
            uint32 type = fields[1].GetUInt32();
            uint32 itemId = fields[2].GetUInt32();

            RandomItemType rit = (RandomItemType)type;
            randomItemCache[level][rit].push_back(itemId);
            count++;

        } while (results->NextRow());
        delete results;
        sLog.outString("Equipment cache loaded from %d records", count);
    }
    else
    {
        sLog.outString("Building random item cache from %u items", sItemStorage.GetMaxEntry());
        for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
        {
            ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
            if (!proto)
                continue;

            if (proto->Duration & 0x80000000)
                continue;

            if (sAhBotConfig.ignoreItemIds.find(proto->ItemId) != sAhBotConfig.ignoreItemIds.end())
                continue;

            if (strstri(proto->Name1, "qa") || strstri(proto->Name1, "test") || strstri(proto->Name1, "deprecated"))
                continue;

            if (!proto->ItemLevel/* || (proto->RequiredLevel && proto->RequiredLevel > sAhBotConfig.maxRequiredLevel) || proto->ItemLevel > sAhBotConfig.maxItemLevel*/)
                continue;

            if (!auctionbot.GetSellPrice(proto))
                continue;

            uint32 level = proto->ItemLevel;
            for (uint32 type = RANDOM_ITEM_GUILD_TASK; type <= RANDOM_ITEM_GUILD_TASK_REWARD_TRADE_RARE; type++)
            {
                RandomItemType rit = (RandomItemType)type;
                if (predicates[rit] && !predicates[rit]->Apply(proto))
                    continue;

                randomItemCache[level / 10][rit].push_back(itemId);
                PlayerbotDatabase.PExecute("insert into ai_playerbot_rnditem_cache (lvl, type, item) values (%u, %u, %u)",
                        level / 10, type, itemId);
            }
        }

        uint32 maxLevel = sPlayerbotAIConfig.randomBotMaxLevel;
        if (maxLevel > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
        for (int level = 0; level < maxLevel / 10; level++)
        {
            for (uint32 type = RANDOM_ITEM_GUILD_TASK; type <= RANDOM_ITEM_GUILD_TASK_REWARD_TRADE_RARE; type++)
            {
                RandomItemList list = randomItemCache[level][(RandomItemType)type];
                sLog.outDetail("    Level %d..%d Type %d - %zu random items cached",
                        level * 10, level * 10 + 9,
                        type,
                        list.size());
                for (RandomItemList::iterator i = list.begin(); i != list.end(); ++i)
                {
                    uint32 itemId = *i;
                    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
                    if (!proto)
                        continue;

                    sLog.outDetail("        [%d] %s", itemId, proto->Name1);
                }
            }
        }
    }
}

uint32 RandomItemMgr::GetRandomItem(uint32 level, RandomItemType type, RandomItemPredicate* predicate)
{
    RandomItemList const& list = Query(level, type, predicate);
    if (list.empty())
        return 0;

    uint32 index = urand(0, list.size() - 1);
    uint32 itemId = list[index];

    return itemId;
}

bool RandomItemMgr::CanEquipItem(BotEquipKey key, ItemPrototype const* proto)
{
    if (proto->Duration & 0x80000000)
        return false;

    if (proto->Quality != key.quality)
        return false;

    if (proto->Bonding == BIND_QUEST_ITEM || proto->Bonding == BIND_WHEN_USE)
        return false;

    if (proto->Class == ITEM_CLASS_CONTAINER)
        return true;

    set<InventoryType> slots = viableSlots[(EquipmentSlots)key.slot];
    if (slots.find((InventoryType)proto->InventoryType) == slots.end())
        return false;

    uint32 requiredLevel = proto->RequiredLevel;

    if (!requiredLevel)
    {
        requiredLevel = GetMinLevelFromCache(proto->ItemId);
    }

    if (!requiredLevel)
        requiredLevel = key.level;

    uint32 level = key.level;
    uint32 delta = 2;
    if (level < 15)
        delta = urand(7, 15);
    else if (proto->Class == ITEM_CLASS_WEAPON || proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
        delta = urand(2, 3);
    else if (!(level % 10) || (level % 10) == 9)
        delta = 2;
    else if (level < 40)
        delta = urand(5, 10);
    else if (level < 60)
        delta = urand(3, 7);
    else if (level < 70)
        delta = urand(2, 5);
    else if (level < 80)
        delta = urand(2, 4);

    if (key.quality > ITEM_QUALITY_NORMAL &&
            (requiredLevel > level || requiredLevel < level - delta))
        return false;

    for (uint32 gap = 60; gap <= 80; gap += 10)
    {
        if (level > gap && requiredLevel <= gap)
            return false;
    }

    return true;
}

bool RandomItemMgr::CanEquipItemNew(ItemPrototype const* proto)
{
    if (proto->Duration & 0x80000000)
        return false;

    if (proto->Bonding == BIND_QUEST_ITEM || proto->Bonding == BIND_WHEN_USE)
        return false;

    if (proto->Class == ITEM_CLASS_CONTAINER)
        return false;

    bool properSlot = false;
    for (map<EquipmentSlots, set<InventoryType> >::iterator i = viableSlots.begin(); i != viableSlots.end(); ++i)
    {
        set<InventoryType> slots = viableSlots[(EquipmentSlots)i->first];
        if (slots.find((InventoryType)proto->InventoryType) != slots.end())
            properSlot = true;
    }

    return properSlot;
}

void RandomItemMgr::AddItemSpellStats(uint32 smod, uint8& sp, uint8& ap, uint8& tank)
{
    switch (smod)
    {
    case SPELL_AURA_MOD_DAMAGE_DONE:
    case SPELL_AURA_MOD_HEALING_DONE:
    case SPELL_AURA_MOD_SPELL_CRIT_CHANCE:
    case SPELL_AURA_MOD_POWER_REGEN:
    case SPELL_AURA_MOD_MANA_REGEN_FROM_STAT:
    case SPELL_AURA_HASTE_SPELLS:
        sp++;
        break;
    }

    switch (smod)
    {

    case SPELL_AURA_MOD_EXPERTISE:
    case SPELL_AURA_MOD_ATTACK_POWER:
    case SPELL_AURA_MOD_CRIT_PERCENT:
    case SPELL_AURA_MOD_HIT_CHANCE:
    case SPELL_AURA_MOD_RANGED_ATTACK_POWER:
    case SPELL_AURA_EXTRA_ATTACKS:
    case SPELL_AURA_MOD_MELEE_HASTE:
    case SPELL_AURA_MOD_RANGED_HASTE:
        ap++;
        break;
    }

    switch (smod)
    {
    case SPELL_AURA_MOD_PARRY_PERCENT:
    case SPELL_AURA_MOD_DODGE_PERCENT:
    case SPELL_AURA_MOD_BLOCK_PERCENT:
    case SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN:
    case SPELL_AURA_MOD_BASE_RESISTANCE_PCT:
    case SPELL_AURA_MOD_BASE_RESISTANCE:
        //case SPELL_AURA_MOD_BLOCK_SKILL:
    case SPELL_AURA_MOD_SKILL:
    case SPELL_AURA_MOD_SHIELD_BLOCKVALUE:
    case SPELL_AURA_MOD_SHIELD_BLOCKVALUE_PCT:
        //case SPELL_AURA_MOD_HEALING_RECEIVED:
        tank++;
        break;
    }
}

void RandomItemMgr::AddItemStats(uint32 mod, uint8& sp, uint8& ap, uint8& tank)
{
    switch (mod)
    {
    case ITEM_MOD_HEALTH:
    case ITEM_MOD_STAMINA:
    case ITEM_MOD_MANA:
    case ITEM_MOD_INTELLECT:
    case ITEM_MOD_SPIRIT:
#ifndef MANGOSBOT_ZERO
    case ITEM_MOD_HIT_SPELL_RATING:
#endif
#ifdef MANGOSBOT_TWO
    case ITEM_MOD_SPELL_HEALING_DONE:
    case ITEM_MOD_SPELL_DAMAGE_DONE:
    case ITEM_MOD_MANA_REGENERATION:
    case ITEM_MOD_ARMOR_PENETRATION_RATING:
    case ITEM_MOD_SPELL_POWER:
    case ITEM_MOD_HEALTH_REGEN:
    case ITEM_MOD_SPELL_PENETRATION:
#endif
        sp++;
        break;
    }

    switch (mod)
    {
    case ITEM_MOD_AGILITY:
    case ITEM_MOD_STRENGTH:
    case ITEM_MOD_HEALTH:
    case ITEM_MOD_STAMINA:
#ifndef MANGOSBOT_ZERO
    case ITEM_MOD_DEFENSE_SKILL_RATING:
    case ITEM_MOD_DODGE_RATING:
    case ITEM_MOD_PARRY_RATING:
    case ITEM_MOD_BLOCK_RATING:
    case ITEM_MOD_CRIT_SPELL_RATING:
    case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
    case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
    case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
    case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
    case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
    case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
    case ITEM_MOD_HIT_TAKEN_RATING:
    case ITEM_MOD_CRIT_TAKEN_RATING:
    case ITEM_MOD_RESILIENCE_RATING:
#endif
#ifdef MANGOSBOT_TWO
    case ITEM_MOD_BLOCK_VALUE:
#endif
        tank++;
        break;
    }

    switch (mod)
    {
    case ITEM_MOD_HEALTH:
    case ITEM_MOD_STAMINA:
    case ITEM_MOD_AGILITY:
    case ITEM_MOD_STRENGTH:
#ifndef MANGOSBOT_ZERO:
    case ITEM_MOD_HIT_MELEE_RATING:
    case ITEM_MOD_HIT_RANGED_RATING:
    case ITEM_MOD_CRIT_MELEE_RATING:
    case ITEM_MOD_CRIT_RANGED_RATING:
    case ITEM_MOD_HASTE_MELEE_RATING:
    case ITEM_MOD_HASTE_RANGED_RATING:
    case ITEM_MOD_HIT_RATING:
    case ITEM_MOD_CRIT_RATING:
    case ITEM_MOD_HASTE_RATING:
    case ITEM_MOD_EXPERTISE_RATING:
#endif
#ifdef MANGOSBOT_TWO
    case ITEM_MOD_ATTACK_POWER:
    case ITEM_MOD_RANGED_ATTACK_POWER:
    case ITEM_MOD_FERAL_ATTACK_POWER:
#endif
        ap++;
        break;
    }
}

bool RandomItemMgr::CheckItemStats(uint8 clazz, uint8 sp, uint8 ap, uint8 tank)
{
    switch (clazz)
    {
    case CLASS_PRIEST:
    case CLASS_MAGE:
    case CLASS_WARLOCK:
        if (!sp || ap > sp || tank > sp)
            return false;
        break;
    case CLASS_PALADIN:
    case CLASS_WARRIOR:
#ifdef MANGOSBOT_TWO
    case CLASS_DEATH_KNIGHT:
#endif
        if ((!ap && !tank) || sp > ap || sp > tank)
            return false;
        break;
    case CLASS_HUNTER:
    case CLASS_ROGUE:
        if (!ap || sp > ap || sp > tank)
            return false;
        break;
    case CLASS_DRUID:
    case CLASS_SHAMAN:
        if ((!ap && !sp) || sp > ap || tank > sp)
            return false;
        break;
    }

    return sp || ap || tank;
}

bool RandomItemMgr::ShouldEquipArmorForSpec(uint8 playerclass, uint8 spec, ItemPrototype const* proto)
{
    if (proto->InventoryType == INVTYPE_TABARD)
        return true;

    if (!m_weightScales[playerclass][spec].info.id)
        return false;

    std::unordered_set<uint32> resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_CLOTH };

    switch (playerclass)
    {
    case CLASS_WARRIOR:
    {
        if (proto->InventoryType == INVTYPE_HOLDABLE)
            return false;

        if (m_weightScales[playerclass][spec].info.name == "arms" || m_weightScales[playerclass][spec].info.name == "fury")
        {
            resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_LEATHER, ITEM_SUBCLASS_ARMOR_MAIL, ITEM_SUBCLASS_ARMOR_PLATE };
        }
        else
            resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_MAIL, ITEM_SUBCLASS_ARMOR_PLATE };
        break;
    }
#ifdef MANGOSBOT_TWO
    case CLASS_DEATH_KNIGHT:
    {
        if (proto->InventoryType == INVTYPE_HOLDABLE)
            return false;

        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_SIGIL, ITEM_SUBCLASS_ARMOR_PLATE };
    }
#endif
    case CLASS_PALADIN:
    {
        if (m_weightScales[playerclass][spec].info.name != "holy" && proto->InventoryType == INVTYPE_HOLDABLE)
            return false;

        if (m_weightScales[playerclass][spec].info.name != "holy")
            resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_MAIL, ITEM_SUBCLASS_ARMOR_PLATE , ITEM_SUBCLASS_ARMOR_LIBRAM };
        else
            resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_CLOTH, ITEM_SUBCLASS_ARMOR_LEATHER, ITEM_SUBCLASS_ARMOR_MAIL, ITEM_SUBCLASS_ARMOR_PLATE, ITEM_SUBCLASS_ARMOR_LIBRAM };
        break;
    }
    case CLASS_HUNTER:
    {
        if (proto->InventoryType == INVTYPE_HOLDABLE)
            return false;

        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_CLOTH, ITEM_SUBCLASS_ARMOR_LEATHER, ITEM_SUBCLASS_ARMOR_MAIL };
        break;
    }
    case CLASS_ROGUE:
    {
        if (proto->InventoryType == INVTYPE_HOLDABLE)
            return false;

        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_CLOTH, ITEM_SUBCLASS_ARMOR_LEATHER };
        break;
    }
    case CLASS_PRIEST:
    {
        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_CLOTH };
        break;
    }
    case CLASS_SHAMAN:
    {
        if (m_weightScales[playerclass][spec].info.name == "enhance" && proto->InventoryType == INVTYPE_HOLDABLE)
            return false;

        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_TOTEM, ITEM_SUBCLASS_ARMOR_CLOTH, ITEM_SUBCLASS_ARMOR_LEATHER, ITEM_SUBCLASS_ARMOR_MAIL };
        break;
    }
    case CLASS_MAGE:
    case CLASS_WARLOCK:
    {
        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_CLOTH };
        break;
    }
    case CLASS_DRUID:
    {
        if ((m_weightScales[playerclass][spec].info.name == "feraltank" || m_weightScales[playerclass][spec].info.name == "feraldps") && proto->InventoryType == INVTYPE_HOLDABLE)
            return false;

        resultArmorSubClass = { ITEM_SUBCLASS_ARMOR_IDOL, ITEM_SUBCLASS_ARMOR_CLOTH, ITEM_SUBCLASS_ARMOR_LEATHER };
        break;
    }
    }

    return resultArmorSubClass.find(proto->SubClass) != resultArmorSubClass.end();
}

bool RandomItemMgr::CanEquipArmor(uint8 clazz, uint32 level, ItemPrototype const* proto)
{
    if (proto->InventoryType == INVTYPE_TABARD || proto->InventoryType == INVTYPE_NECK || proto->InventoryType == INVTYPE_CLOAK || proto->InventoryType == INVTYPE_FINGER || proto->InventoryType == INVTYPE_TRINKET)
        return true;

    if ((clazz == CLASS_WARRIOR || clazz == CLASS_PALADIN)
        && proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD || proto->SubClass == ITEM_SUBCLASS_ARMOR_BUCKLER)
        return true;

    if ((clazz == CLASS_SHAMAN)
        && proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD || proto->SubClass == ITEM_SUBCLASS_ARMOR_BUCKLER || proto->SubClass == ITEM_SUBCLASS_ARMOR_MISC)
        return true;

    if ((clazz == CLASS_WARRIOR || clazz == CLASS_PALADIN) && level >= 40)
    {
        if (proto->SubClass != ITEM_SUBCLASS_ARMOR_PLATE)
            return false;
    }

    if (((clazz == CLASS_WARRIOR || clazz == CLASS_PALADIN) && level < 40) ||
            ((clazz == CLASS_HUNTER || clazz == CLASS_SHAMAN) && level >= 40))
    {
        if (proto->SubClass != ITEM_SUBCLASS_ARMOR_MAIL)
            return false;
    }

    if (((clazz == CLASS_HUNTER || clazz == CLASS_SHAMAN) && level < 40) ||
            (clazz == CLASS_DRUID || clazz == CLASS_ROGUE))
    {
        if (proto->SubClass != ITEM_SUBCLASS_ARMOR_LEATHER)
            return false;
    }

    if (proto->Quality <= ITEM_QUALITY_NORMAL)
        return true;

    uint8 sp = 0, ap = 0, tank = 0;
    for (int j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
    {
        // for ItemStatValue != 0
        if(!proto->ItemStat[j].ItemStatValue)
            continue;

        AddItemStats(proto->ItemStat[j].ItemStatType, sp, ap, tank);
    }
    //filtering "spellstats" on items like spellpower/healing, attackpower,crit,hit,etc..
    for (int k = 0; k < MAX_ITEM_PROTO_SPELLS; k++)
    {
        const SpellEntry* const spellInfo = sServerFacade.LookupSpellInfo(proto->Spells[k].SpellId);
        if (!spellInfo)
            continue;

        for (int l = 0; l < 3; l++)
        {
            if (spellInfo->Effect[l] != SPELL_EFFECT_APPLY_AURA)
                continue;

            AddItemSpellStats(spellInfo->EffectApplyAuraName[l], sp, ap, tank);
        }
    }
    return CheckItemStats(clazz, sp, ap, tank);
}

bool RandomItemMgr::ShouldEquipWeaponForSpec(uint8 playerclass, uint8 spec, ItemPrototype const* proto)
{
    EquipmentSlots slot_mh = EQUIPMENT_SLOT_START;
    EquipmentSlots slot_oh = EQUIPMENT_SLOT_START;
    EquipmentSlots slot_rh = EQUIPMENT_SLOT_START;
    for (map<EquipmentSlots, set<InventoryType> >::iterator i = viableSlots.begin(); i != viableSlots.end(); ++i)
    {
        set<InventoryType> slots = viableSlots[(EquipmentSlots)i->first];
        if (slots.find((InventoryType)proto->InventoryType) != slots.end())
        {
            if (i->first == EQUIPMENT_SLOT_MAINHAND)
                slot_mh = i->first;
            if (i->first == EQUIPMENT_SLOT_OFFHAND)
                slot_oh = i->first;
            if (i->first == EQUIPMENT_SLOT_RANGED)
                slot_rh = i->first;
        }
    }

    if (slot_mh == EQUIPMENT_SLOT_START && slot_oh == EQUIPMENT_SLOT_START && slot_rh == EQUIPMENT_SLOT_START)
        return false;

    if (!m_weightScales[playerclass][spec].info.id)
        return false;

    std::unordered_set<uint32> mh_weapons;
    std::unordered_set<uint32> oh_weapons;
    std::unordered_set<uint32> r_weapons;

    switch (playerclass)
    {
    case CLASS_WARRIOR:
    {
        if (m_weightScales[playerclass][spec].info.name == "prot")
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_SWORD, ITEM_SUBCLASS_WEAPON_AXE, ITEM_SUBCLASS_WEAPON_MACE, ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_FIST };
            oh_weapons = { ITEM_SUBCLASS_ARMOR_SHIELD };
            r_weapons = { ITEM_SUBCLASS_WEAPON_BOW, ITEM_SUBCLASS_WEAPON_CROSSBOW, ITEM_SUBCLASS_WEAPON_GUN };
        }
        else
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_SWORD2, ITEM_SUBCLASS_WEAPON_AXE2, ITEM_SUBCLASS_WEAPON_MACE2, ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_POLEARM };
            r_weapons = { ITEM_SUBCLASS_WEAPON_BOW, ITEM_SUBCLASS_WEAPON_CROSSBOW, ITEM_SUBCLASS_WEAPON_GUN };
        }
        break;
    }
    case CLASS_PALADIN:
    {
        if (m_weightScales[playerclass][spec].info.name == "prot")
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_SWORD, ITEM_SUBCLASS_WEAPON_AXE, ITEM_SUBCLASS_WEAPON_MACE };
            oh_weapons = { ITEM_SUBCLASS_ARMOR_SHIELD };
            r_weapons = { ITEM_SUBCLASS_ARMOR_LIBRAM };
        }
        else if (m_weightScales[playerclass][spec].info.name == "holy")
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_SWORD, ITEM_SUBCLASS_WEAPON_AXE, ITEM_SUBCLASS_WEAPON_MACE };
            oh_weapons = { ITEM_SUBCLASS_ARMOR_SHIELD, ITEM_SUBCLASS_ARMOR_MISC };
            r_weapons = { ITEM_SUBCLASS_ARMOR_LIBRAM };
        }
        else
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_SWORD2, ITEM_SUBCLASS_WEAPON_AXE2, ITEM_SUBCLASS_WEAPON_MACE2, ITEM_SUBCLASS_WEAPON_POLEARM };
            r_weapons = { ITEM_SUBCLASS_ARMOR_LIBRAM };
        }
        break;
    }
    case CLASS_HUNTER:
    {
        mh_weapons = { ITEM_SUBCLASS_WEAPON_SWORD2, ITEM_SUBCLASS_WEAPON_AXE2, ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_POLEARM };
        r_weapons = { ITEM_SUBCLASS_WEAPON_BOW, ITEM_SUBCLASS_WEAPON_CROSSBOW, ITEM_SUBCLASS_WEAPON_GUN };
        break;
    }
    case CLASS_ROGUE:
    {
        mh_weapons = { ITEM_SUBCLASS_WEAPON_DAGGER };
        oh_weapons = { ITEM_SUBCLASS_WEAPON_DAGGER };
        r_weapons = { ITEM_SUBCLASS_WEAPON_BOW, ITEM_SUBCLASS_WEAPON_CROSSBOW, ITEM_SUBCLASS_WEAPON_GUN };
        break;
    }
    case CLASS_PRIEST:
    {
        mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_MACE };
        oh_weapons = { ITEM_SUBCLASS_ARMOR_MISC };
        r_weapons = { ITEM_SUBCLASS_WEAPON_WAND };
        break;
    }
    case CLASS_SHAMAN:
    {
        if (m_weightScales[playerclass][spec].info.name == "resto")
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_AXE, ITEM_SUBCLASS_WEAPON_MACE, ITEM_SUBCLASS_WEAPON_FIST };
            oh_weapons = { ITEM_SUBCLASS_ARMOR_MISC, ITEM_SUBCLASS_ARMOR_SHIELD };
            r_weapons = { ITEM_SUBCLASS_ARMOR_TOTEM };
        }
        else if (m_weightScales[playerclass][spec].info.name == "enhance")
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_MACE2, ITEM_SUBCLASS_WEAPON_AXE2, ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_AXE, ITEM_SUBCLASS_WEAPON_MACE, ITEM_SUBCLASS_WEAPON_FIST };
            oh_weapons = { ITEM_SUBCLASS_ARMOR_SHIELD };
            r_weapons = { ITEM_SUBCLASS_ARMOR_TOTEM };
        }
        else
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF };
            r_weapons = { ITEM_SUBCLASS_ARMOR_TOTEM };
        }
        break;
    }
    case CLASS_MAGE:
    case CLASS_WARLOCK:
    {
        mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_SWORD };
        oh_weapons = { ITEM_SUBCLASS_ARMOR_MISC };
        r_weapons = { ITEM_SUBCLASS_WEAPON_WAND };
        break;
    }
    case CLASS_DRUID:
    {
        if (m_weightScales[playerclass][spec].info.name == "feraltank")
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_MACE2 };
            r_weapons = { ITEM_SUBCLASS_ARMOR_IDOL };
        }
        else if (m_weightScales[playerclass][spec].info.name == "resto")
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_DAGGER, ITEM_SUBCLASS_WEAPON_FIST, ITEM_SUBCLASS_WEAPON_MACE };
            oh_weapons = { ITEM_SUBCLASS_ARMOR_MISC };
            r_weapons = { ITEM_SUBCLASS_ARMOR_IDOL };
        }
        else if (m_weightScales[playerclass][spec].info.name == "feraldps")
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF, ITEM_SUBCLASS_WEAPON_MACE2 };
            r_weapons = { ITEM_SUBCLASS_ARMOR_IDOL };
        }
        else
        {
            mh_weapons = { ITEM_SUBCLASS_WEAPON_STAFF };
            r_weapons = { ITEM_SUBCLASS_ARMOR_IDOL };
        }
        break;
    }
    }

    if (slot_mh == EQUIPMENT_SLOT_MAINHAND)
    {
        return mh_weapons.find(proto->SubClass) != mh_weapons.end();
    }
    if (slot_oh == EQUIPMENT_SLOT_OFFHAND)
    {
        return oh_weapons.find(proto->SubClass) != oh_weapons.end();
    }
    if (slot_rh == EQUIPMENT_SLOT_RANGED)
    {
        return r_weapons.find(proto->SubClass) != r_weapons.end();
    }

    return false;
}

bool RandomItemMgr::CanEquipWeapon(uint8 clazz, ItemPrototype const* proto)
{
    switch (clazz)
    {
    case CLASS_PRIEST:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_WAND &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE)
            return false;
        break;
    case CLASS_MAGE:
    case CLASS_WARLOCK:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_WAND &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
            return false;
        break;
    case CLASS_WARRIOR:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_GUN &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_BOW &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_POLEARM &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_THROWN)
            return false;
        break;
    case CLASS_PALADIN:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_POLEARM &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
            return false;
        break;
    case CLASS_SHAMAN:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF)
            return false;
        break;
    case CLASS_DRUID:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_POLEARM &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF)
            return false;
        break;
    case CLASS_HUNTER:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_POLEARM &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_GUN &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_BOW)
            return false;
        break;
    case CLASS_ROGUE:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_GUN &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST &&
#ifdef MANGOSBOT_TWO
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE &&
#endif
            proto->SubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_BOW &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_THROWN)
            return false;
        break;
#ifdef MANGOSBOT_TWO
    case CLASS_DEATH_KNIGHT:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_POLEARM &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE &&
            proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2)
            return false;
        break;
#endif
    }

    return true;
}

void RandomItemMgr::BuildItemInfoCache()
{
    uint32 maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);

    // load weightscales
    sLog.outString("Loading weightscales info");
    QueryResult* results = PlayerbotDatabase.PQuery("select id, name, class from ai_playerbot_weightscales");

    if (results)
    {
        int counter = 1;
        int totalcount = 0;
        int statcount = 0;
        int curClass = CLASS_WARRIOR;

        do
        {
            Field* fields = results->Fetch();
            uint32 id = fields[0].GetUInt32();
            string name = fields[1].GetString();
            uint32 clazz = fields[2].GetUInt32();

            if (clazz != curClass)
            {
                counter = 1;
                curClass = clazz;
            }

            WeightScale scale;
            scale.info.id = id;
            scale.info.name = name;
            m_weightScales[clazz][counter] = scale;
            counter++;
            totalcount++;

        } while (results->NextRow());
        delete results;

        sLog.outString("Loaded %d weightscale class specs", totalcount);

        QueryResult* result = PlayerbotDatabase.PQuery("select id, field, val from ai_playerbot_weightscale_data");
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                uint32 id = fields[0].GetUInt32();
                string field = fields[1].GetString();
                uint32 weight = fields[2].GetUInt32();

                WeightScaleStat stat;
                stat.stat = field;
                stat.weight = weight;

                for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES; ++cls)
                {
                    for (uint32 spec = 1; spec < 5; ++spec)
                    {
                        if (m_weightScales[cls][spec].info.id == id)
                            m_weightScales[cls][spec].stats.push_back(stat);
                    }
                }
                statcount++;

            } while (result->NextRow());
            delete result;
        }

        sLog.outString("Loaded %d weightscale stat weights", statcount);
    }

    if (m_weightScales[1].empty())
    {
        sLog.outError("Error loading item weight scales");
        return;
    }

    // vendor items
    sLog.outString("Loading vendor item list...");
    std::vector<uint32> vendorItems;
    vendorItems.clear();
    if (QueryResult* result = WorldDatabase.PQuery("%s", "SELECT item FROM npc_vendor"))
    {
        BarGoLink bar(result->GetRowCount());
        do
        {
            bar.step();
            Field* fields = result->Fetch();
            uint32 entry = fields[0].GetUInt32();
            if (!entry)
                continue;
            vendorItems.push_back(fields[0].GetUInt32());
        } while (result->NextRow());
        delete result;
    }
    sLog.outString("Loaded %d vendor items...", vendorItems.size());

    // calculate drop source
    sLog.outString("Loading loot templates...");
    DropMap* dropMap = new DropMap;

    int32 sEntry;

    for (uint32 entry = 0; entry < sCreatureStorage.GetMaxEntry(); entry++)
    {
        sEntry = entry;

        LootTemplateAccess const* lTemplateA = DropMapValue::GetLootTemplate(ObjectGuid(HIGHGUID_UNIT, entry, uint32(1)), LOOT_CORPSE);

        if (lTemplateA)
            for (LootStoreItem const& lItem : lTemplateA->Entries)
                dropMap->insert(make_pair(lItem.itemid, sEntry));
    }

    for (uint32 entry = 0; entry < sGOStorage.GetMaxEntry(); entry++)
    {
        sEntry = entry;

        LootTemplateAccess const* lTemplateA = DropMapValue::GetLootTemplate(ObjectGuid(HIGHGUID_GAMEOBJECT, entry, uint32(1)), LOOT_CORPSE);

        if (lTemplateA)
            for (LootStoreItem const& lItem : lTemplateA->Entries)
                dropMap->insert(make_pair(lItem.itemid, -sEntry));
    }

    sLog.outString("Loaded %d loot templates...", dropMap->size());

    sLog.outString("Calculating stat weights for %d items...", sItemStorage.GetMaxEntry());
    BarGoLink bar(sItemStorage.GetMaxEntry());

    // generate stat weights for classes/specs
    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        bar.step();

        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
            continue;

        // skip non armor/weapon
        if (proto->Class != ITEM_CLASS_WEAPON &&
            proto->Class != ITEM_CLASS_ARMOR &&
            proto->Class != ITEM_CLASS_CONTAINER &&
            proto->Class != ITEM_CLASS_PROJECTILE)
            continue;

        if (!CanEquipItemNew(proto))
            continue;

        // skip test items
        if (strstr(proto->Name1, "(Test)") ||
            strstr(proto->Name1, "(TEST)") ||
            strstr(proto->Name1, "(test)") ||
            strstr(proto->Name1, "(JEFFTEST)") ||
            strstr(proto->Name1, "Test ") ||
            strstr(proto->Name1, "Test") ||
            strstr(proto->Name1, "TEST") ||
            strstr(proto->Name1, "TEST ") ||
            strstr(proto->Name1, " TEST") ||
            strstr(proto->Name1, "2200 ") ||
            strstr(proto->Name1, "Deprecated ") ||
            strstr(proto->Name1, "Unused ") ||
            strstr(proto->Name1, "Monster ") ||
            strstr(proto->Name1, "[PH]") ||
            strstr(proto->Name1, "(OLD)")
            )
            continue;

        // skip items with rank/rep requirements
        /*if (proto->RequiredHonorRank > 0 ||
            proto->RequiredSkillRank > 0 ||
            proto->RequiredCityRank > 0 ||
            proto->RequiredReputationRank > 0)
            continue;*/

        if (proto->RequiredHonorRank > 0 ||
            proto->RequiredSkillRank > 0 ||
            proto->RequiredCityRank > 0)
            continue;

        // skip random enchant items
        if (proto->RandomProperty)
            continue;

#ifdef MANGOSBOT_TWO
        // skip heirloom items
        if (proto->Quality == ITEM_QUALITY_HEIRLOOM)
            continue;
#endif

        // check possible equip slots
        EquipmentSlots slot = EQUIPMENT_SLOT_START;
        for (map<EquipmentSlots, set<InventoryType> >::iterator i = viableSlots.begin(); i != viableSlots.end(); ++i)
        {
            set<InventoryType> slots = viableSlots[(EquipmentSlots)i->first];
            if (slots.find((InventoryType)proto->InventoryType) != slots.end())
                slot = i->first;
        }

        if (slot == EQUIPMENT_SLOT_START)
            continue;

        // Init Item cache
        ItemInfoEntry* cacheInfo = new ItemInfoEntry;

        for (uint8 clazz = CLASS_WARRIOR; clazz < MAX_CLASSES; ++clazz)
        {
            // skip nonexistent classes
            if (!((1 << (clazz - 1)) & CLASSMASK_ALL_PLAYABLE) || !sChrClassesStore.LookupEntry(clazz))
                continue;

            // skip wrong classes
            if ((proto->AllowableClass & (1 << (clazz - 1))) == 0)
                continue;

            for (uint32 spec = 1; spec < 5; ++spec)
            {
                if (!m_weightScales[clazz][spec].info.id)
                    continue;

                // check possible armor for spec
                if (m_weightScales)
                if (proto->Class == ITEM_CLASS_ARMOR && (
                    slot == EQUIPMENT_SLOT_HEAD ||
                    slot == EQUIPMENT_SLOT_SHOULDERS ||
                    slot == EQUIPMENT_SLOT_CHEST ||
                    slot == EQUIPMENT_SLOT_WAIST ||
                    slot == EQUIPMENT_SLOT_LEGS ||
                    slot == EQUIPMENT_SLOT_FEET ||
                    slot == EQUIPMENT_SLOT_WRISTS ||
                    slot == EQUIPMENT_SLOT_HANDS) &&
                    !ShouldEquipArmorForSpec(clazz, spec, proto))
                    continue;

                // check possible weapon for spec
                if ((proto->Class == ITEM_CLASS_WEAPON || (proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD || (proto->SubClass == ITEM_SUBCLASS_ARMOR_MISC && proto->InventoryType == INVTYPE_HOLDABLE))) &&
                    !ShouldEquipWeaponForSpec(clazz, spec, proto))
                    continue;

                StatWeight statWeight;
                statWeight.id = m_weightScales[clazz][spec].info.id;
                uint32 statW = CalculateStatWeight(clazz, spec, proto);
                // set stat weight = 1 for items that can be equipped but have no proper stats
                statWeight.weight = statW ? statW : 1;
                //statWeight.weight = statW;
                // save item statWeight into ItemCache
                cacheInfo->weights[statWeight.id] = statWeight.weight;
                sLog.outDetail("Item: %d, weight: %d, class: %d, spec: %s", proto->ItemId, statWeight.weight, clazz, m_weightScales[clazz][spec].info.name);
            }
        }

#ifdef MANGOSBOT_TWO
        // check faction
        if (proto->Flags2 & ITEM_FLAG2_FACTION_HORDE)
            cacheInfo->team = HORDE;

        if (proto->Flags2 & ITEM_FLAG2_FACTION_ALLIANCE)
            cacheInfo->team = ALLIANCE;

        if (!cacheInfo->team && proto->AllowableRace > 1 && proto->AllowableRace < 8388607)
        {
            if (FactionEntry const* faction = sFactionStore.LookupEntry(HORDE))
                if ((proto->AllowableRace & faction->BaseRepRaceMask[0]) != 0)
                    cacheInfo->team = HORDE;

            if (FactionEntry const* faction = sFactionStore.LookupEntry(ALLIANCE))
                if ((proto->AllowableRace & faction->BaseRepRaceMask[0]) != 0)
                    cacheInfo->team = ALLIANCE;
        }
#endif
#ifdef MANGOSBOT_ONE
        // check faction
        if (!cacheInfo->team && proto->AllowableRace > 1 && proto->AllowableRace < 8388607)
        {
            if (FactionEntry const* faction = sFactionStore.LookupEntry<FactionEntry>(HORDE))
                if ((proto->AllowableRace & faction->BaseRepRaceMask[0]) != 0)
                    cacheInfo->team = HORDE;

            if (FactionEntry const* faction = sFactionStore.LookupEntry<FactionEntry>(ALLIANCE))
                if ((proto->AllowableRace & faction->BaseRepRaceMask[0]) != 0)
                    cacheInfo->team = ALLIANCE;
        }
#endif
#ifdef MANGOSBOT_ZERO
        // check faction
        if (!cacheInfo->team && proto->AllowableRace > 1 && proto->AllowableRace < 8388607)
        {
            if (FactionEntry const* faction = sFactionStore.LookupEntry(HORDE))
                if ((proto->AllowableRace & faction->BaseRepRaceMask[0]) != 0)
                    cacheInfo->team = HORDE;

            if (FactionEntry const* faction = sFactionStore.LookupEntry(ALLIANCE))
                if ((proto->AllowableRace & faction->BaseRepRaceMask[0]) != 0)
                    cacheInfo->team = ALLIANCE;
        }
#endif

        if (cacheInfo->team)
            sLog.outDetail("Item: %d, team (item): %s", proto->ItemId, cacheInfo->team == ALLIANCE ? "Alliance" : "Horde");

        // check min level
        if (proto->RequiredLevel)
            cacheInfo->minLevel = proto->RequiredLevel;

        // check item source

        if (proto->Flags & ITEM_FLAG_NO_DISENCHANT)
        {
            cacheInfo->source = ITEM_SOURCE_PVP;
            sLog.outDetail("Item: %d, source: PvP Reward", proto->ItemId);
        }

        // check quests
        if (cacheInfo->source == ITEM_SOURCE_NONE)
        {
            vector<uint32> questIds = GetQuestIdsForItem(proto->ItemId);
            if (questIds.size())
            {
                bool isAlly = false;
                bool isHorde = false;
                for (vector<uint32>::iterator i = questIds.begin(); i != questIds.end(); ++i)
                {
                    Quest const* quest = sObjectMgr.GetQuestTemplate(*i);
                    if (quest)
                    {
                        cacheInfo->source = ITEM_SOURCE_QUEST;
                        cacheInfo->sourceId = *i;
                        if (!cacheInfo->minLevel)
                            cacheInfo->minLevel = quest->GetMinLevel();

                        // check quest team
                        if (!cacheInfo->team)
                        {
                            uint32 reqRace = quest->GetRequiredRaces();
                            if (reqRace)
                            {
                                if ((reqRace & RACEMASK_ALLIANCE) != 0)
                                    isAlly = true;
                                else if ((reqRace & RACEMASK_HORDE) != 0)
                                    isHorde = true;
                            }
                        }
                    }
                }
                if (isAlly && isHorde)
                    cacheInfo->team = TEAM_BOTH_ALLOWED;
                else if (isAlly)
                    cacheInfo->team = ALLIANCE;
                else if (isHorde)
                    cacheInfo->team = HORDE;

                sLog.outDetail("Item: %d, team (quest): %s", proto->ItemId, cacheInfo->team == ALLIANCE ? "Alliance" : cacheInfo->team == HORDE ? "Horde" : "Both");
                sLog.outDetail("Item: %d, source: quest %d, minlevel: %d", proto->ItemId, cacheInfo->sourceId, cacheInfo->minLevel);
            }
        }

        if (cacheInfo->minLevel)
            sLog.outDetail("Item: %d, minlevel: %d", proto->ItemId, cacheInfo->minLevel);

        // check vendors
        if (cacheInfo->source == ITEM_SOURCE_NONE)
        {
            for (vector<uint32>::iterator i = vendorItems.begin(); i != vendorItems.end(); ++i)
            {
                if (proto->ItemId == *i)
                {
                    cacheInfo->source = ITEM_SOURCE_VENDOR;
                    sLog.outDetail("Item: %d, source: vendor", proto->ItemId);
                    break;
                }
            }
        }

        // check drops
        list<int32> creatures;
        list<int32> gameobjects;

        auto range = dropMap->equal_range(itemId);

        for (auto itr = range.first; itr != range.second; ++itr)
        {
            if (itr->second > 0)
                creatures.push_back(itr->second);
            else
                gameobjects.push_back(abs(itr->second));
        }
        
        // check creature drop
        if (cacheInfo->source == ITEM_SOURCE_NONE)
        {
            if (creatures.size())
            {
                if (creatures.size() == 1)
                {
                    cacheInfo->source = ITEM_SOURCE_DROP;
                    cacheInfo->sourceId = creatures.front();
                    sLog.outDetail("Item: %d, source: creature drop, ID: %d", proto->ItemId, creatures.front());
                }
                else
                {
                    cacheInfo->source = ITEM_SOURCE_DROP;
                    sLog.outDetail("Item: %d, source: creatures drop, number: %d", proto->ItemId, creatures.size());
                }
            }
        }

        // check gameobject drop
        if (cacheInfo->source == ITEM_SOURCE_NONE || (cacheInfo->source == ITEM_SOURCE_DROP && !cacheInfo->sourceId))
        {
            if (gameobjects.size())
            {
                if (gameobjects.size() == 1)
                {
                    cacheInfo->source = ITEM_SOURCE_DROP;
                    cacheInfo->sourceId = gameobjects.front();
                    sLog.outDetail("Item: %d, source: gameobject, ID: %d", proto->ItemId, gameobjects.front());
                }
                else
                {
                    cacheInfo->source = ITEM_SOURCE_DROP;
                    sLog.outDetail("Item: %d, source: gameobjects, number: %d", proto->ItemId, gameobjects.size());
                }
            }
        }

        // check faction
        if (proto->RequiredReputationFaction > 0 && proto->RequiredReputationFaction != 35 && proto->RequiredReputationRank < 15)
        {
            cacheInfo->repFaction = proto->RequiredReputationFaction;
            cacheInfo->repRank = proto->RequiredReputationRank;
        }

        cacheInfo->quality = proto->Quality;
        cacheInfo->itemId = proto->ItemId;
        cacheInfo->slot = slot;

        // save cache
        static SqlStatementID delCache;
        static SqlStatementID insertCache;

        SqlStatement stmt = PlayerbotDatabase.CreateStatement(delCache, "DELETE FROM ai_playerbot_item_info_cache WHERE id = ?");
        stmt.PExecute(proto->ItemId);

        stmt = PlayerbotDatabase.CreateStatement(insertCache, "INSERT INTO ai_playerbot_item_info_cache (id, quality, slot, source, sourceId, team, faction, factionRepRank, minLevel, "
            "scale_1, scale_2, scale_3, scale_4, scale_5, scale_6, scale_7, scale_8, scale_9, scale_10, scale_11, scale_12, scale_13, scale_14, scale_15, "
            "scale_16, scale_17, scale_18, scale_19, scale_20, scale_21, scale_22, scale_23, scale_24, scale_25, scale_26, scale_27, scale_28, scale_29, scale_30, scale_31, scale_32)"
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        stmt.addUInt32(cacheInfo->itemId);
        stmt.addUInt32(cacheInfo->quality);
        stmt.addUInt32(cacheInfo->slot);
        stmt.addUInt32(cacheInfo->source);
        stmt.addUInt32(cacheInfo->sourceId);
        stmt.addUInt32(cacheInfo->team);
        stmt.addUInt32(cacheInfo->repFaction);
        stmt.addUInt32(cacheInfo->repRank);
        stmt.addUInt32(cacheInfo->minLevel);

        for (int i = 1; i <= MAX_STAT_SCALES; ++i)
        {
            if (cacheInfo->weights[i])
                stmt.addUInt32(cacheInfo->weights[i]);
            else
                stmt.addUInt32(0);
        }

        stmt.Execute();

        itemInfoCache[cacheInfo->itemId] = cacheInfo;
    }
}

uint32 RandomItemMgr::CalculateStatWeight(uint8 playerclass, uint8 spec, ItemPrototype const* proto)
{
    uint32 statWeight = 0;
    bool isCasterItem = false;
    bool isAttackItem = false;
#ifdef MANGOSBOT_TWO
    bool noCaster = (Classes)playerclass == CLASS_WARRIOR || (Classes)playerclass == CLASS_ROGUE || (Classes)playerclass == CLASS_DEATH_KNIGHT || (Classes)playerclass == CLASS_HUNTER;
    uint32 spellPower = 0;
    uint32 spellHeal = 0;
    uint32 attackPower = 0;
    bool hasInt = false;
    bool hasMana = !((Classes)playerclass == CLASS_WARRIOR || (Classes)playerclass == CLASS_ROGUE || (Classes)playerclass == CLASS_DEATH_KNIGHT);

    if (proto->SubClass == ITEM_SUBCLASS_ARMOR_LIBRAM || proto->SubClass == ITEM_SUBCLASS_ARMOR_IDOL || proto->SubClass == ITEM_SUBCLASS_ARMOR_TOTEM || proto->SubClass == ITEM_SUBCLASS_ARMOR_SIGIL)
        return (uint32)(proto->Quality + proto->ItemLevel);
#else
    bool noCaster = (Classes)playerclass == CLASS_WARRIOR || (Classes)playerclass == CLASS_ROGUE || (Classes)playerclass == CLASS_HUNTER;
    uint32 spellPower = 0;
    uint32 spellHeal = 0;
    uint32 attackPower = 0;
    bool hasInt = false;
    bool hasMana = !((Classes)playerclass == CLASS_WARRIOR || (Classes)playerclass == CLASS_ROGUE);

    if (proto->SubClass == ITEM_SUBCLASS_ARMOR_LIBRAM || proto->SubClass == ITEM_SUBCLASS_ARMOR_IDOL || proto->SubClass == ITEM_SUBCLASS_ARMOR_TOTEM)
        return (uint32)(proto->Quality + proto->ItemLevel);
#endif

    // check basic item stats
    int32 basicStatsWeight = 0;
    for (int j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
    {
        uint32 statType;
        int32 val;
        string weightName;

        //if (j >= proto->StatsCount)
        //    continue;

        statType = proto->ItemStat[j].ItemStatType;
        val = proto->ItemStat[j].ItemStatValue;

        if (val == 0)
            continue;

        for (map<string, uint32 >::iterator i = weightStatLink.begin(); i != weightStatLink.end(); ++i)
        {
            uint32 modd = i->second;
            if (modd == statType)
            {
                weightName = i->first;
                break;
            }
        }

        if (weightName.empty())
            continue;

        uint32 singleStat = CalculateSingleStatWeight(playerclass, spec, weightName, val);
        basicStatsWeight += singleStat;

        if (val)
        {
            if (weightName == "int" && !noCaster)
                isCasterItem = true;

            if (weightName == "int")
                hasInt = true;

            if (weightName == "splpwr")
                isCasterItem = true;

            if (weightName == "str")
                isAttackItem = true;

            if (weightName == "agi")
                isAttackItem = true;

            if (weightName == "atkpwr")
                isAttackItem = true;
        }
    }

    // check armor & block
    statWeight += CalculateSingleStatWeight(playerclass, spec, "armor", proto->Armor);
    statWeight += CalculateSingleStatWeight(playerclass, spec, "block", proto->Block);

    // check weapon dps
    if (proto->IsWeapon())
    {
        WeaponAttackType attType = BASE_ATTACK;

        uint32 dps = 0;

        for (int i = 0; i < MAX_ITEM_PROTO_DAMAGES; i++)
        {
            if (proto->Damage[i].DamageMax == 0)
                break;

            dps = (proto->Damage[i].DamageMin + proto->Damage[i].DamageMax) / (float)(proto->Delay / 1000.0f) / 2;
            if (dps)
            {
                if (proto->IsRangedWeapon())
                    statWeight += CalculateSingleStatWeight(playerclass, spec, "rgddps", dps);
                else
                    statWeight += CalculateSingleStatWeight(playerclass, spec, "mledps", dps);
            }
        }
    }

    // check item spells
    for (const auto& spellData : proto->Spells)
    {
        // no spell
        if (!spellData.SpellId)
            continue;

        // apply only at-equip spells
        if (spellData.SpellTrigger != ITEM_SPELLTRIGGER_ON_EQUIP)
            continue;

        // check if it is valid spell
        SpellEntry const* spellproto = sSpellTemplate.LookupEntry<SpellEntry>(spellData.SpellId);
        if (!spellproto)
            continue;

        uint32 spellDamage = 0;
        uint32 spellHealing = 0;
        for (uint32 j = 0; j < MAX_EFFECT_INDEX; j++)
        {
            if ((spellproto->Effect[j] == SPELL_EFFECT_APPLY_AURA) &&
                (spellproto->EffectBasePoints[j]))
            {
#ifdef MANGOSBOT_TWO
                // spell damage
                // SPELL_AURA_MOD_DAMAGE_DONE
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_DAMAGE_DONE)
                {
                    spellDamage = spellproto->EffectBasePoints[j] + 1;
                }
                // spell healing
                // SPELL_AURA_MOD_HEALING_DONE
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_HEALING_DONE)
                {
                    spellHealing = spellproto->EffectBasePoints[j] + 1;
                }
                // check spell power
                if (spellDamage && spellDamage == spellHealing)
                {
                    isCasterItem = true;
                    spellPower += CalculateSingleStatWeight(playerclass, spec, "splpwr", spellDamage);
                }
#else
                // spell damage
                // SPELL_AURA_MOD_DAMAGE_DONE
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_DAMAGE_DONE)
                {
                    spellDamage = spellproto->EffectBasePoints[j] + 1;
                    // generic spell damage
                    if (spellproto->EffectMiscValue[j] == SPELL_SCHOOL_MASK_MAGIC)
                    {
                        spellPower += CalculateSingleStatWeight(playerclass, spec, "splpwr", spellDamage);
                    }
                    else
                    {
                        if (spellproto->EffectMiscValue[j] & SPELL_SCHOOL_MASK_ARCANE)
                            spellPower += CalculateSingleStatWeight(playerclass, spec, "arcsplpwr", spellDamage);

                        if (spellproto->EffectMiscValue[j] & SPELL_SCHOOL_MASK_FROST)
                            spellPower += CalculateSingleStatWeight(playerclass, spec, "frosplpwr", spellDamage);

                        if (spellproto->EffectMiscValue[j] & SPELL_SCHOOL_MASK_FIRE)
                            spellPower += CalculateSingleStatWeight(playerclass, spec, "firsplpwr", spellDamage);

                        if (spellproto->EffectMiscValue[j] & SPELL_SCHOOL_MASK_SHADOW)
                            spellPower += CalculateSingleStatWeight(playerclass, spec, "shasplpwr", spellDamage);

                        if (spellproto->EffectMiscValue[j] & SPELL_SCHOOL_MASK_NATURE)
                            spellPower += CalculateSingleStatWeight(playerclass, spec, "natsplpwr", spellDamage);
                    }
                }
                // spell healing
                // SPELL_AURA_MOD_HEALING_DONE
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_HEALING_DONE)
                {
                    spellHeal += CalculateSingleStatWeight(playerclass, spec, "splheal", spellproto->EffectBasePoints[j] + 1);
                }
#endif

                // spell hit rating (pre tbc)
                // SPELL_AURA_MOD_SPELL_HIT_CHANCE
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_SPELL_HIT_CHANCE)
                {
                    statWeight += CalculateSingleStatWeight(playerclass, spec, "spellhitrtng", spellproto->EffectBasePoints[j] + 1);
                }

                // spell crit rating (pre tbc)
                // SPELL_AURA_MOD_SPELL_CRIT_CHANCE
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_SPELL_CRIT_CHANCE)
                {
                    statWeight += CalculateSingleStatWeight(playerclass, spec, "spellcritstrkrtng", spellproto->EffectBasePoints[j] + 1);
                }

                // spell penetration
                // SPELL_AURA_MOD_TARGET_RESISTANCE
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_TARGET_RESISTANCE)
                {
                    // check if magic type
                    if (spellproto->EffectMiscValue[j] == SPELL_SCHOOL_MASK_SPELL)
                        statWeight += CalculateSingleStatWeight(playerclass, spec, "spellpenrtng", abs(spellproto->EffectBasePoints[j] + 1));
                }

                // check attack power
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_ATTACK_POWER)
                {
                    isAttackItem = true;
                    attackPower += CalculateSingleStatWeight(playerclass, spec, "atkpwr", spellproto->EffectBasePoints[j] + 1);
                }

                // check ranged ap
                // SPELL_AURA_MOD_RANGED_ATTACK_POWER
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_RANGED_ATTACK_POWER)
                {
                    isAttackItem = true;
                    attackPower += CalculateSingleStatWeight(playerclass, spec, "atkpwr", spellproto->EffectBasePoints[j] + 1);
                }

                // check block
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_SHIELD_BLOCKVALUE)
                {
                    statWeight += CalculateSingleStatWeight(playerclass, spec, "block", spellproto->EffectBasePoints[j] + 1);
                }

                // block chance
                // SPELL_AURA_MOD_BLOCK_PERCENT
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_BLOCK_PERCENT)
                {
                    statWeight += CalculateSingleStatWeight(playerclass, spec, "blockrtng", spellproto->EffectBasePoints[j] + 1);
                }

                // armor penetration
                // SPELL_AURA_MOD_TARGET_RESISTANCE
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_TARGET_RESISTANCE)
                {
                    // check if physical type
                    if (spellproto->EffectMiscValue[j] == SPELL_SCHOOL_MASK_NORMAL)
                        statWeight += CalculateSingleStatWeight(playerclass, spec, "armorpenrtng", abs(spellproto->EffectBasePoints[j] + 1));
                }

                // hit rating (pre tbc)
                // SPELL_AURA_MOD_HIT_CHANCE
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_HIT_CHANCE)
                {
                    statWeight += CalculateSingleStatWeight(playerclass, spec, "hitrtng", spellproto->EffectBasePoints[j] + 1);
                }

                // crit rating (pre tbc)
                // SPELL_AURA_MOD_HIT_CHANCE
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_CRIT_PERCENT)
                {
                    statWeight += CalculateSingleStatWeight(playerclass, spec, "critstrkrtng", spellproto->EffectBasePoints[j] + 1);
                }

                //check defense SPELL_AURA_MOD_SKILL
                //if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_SKILL)
                //{
                //    statWeight += CalculateSingleStatWeight(playerclass, spec, "block", spellproto->EffectBasePoints[j] + 1);
                //}

#ifndef MANGOSBOT_ZERO
                // ratings
                // enum CombatRating
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_RATING)
                {
                    for (uint32 rating = 0; rating < MAX_COMBAT_RATING; ++rating)
                    {
                        if (spellproto->EffectMiscValue[0] & (1 << rating))
                        {
                            int32 val = spellproto->EffectBasePoints[j] + 1;
                            string weightName;

                            for (map<string, uint32 >::iterator i = weightRatingLink.begin(); i != weightRatingLink.end(); ++i)
                            {
                                uint32 modd = i->second;
                                if (modd == rating)
                                {
                                    weightName = i->first;
                                    break;
                                }
                            }

                            if (weightName.empty())
                                continue;

                            statWeight += CalculateSingleStatWeight(playerclass, spec, weightName, val);
                        }
                    }
                }
#endif

                // mana regen
                // SPELL_AURA_MOD_POWER_REGEN
                if (spellproto->EffectApplyAuraName[j] == SPELL_AURA_MOD_POWER_REGEN)
                {
                    statWeight += CalculateSingleStatWeight(playerclass, spec, "manargn", spellproto->EffectBasePoints[j] + 1);
                }
            }
        }
    }

    // check for caster item
    if (isCasterItem || hasInt)
    {
        if ((!hasMana || noCaster) && spellPower)
            return 0;

        if (!hasMana && hasInt)
            return 0;

        if ((!hasMana && noCaster && playerclass != CLASS_PALADIN) && spellPower > attackPower)
            return 0;

        bool playerCaster = false;
        for (vector<WeightScaleStat>::iterator i = m_weightScales[playerclass][spec].stats.begin(); i != m_weightScales[playerclass][spec].stats.end(); ++i)
        {
            if (i->stat == "splpwr" || i->stat == "int" || i->stat == "manargn")
            {
                playerCaster = true;
            }
        }

        if (!playerCaster)
            return 0;
    }

    // check for caster item
    if (isAttackItem)
    {
        if (hasMana && !noCaster && !(hasInt || spellPower))
            return 0;
        //if (!noCaster && attackPower)
        //    return 0;

        bool playerAttacker = false;
        for (vector<WeightScaleStat>::iterator i = m_weightScales[playerclass][spec].stats.begin(); i != m_weightScales[playerclass][spec].stats.end(); ++i)
        {
            if (i->stat == "str" || i->stat == "agi" || i->stat == "atkpwr" || i->stat == "mledps" || i->stat == "rgddps")
            {
                playerAttacker = true;
            }
        }

        if (!playerAttacker)
            return 0;
    }

    statWeight += spellPower;
    statWeight += spellHeal;
    statWeight += attackPower;

    // handle negative stats
    if (basicStatsWeight < 0 && (abs(basicStatsWeight) >= statWeight))
        statWeight = 0;
    else
        statWeight += basicStatsWeight;

    return statWeight;
}

uint32 RandomItemMgr::CalculateSingleStatWeight(uint8 playerclass, uint8 spec, std::string stat, uint32 value)
{
    uint32 statWeight = 0;
    for (vector<WeightScaleStat>::iterator i = m_weightScales[playerclass][spec].stats.begin(); i != m_weightScales[playerclass][spec].stats.end(); ++i)
    {
        if (stat == i->stat)
        {
            statWeight = i->weight * value;
            if (statWeight)
                sLog.outDetail("stat: %s, val: %d, weight: %d, total: %d, class: %d, spec: %s", stat, value, i->weight, statWeight, playerclass, m_weightScales[playerclass][spec].info.name);
            return statWeight;
        }
    }

    return statWeight;
}

uint32 RandomItemMgr::GetQuestIdForItem(uint32 itemId)
{
    bool isQuest = false;
    uint32 questId = 0;
    ObjectMgr::QuestMap const& questTemplates = sObjectMgr.GetQuestTemplates();
    for (ObjectMgr::QuestMap::const_iterator i = questTemplates.begin(); i != questTemplates.end(); ++i)
    {
        Quest const* quest = i->second;

        uint32 rewItemCount = quest->GetRewItemsCount();
        for (uint32 i = 0; i < rewItemCount; ++i)
        {
            if (!quest->RewItemId[i])
                continue;

            if (quest->RewItemId[i] == itemId)
            {
                isQuest = true;
                questId = quest->GetQuestId();
                break;
            }
        }

        uint32 rewChocieItemCount = quest->GetRewChoiceItemsCount();
        for (uint32 i = 0; i < rewChocieItemCount; ++i)
        {
            if (!quest->RewChoiceItemId[i])
                continue;

            if (quest->RewChoiceItemId[i] == itemId)
            {
                isQuest = true;
                questId = quest->GetQuestId();
                break;
            }
        }
        if (isQuest)
            break;
    }
    return questId;
}

vector<uint32> RandomItemMgr::GetQuestIdsForItem(uint32 itemId)
{
    vector<uint32> questIds;
    ObjectMgr::QuestMap const& questTemplates = sObjectMgr.GetQuestTemplates();
    for (ObjectMgr::QuestMap::const_iterator i = questTemplates.begin(); i != questTemplates.end(); ++i)
    {
        Quest const* quest = i->second;

        uint32 rewItemCount = quest->GetRewItemsCount();
        for (uint32 i = 0; i < rewItemCount; ++i)
        {
            if (!quest->RewItemId[i])
                continue;

            if (quest->RewItemId[i] == itemId)
            {
                questIds.push_back(quest->GetQuestId());
                break;
            }
        }

        uint32 rewChocieItemCount = quest->GetRewChoiceItemsCount();
        for (uint32 i = 0; i < rewChocieItemCount; ++i)
        {
            if (!quest->RewChoiceItemId[i])
                continue;

            if (quest->RewChoiceItemId[i] == itemId)
            {
                questIds.push_back(quest->GetQuestId());
                break;
            }
        }
    }
    return questIds;
}

uint32 RandomItemMgr::GetUpgrade(Player* player, std::string spec, uint8 slot, uint32 quality, uint32 itemId)
{
    if (!player)
        return 0;

    // get old item statWeight
    uint32 oldStatWeight = 0;
    uint32 specId = 0;
    uint32 closestUpgrade = 0;
    uint32 closestUpgradeWeight = 0;
    vector<uint32> classspecs;

    for (uint32 specNum = 1; specNum < 5; ++specNum)
    {
        if (!m_weightScales[player->getClass()][specNum].info.id)
            continue;

        classspecs.push_back(m_weightScales[player->getClass()][specNum].info.id);

        if (m_weightScales[player->getClass()][specNum].info.name == spec)
            specId = m_weightScales[player->getClass()][specNum].info.id;
    }
    if (!specId)
        return 0;

    if (itemId && itemInfoCache[itemId])
    {
        oldStatWeight = itemInfoCache[itemId]->weights[specId];

        if (oldStatWeight)
            sLog.outString("Old Item: %d, weight: %d", itemId, oldStatWeight);
        else
            sLog.outString("Old item has no stat weight");
    }

    for (map<uint32, ItemInfoEntry*>::iterator i = itemInfoCache.begin(); i != itemInfoCache.end(); ++i)
    {
        ItemInfoEntry* info = i->second;
        if (!info)
            continue;

        // skip useless items
        if (info->weights[specId] == 0)
            continue;

        // skip higher lvl
        if (info->minLevel > player->getLevel())
            continue;

        // skip too low level
        if (info->minLevel < (player->getLevel() - 10))
            continue;

        // skip wrong team
        if (info->team && info->team != player->GetTeam())
            continue;

        // skip wrong slot
        if ((EquipmentSlots)info->slot != (EquipmentSlots)slot)
            continue;

        // skip higher quality
        if (quality && info->quality != quality)
            continue;

        // skip worse items
        if (info->weights[specId] <= oldStatWeight)
            continue;

        // skip items that only fit in slot, but not stats
        if (!itemId && info->weights[specId] == 1 && player->getLevel() > 40)
            continue;

        // skip quest items
        if (info->source == ITEM_SOURCE_QUEST)
        {
            if (player->GetQuestRewardStatus(info->sourceId) != QUEST_STATUS_COMPLETE)
                continue;
        }

        // skip no stats trinkets
        if (info->weights[specId] == 1 &&
            info->slot == EQUIPMENT_SLOT_NECK ||
            info->slot == EQUIPMENT_SLOT_TRINKET1 ||
            info->slot == EQUIPMENT_SLOT_TRINKET2 ||
            info->slot == EQUIPMENT_SLOT_FINGER1 ||
            info->slot == EQUIPMENT_SLOT_FINGER2)
            continue;

        // check if item stat score is the best among class specs
        uint32 bestSpecId = 0;
        uint32 bestSpecScore = 0;
        for (vector<uint32>::iterator i = classspecs.begin(); i != classspecs.end(); ++i)
        {
            if (info->weights[*i] > bestSpecScore)
            {
                bestSpecId = *i;
                bestSpecScore = info->weights[specId];
            }
        }

        if (bestSpecId && bestSpecId != specId && player->getLevel() > 40)
            return 0;

        if (!closestUpgrade)
        {
            closestUpgrade = info->itemId;
            closestUpgradeWeight = info->weights[specId];
        }

        // pick closest upgrade
        if (info->weights[specId] < closestUpgradeWeight)
        {
            closestUpgrade = info->itemId;
            closestUpgradeWeight = info->weights[specId];
        }
    }

    if (closestUpgrade)
        sLog.outString("New Item: %d, weight: %d", closestUpgrade, closestUpgradeWeight);

    return closestUpgrade;
}

vector<uint32> RandomItemMgr::GetUpgradeList(Player* player, std::string spec, uint8 slot, uint32 quality, uint32 itemId, uint32 amount)
{
    vector<uint32> listItems;
    if (!player)
        return listItems;

    // get old item statWeight
    uint32 oldStatWeight = 0;
    uint32 specId = 0;
    uint32 closestUpgrade = 0;
    uint32 closestUpgradeWeight = 0;
    vector<uint32> classspecs;

    for (uint32 specNum = 1; specNum < 5; ++specNum)
    {
        if (!m_weightScales[player->getClass()][specNum].info.id)
            continue;

        classspecs.push_back(m_weightScales[player->getClass()][specNum].info.id);

        if (m_weightScales[player->getClass()][specNum].info.name == spec)
            specId = m_weightScales[player->getClass()][specNum].info.id;
    }
    if (!specId)
        return listItems;

    if (itemId && itemInfoCache[itemId])
    {
        oldStatWeight = itemInfoCache[itemId]->weights[specId];

        if (oldStatWeight)
            sLog.outString("Old Item: %d, weight: %d", itemId, oldStatWeight);
        else
            sLog.outString("Old item has no stat weight");
    }

    for (map<uint32, ItemInfoEntry*>::iterator i = itemInfoCache.begin(); i != itemInfoCache.end(); ++i)
    {
        ItemInfoEntry* info = i->second;
        if (!info)
            continue;

        // skip useless items
        if (info->weights[specId] == 0)
            continue;

        // skip higher lvl
        if (info->minLevel > player->getLevel())
            continue;

        // skip too low level
        if ((int32)info->minLevel < (int32)(player->getLevel() - 20))
            continue;

        // skip wrong team
        if (info->team && info->team != player->GetTeam())
            continue;

        // skip wrong slot
        if ((EquipmentSlots)info->slot != (EquipmentSlots)slot)
            continue;

        // skip higher quality
        if (quality && info->quality != quality)
            continue;

        // skip worse items
        if (info->weights[specId] <= oldStatWeight)
            continue;

        // skip items that only fit in slot, but not stats
        if (!itemId && info->weights[specId] == 1 && player->getLevel() > 40)
            continue;

        // skip quest items
        if (info->source == ITEM_SOURCE_QUEST)
        {
            if (player->GetQuestRewardStatus(info->sourceId) != QUEST_STATUS_COMPLETE)
                continue;
        }

        // skip no stats trinkets
        if (info->weights[specId] < 2 && (
            info->slot == EQUIPMENT_SLOT_NECK ||
            info->slot == EQUIPMENT_SLOT_TRINKET1 ||
            info->slot == EQUIPMENT_SLOT_TRINKET2 ||
            info->slot == EQUIPMENT_SLOT_FINGER1 ||
            info->slot == EQUIPMENT_SLOT_FINGER2))
            continue;

        //if (player->getLevel() >= 40)
        //{
        //    // check if item stat score is the best among class specs
        //    uint32 bestSpecId = 0;
        //    uint32 bestSpecScore = 0;
        //    for (vector<uint32>::iterator i = classspecs.begin(); i != classspecs.end(); ++i)
        //    {
        //        if (info->weights[*i] > bestSpecScore)
        //        {
        //            bestSpecId = *i;
        //            bestSpecScore = info->weights[specId];
        //        }
        //    }

        //    if (bestSpecId && bestSpecId != specId)
        //        continue;
        //}

        listItems.push_back(info->itemId);
        //continue;

        // pick closest upgrade
        if (info->weights[specId] > closestUpgradeWeight)
        {
            closestUpgrade = info->itemId;
            closestUpgradeWeight = info->weights[specId];
        }
    }

    if (listItems.size())
        sLog.outString("New Items: %d, Old item:%d, New items max: %d", listItems.size(), oldStatWeight, closestUpgradeWeight);

    return listItems;
}

bool RandomItemMgr::HasStatWeight(uint32 itemId)
{
    return itemInfoCache[itemId] != nullptr;
}

uint32 RandomItemMgr::GetMinLevelFromCache(uint32 itemId)
{
    ItemInfoEntry* info = itemInfoCache[itemId];
    if (!info)
        return 0;

    return info->minLevel;
}

uint32 RandomItemMgr::GetStatWeight(Player* player, uint32 itemId)
{
    if (!player || !itemId)
        return 0;

    if (!itemInfoCache[itemId])
        return 0;

    uint32 statWeight = 0;
    uint32 specId = 0;
    vector<uint32> classspecs;

    string specName = AiFactory::GetPlayerSpecName(player);
    if (specName.empty())
        return 0;

    for (uint32 specNum = 1; specNum < 5; ++specNum)
    {
        if (!m_weightScales[player->getClass()][specNum].info.id)
            continue;

        classspecs.push_back(m_weightScales[player->getClass()][specNum].info.id);

        if (m_weightScales[player->getClass()][specNum].info.name == specName)
            specId = m_weightScales[player->getClass()][specNum].info.id;
    }
    if (!specId)
        return 0;

    map<uint32, ItemInfoEntry*>::iterator itr = itemInfoCache.find(itemId);
    if (itr != itemInfoCache.end())
    {
        statWeight = itemInfoCache[itemId]->weights[specId];
    }

    return statWeight;
}

uint32 RandomItemMgr::GetLiveStatWeight(Player* player, uint32 itemId)
{
    if (!player || !itemId)
        return 0;

    if (!itemInfoCache[itemId])
        return 0;

    uint32 statWeight = 0;
    uint32 specId = 0;
    vector<uint32> classspecs;

    string specName = AiFactory::GetPlayerSpecName(player);
    if (specName.empty())
        return 0;

    for (uint32 specNum = 1; specNum < 5; ++specNum)
    {
        if (!m_weightScales[player->getClass()][specNum].info.id)
            continue;

        // for bestSpec check
        // classspecs.push_back(m_weightScales[player->getClass()][specNum].info.id);

        if (m_weightScales[player->getClass()][specNum].info.name == specName)
            specId = m_weightScales[player->getClass()][specNum].info.id;
    }
    if (!specId)
        return 0;

    ItemInfoEntry* info = itemInfoCache[itemId];

    if (!info)
        return 0;

    statWeight = info->weights[specId];

    // skip higher lvl
    if (info->minLevel > player->getLevel())
        return 0;

    // skip too low level
    //if ((int32)info->minLevel < (int32)(player->getLevel() - 20))
    //    return 0;

    // skip wrong team
    if (info->team && info->team != player->GetTeam())
        return 0;

    // skip quest items
    if (info->source == ITEM_SOURCE_QUEST && info->sourceId)
    {
        if (player->GetQuestRewardStatus(info->sourceId) != QUEST_STATUS_COMPLETE)
            return 0;
    }

    // skip pvp items
    if (info->source == ITEM_SOURCE_PVP)
    {
#ifndef MANGOSBOT_ZERO
        if (!player->GetHonorPoints() && !player->GetArenaPoints())
            return 0;
#else
        if (!player->GetHonorRankInfo().rank)
            return 0;
#endif
    }

    // skip no stats trinkets
    if (info->weights[specId] == 1 && (
        info->slot == EQUIPMENT_SLOT_NECK ||
        info->slot == EQUIPMENT_SLOT_TRINKET1 ||
        info->slot == EQUIPMENT_SLOT_TRINKET2 ||
        info->slot == EQUIPMENT_SLOT_FINGER1 ||
        info->slot == EQUIPMENT_SLOT_FINGER2))
        return 0;

    // skip items that only fit in slot, but not stats
    if (!itemId && info->weights[specId] == 1 && player->getLevel() > 20)
        return 0;

    // check if item stat score is the best among class specs
    /*uint32 bestSpecId = 0;
    uint32 bestSpecScore = 0;
    for (vector<uint32>::iterator i = classspecs.begin(); i != classspecs.end(); ++i)
    {
        if (itemCache[itemId]->weights[*i] > bestSpecScore && itemCache[itemId]->weights[*i] > 1)
        {
            bestSpecId = *i;
            bestSpecScore = itemCache[itemId]->weights[specId];
        }
    }

    if (bestSpecId && bestSpecId != specId && player->getLevel() >= 60)
        return 0;*/

    return statWeight;
}

void RandomItemMgr::BuildEquipCache()
{
    uint32 maxLevel = sPlayerbotAIConfig.randomBotMaxLevel;
    if (maxLevel > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);

    QueryResult* results = PlayerbotDatabase.PQuery("select clazz, lvl, slot, quality, item from ai_playerbot_equip_cache");
    if (results)
    {
        sLog.outString("Loading equipment cache for %d classes, %d levels, %d slots, %d quality from %d items",
                MAX_CLASSES, maxLevel, EQUIPMENT_SLOT_END, ITEM_QUALITY_ARTIFACT, sItemStorage.GetMaxEntry());
        int count = 0;
        do
        {
            Field* fields = results->Fetch();
            uint32 clazz = fields[0].GetUInt32();
            uint32 level = fields[1].GetUInt32();
            uint32 slot = fields[2].GetUInt32();
            uint32 quality = fields[3].GetUInt32();
            uint32 itemId = fields[4].GetUInt32();

            BotEquipKey key(level, clazz, slot, quality);
            equipCache[key].push_back(itemId);
            count++;

        } while (results->NextRow());
        delete results;
        sLog.outString("Equipment cache loaded from %d records", count);
    }
    else
    {
        uint64 total = MAX_CLASSES * maxLevel * EQUIPMENT_SLOT_END * ITEM_QUALITY_ARTIFACT;
        sLog.outString("Building equipment cache for %d classes, %d levels, %d slots, %d quality from %d items (%zu total)",
                MAX_CLASSES, maxLevel, EQUIPMENT_SLOT_END, ITEM_QUALITY_ARTIFACT, sItemStorage.GetMaxEntry(), total);

        BarGoLink bar(total);
        for (uint8 clazz = CLASS_WARRIOR; clazz < MAX_CLASSES; ++clazz)
        {
            // skip nonexistent classes
            if (!((1 << (clazz - 1)) & CLASSMASK_ALL_PLAYABLE) || !sChrClassesStore.LookupEntry(clazz))
                continue;

            for (uint32 level = 1; level <= maxLevel; ++level)
            {
                for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
                {
                    for (uint32 quality = ITEM_QUALITY_POOR; quality <= ITEM_QUALITY_ARTIFACT; ++quality)
                    {
                        BotEquipKey key(level, clazz, slot, quality);

                        RandomItemList items;
                        for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
                        {
                            ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
                            if (!proto)
                                continue;

                            if (proto->Class != ITEM_CLASS_WEAPON &&
                                proto->Class != ITEM_CLASS_ARMOR /*&&
                                proto->Class != ITEM_CLASS_CONTAINER &&
                                proto->Class != ITEM_CLASS_PROJECTILE*/)
                                continue;

                            if (!CanEquipItem(key, proto))
                                continue;

                            if (proto->Class == ITEM_CLASS_ARMOR && (
                                slot == EQUIPMENT_SLOT_HEAD ||
                                slot == EQUIPMENT_SLOT_NECK ||
                                slot == EQUIPMENT_SLOT_SHOULDERS ||
                                slot == EQUIPMENT_SLOT_BACK ||
                                slot == EQUIPMENT_SLOT_CHEST ||
                                slot == EQUIPMENT_SLOT_WAIST ||
                                slot == EQUIPMENT_SLOT_LEGS ||
                                slot == EQUIPMENT_SLOT_FEET ||
                                slot == EQUIPMENT_SLOT_WRISTS ||
                                slot == EQUIPMENT_SLOT_HANDS ||
                                slot == EQUIPMENT_SLOT_FINGER1 ||
                                slot == EQUIPMENT_SLOT_FINGER2 ||
                                slot == EQUIPMENT_SLOT_TRINKET1 ||
                                slot == EQUIPMENT_SLOT_TRINKET2) && !CanEquipArmor(key.clazz, key.level, proto))
                                    continue;

                            if (proto->Class == ITEM_CLASS_WEAPON && !CanEquipWeapon(key.clazz, proto))
                                continue;

                            if (slot == EQUIPMENT_SLOT_OFFHAND && key.clazz == CLASS_ROGUE && proto->Class != ITEM_CLASS_WEAPON)
                                continue;

                            if (slot == EQUIPMENT_SLOT_OFFHAND && key.clazz == CLASS_PALADIN && proto->SubClass != ITEM_SUBCLASS_ARMOR_BUCKLER && proto->SubClass != ITEM_SUBCLASS_ARMOR_SHIELD)
                                continue;

                            if (slot == EQUIPMENT_SLOT_OFFHAND && key.clazz == CLASS_WARRIOR && proto->SubClass != ITEM_SUBCLASS_ARMOR_BUCKLER && proto->SubClass != ITEM_SUBCLASS_ARMOR_SHIELD && proto->Class != ITEM_CLASS_WEAPON)
                                continue;

                            items.push_back(itemId);

                            PlayerbotDatabase.PExecute("insert into ai_playerbot_equip_cache (clazz, lvl, slot, quality, item) values (%u, %u, %u, %u, %u)",
                                    clazz, level, slot, quality, itemId);
                        }

                        equipCache[key] = items;
                        bar.step();
                        sLog.outDetail("Equipment cache for class: %d, level %d, slot %d, quality %d: %zu items",
                                clazz, level, slot, quality, items.size());
                    }
                }
            }
        }
        sLog.outString("Equipment cache saved to DB");
    }
}

RandomItemList RandomItemMgr::Query(uint32 level, uint8 clazz, uint8 slot, uint32 quality)
{
    BotEquipKey key(level, clazz, slot, quality);
    return equipCache[key];
}

void RandomItemMgr::BuildAmmoCache()
{
    uint32 maxLevel = sPlayerbotAIConfig.randomBotMaxLevel;
    if (maxLevel > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);

    sLog.outBasic("Building ammo cache for %d levels", maxLevel);
	int counter1 = 0;
    for (uint32 level = 1; level <= maxLevel+1; level+=10)
    {
        for (uint32 subClass = ITEM_SUBCLASS_ARROW; subClass <= ITEM_SUBCLASS_BULLET; subClass++)
        {
            QueryResult* results = WorldDatabase.PQuery(
                    "select entry, RequiredLevel from item_template where class = '%u' and subclass = '%u' and RequiredLevel <= '%u' and quality = '%u' order by RequiredLevel desc",
                    ITEM_CLASS_PROJECTILE, subClass, level, ITEM_QUALITY_NORMAL);
            if (!results)
                return;

            Field* fields = results->Fetch();
            if (fields)
            {
                uint32 entry = fields[0].GetUInt32();
                ammoCache[level / 10][subClass] = entry;
				counter1++;
            }

            delete results;
        }
    }
	sLog.outString("Cached %d types of ammo", counter1); // TEST
}

uint32 RandomItemMgr::GetAmmo(uint32 level, uint32 subClass)
{
    return ammoCache[(level - 1) / 10][subClass];
}


void RandomItemMgr::BuildPotionCache()
{
    uint32 maxLevel = sPlayerbotAIConfig.randomBotMaxLevel;
    if (maxLevel > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);

    sLog.outBasic("Building potion cache for %d levels", maxLevel);
	int counter2 = 0;
    for (uint32 level = 1; level <= maxLevel+1; level+=10)
    {
        uint32 effects[] = { SPELL_EFFECT_HEAL, SPELL_EFFECT_ENERGIZE };
        for (int i = 0; i < 2; ++i)
        {
            uint32 effect = effects[i];

            for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
            {
                ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
                if (!proto)
                    continue;

                if (proto->Class != ITEM_CLASS_CONSUMABLE ||
                    (proto->SubClass != ITEM_SUBCLASS_POTION && proto->SubClass != ITEM_SUBCLASS_FLASK) ||
                    proto->Bonding != NO_BIND)
                    continue;

                if (proto->RequiredLevel && (proto->RequiredLevel > level || proto->RequiredLevel < level - 10))
                    continue;

                if (proto->RequiredSkill)
                    continue;

                if (proto->Area || proto->Map || proto->RequiredCityRank || proto->RequiredHonorRank)
                    continue;

                if (proto->Duration & 0x80000000)
                    continue;

                if (sAhBotConfig.ignoreItemIds.find(proto->ItemId) != sAhBotConfig.ignoreItemIds.end())
                    continue;

                for (int j = 0; j < MAX_ITEM_PROTO_SPELLS; j++)
                {
                    const SpellEntry* const spellInfo = sServerFacade.LookupSpellInfo(proto->Spells[j].SpellId);
                    if (!spellInfo)
                        continue;

                    for (int i = 0 ; i < 3; i++)
                    {
                        if (spellInfo->Effect[i] == effect)
                        {
                            potionCache[level / 10][effect].push_back(itemId);
                            break;
                        }
                    }
                }
            }
        }
    }

    for (uint32 level = 1; level <= maxLevel+1; level+=10)
    {
        uint32 effects[] = { SPELL_EFFECT_HEAL, SPELL_EFFECT_ENERGIZE };
        for (int i = 0; i < 2; ++i)
        {
            uint32 effect = effects[i];
            uint32 size = potionCache[level / 10][effect].size();
			counter2++;
            sLog.outDetail("Potion cache for level=%d, effect=%d: %d items", level, effect, size);
        }
    }
	sLog.outString("Cached %d types of potions", counter2); // TEST
}

void RandomItemMgr::BuildFoodCache()
{
    uint32 maxLevel = sPlayerbotAIConfig.randomBotMaxLevel;
    if (maxLevel > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);

    sLog.outBasic("Building food cache for %d levels", maxLevel);
	int counter3 = 0;
    for (uint32 level = 1; level <= maxLevel+1; level+=10)
    {
        uint32 categories[] = { 11, 59 };
        for (int i = 0; i < 2; ++i)
        {
            uint32 category = categories[i];

            for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
            {
                ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
                if (!proto)
                    continue;

                if (proto->Class != ITEM_CLASS_CONSUMABLE ||
                    (proto->SubClass != ITEM_SUBCLASS_FOOD && proto->SubClass != ITEM_SUBCLASS_CONSUMABLE) ||
                    (proto->Spells[0].SpellCategory != category) ||
                    proto->Bonding != NO_BIND)
                    continue;

                if (proto->RequiredLevel && (proto->RequiredLevel > level || proto->RequiredLevel < level - 10))
                    continue;

                if (proto->RequiredSkill)
                    continue;

                if (proto->Area || proto->Map || proto->RequiredCityRank || proto->RequiredHonorRank)
                    continue;

                if (proto->Duration & 0x80000000)
                    continue;

                if (sAhBotConfig.ignoreItemIds.find(proto->ItemId) != sAhBotConfig.ignoreItemIds.end())
                    continue;

                foodCache[level / 10][category].push_back(itemId);
            }
        }
    }

    for (uint32 level = 1; level <= maxLevel+1; level+=10)
    {
        uint32 categories[] = { 11, 59 };
        for (int i = 0; i < 2; ++i)
        {
            uint32 category = categories[i];
            uint32 size = foodCache[level / 10][category].size();
			counter3++;
            sLog.outDetail("Food cache for level=%d, category=%d: %d items", level, category, size);
        }
    }
	sLog.outString("Cached %d types of food", counter3);
}

uint32 RandomItemMgr::GetRandomPotion(uint32 level, uint32 effect)
{
    vector<uint32> potions = potionCache[(level - 1) / 10][effect];
    if (potions.empty()) return 0;
    return potions[urand(0, potions.size() - 1)];
}

uint32 RandomItemMgr::GetFood(uint32 level, uint32 category)
{
    initializer_list<uint32> items;
    vector<uint32> food;
    if (category == 11)
    {
        if (level < 5)
            items = { 787, 117, 4540, 2680 };
        else if (level < 15)
            items = { 2287, 4592, 4541, 21072 };
        else if (level < 25)
            items = { 3770, 16170, 4542, 20074 };
        else if (level < 35)
            items = { 4594, 3771, 1707, 4457 };
        else if (level < 45)
            items = { 4599, 4601, 21552, 17222 /*21030, 16168 */ };
#ifdef MANGOSBOT_ZERO
        else
#else
        else if (level < 55)
#endif
            items = { 8950, 8952, 8957, 21023 /*21033, 21031 */ };
#ifdef MANGOSBOT_ONE
        else if (level < 65)
            items = { 29292, 27859, 30458, 27662 };
        else
            items = { 29450, 29451, 29452 };
#endif
#ifdef MANGOSBOT_TWO
    else if (level < 65)
        items = { 29292, 27859, 30458, 27662 };
    else if (level < 75)
        items = { 29450, 29451, 29452 };
    else
        items = { 35947 };
#endif
    }

    if (category == 59)
    {
        if (level < 5)
            items = { 159, 117 };
        else if (level < 15)
            items = { 1179, 21072 };
        else if (level < 25)
            items = { 1205 };
        else if (level < 35)
            items = { 1708 };
        else if (level < 45)
            items = { 1645 };
#ifdef MANGOSBOT_ZERO
        else
#else
        else if (level < 55)
#endif
            items = { 8766 };
#ifdef MANGOSBOT_ONE
        else if (level < 65)
            items = { 28399 };
        else
            items = { 27860 };
#endif
#ifdef MANGOSBOT_TWO
    else if (level < 65)
        items = { 28399 };
    else if (level < 75)
        items = { 27860 };
    else
        items = { 33445 };
#endif
    }

    food.insert(food.end(), items);
    if (food.empty()) return 0;
    return food[urand(0, food.size() - 1)];
}

uint32 RandomItemMgr::GetRandomFood(uint32 level, uint32 category)
{
    vector<uint32> food = foodCache[(level - 1) / 10][category];
    if (food.empty()) return 0;
    return food[urand(0, food.size() - 1)];
}

void RandomItemMgr::BuildTradeCache()
{
    uint32 maxLevel = sPlayerbotAIConfig.randomBotMaxLevel;
    if (maxLevel > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
        maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);

    sLog.outBasic("Building trade cache for %d levels", maxLevel);
	int counter4 = 0;
    for (uint32 level = 1; level <= maxLevel+1; level+=10)
    {
        for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
        {
            ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
            if (!proto)
                continue;

            if (proto->Class != ITEM_CLASS_TRADE_GOODS || proto->Bonding != NO_BIND)
                continue;

            if (proto->ItemLevel < level)
                continue;

            if (proto->RequiredLevel && (proto->RequiredLevel > level || proto->RequiredLevel < level - 10))
                continue;

            if (proto->RequiredSkill)
                continue;

            tradeCache[level / 10].push_back(itemId);
        }
    }

    for (uint32 level = 1; level <= maxLevel+1; level+=10)
    {
        uint32 size = tradeCache[level / 10].size();
        sLog.outDetail("Trade cache for level=%d: %d items", level, size);
		counter4++;
    }
	sLog.outString("Cached %d trade categories", counter4); // TEST
}

uint32 RandomItemMgr::GetRandomTrade(uint32 level)
{
    vector<uint32> trade = tradeCache[(level - 1) / 10];
    if (trade.empty()) return 0;
    return trade[urand(0, trade.size() - 1)];
}

void RandomItemMgr::BuildRarityCache()
{
    QueryResult* results = PlayerbotDatabase.PQuery("select item, rarity from ai_playerbot_rarity_cache");
    if (results)
    {
        sLog.outBasic("Loading item rarity cache");
        int count = 0;
        do
        {
            Field* fields = results->Fetch();
            uint32 itemId = fields[0].GetUInt32();
            float rarity = fields[1].GetFloat();

            rarityCache[itemId] = rarity;
            count++;

        } while (results->NextRow());
        delete results;
        sLog.outString("Item rarity cache loaded from %d records", count);
    }
    else
    {
        sLog.outBasic("Building item rarity cache from %u items", sItemStorage.GetMaxEntry());
        BarGoLink bar(sItemStorage.GetMaxEntry());
        for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
        {
            bar.step();
            ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
            if (!proto)
                continue;

            if (proto->Duration & 0x80000000)
                continue;

            if (proto->Quality == ITEM_QUALITY_POOR)
                continue;

            if (sAhBotConfig.ignoreItemIds.find(proto->ItemId) != sAhBotConfig.ignoreItemIds.end())
                continue;

            if (strstri(proto->Name1, "qa") || strstri(proto->Name1, "test") || strstri(proto->Name1, "deprecated"))
                continue;

            if (!proto->ItemLevel/* || proto->RequiredLevel > sAhBotConfig.maxRequiredLevel || proto->ItemLevel > sAhBotConfig.maxItemLevel*/)
                continue;
            QueryResult* results = WorldDatabase.PQuery(
                    "select max(q.chance) from ( "
                    // "-- Creature "
                    "select  "
                    "avg ( "
                    "   case  "
                    "    when lt.groupid = 0 then lt.ChanceOrQuestChance  "
                    "    when lt.ChanceOrQuestChance > 0 then lt.ChanceOrQuestChance "
                    "    else   "
                    "    ifnull(100 - (select sum(ChanceOrQuestChance) from creature_loot_template lt1 where lt1.groupid = lt.groupid and lt1.entry = lt.entry and lt1.ChanceOrQuestChance > 0), 100) "
                    "    / (select count(*) from creature_loot_template lt1 where lt1.groupid = lt.groupid and lt1.entry = lt.entry and lt1.ChanceOrQuestChance = 0) "
                    "    end "
                    ") chance, 'creature' type "
                    "from creature_loot_template lt "
                    "join creature_template ct on ct.LootId = lt.entry "
                    "join creature c on c.id = ct.entry "
                    "where lt.item = '%u' "
                    "union all "
                    // "-- Gameobject "
                    "select  "
                    "avg ( "
                    "   case  "
                    "    when lt.groupid = 0 then lt.ChanceOrQuestChance  "
                    "    when lt.ChanceOrQuestChance > 0 then lt.ChanceOrQuestChance "
                    "    else   "
                    "    ifnull(100 - (select sum(ChanceOrQuestChance) from gameobject_loot_template lt1 where lt1.groupid = lt.groupid and lt1.entry = lt.entry and lt1.ChanceOrQuestChance > 0), 100) "
                    "    / (select count(*) from gameobject_loot_template lt1 where lt1.groupid = lt.groupid and lt1.entry = lt.entry and lt1.ChanceOrQuestChance = 0) "
                    "    end "
                    ") chance, 'gameobject' type "
                    "from gameobject_loot_template lt "
                    "join gameobject_template ct on ct.data1 = lt.entry "
                    "join gameobject c on c.id = ct.entry "
                    "where lt.item = '%u' "
                    "union all "
                    // "-- Disenchant "
                    "select  "
                    "avg ( "
                    "   case  "
                    "    when lt.groupid = 0 then lt.ChanceOrQuestChance  "
                    "    when lt.ChanceOrQuestChance > 0 then lt.ChanceOrQuestChance "
                    "    else   "
                    "    ifnull(100 - (select sum(ChanceOrQuestChance) from disenchant_loot_template lt1 where lt1.groupid = lt.groupid and lt1.entry = lt.entry and lt1.ChanceOrQuestChance > 0), 100) "
                    "    / (select count(*) from disenchant_loot_template lt1 where lt1.groupid = lt.groupid and lt1.entry = lt.entry and lt1.ChanceOrQuestChance = 0) "
                    "    end "
                    ") chance, 'disenchant' type "
                    "from disenchant_loot_template lt "
                    "join item_template ct on ct.DisenchantID = lt.entry "
                    "where lt.item = '%u' "
                    "union all "
                    // "-- Fishing "
                    "select  "
                    "avg ( "
                    "   case  "
                    "    when lt.groupid = 0 then lt.ChanceOrQuestChance  "
                    "    when lt.ChanceOrQuestChance > 0 then lt.ChanceOrQuestChance "
                    "    else   "
                    "    ifnull(100 - (select sum(ChanceOrQuestChance) from fishing_loot_template lt1 where lt1.groupid = lt.groupid and lt1.entry = lt.entry and lt1.ChanceOrQuestChance > 0), 100) "
                    "    / (select count(*) from fishing_loot_template lt1 where lt1.groupid = lt.groupid and lt1.entry = lt.entry and lt1.ChanceOrQuestChance = 0) "
                    "    end "
                    ") chance, 'fishing' type "
                    "from fishing_loot_template lt "
                    "where lt.item = '%u' "
                    "union all "
                    // "-- Skinning "
                    "select  "
                    "avg ( "
                    "   case  "
                    "    when lt.groupid = 0 then lt.ChanceOrQuestChance  "
                    "    when lt.ChanceOrQuestChance > 0 then lt.ChanceOrQuestChance  "
                    "    else   "
                    "    ifnull(100 - (select sum(ChanceOrQuestChance) from skinning_loot_template lt1 where lt1.groupid = lt.groupid and lt1.entry = lt.entry and lt1.ChanceOrQuestChance > 0), 100) "
                    "    * ifnull((select 1/count(*) from skinning_loot_template lt1 where lt1.groupid = lt.groupid and lt1.entry = lt.entry and lt1.ChanceOrQuestChance = 0), 1) "
                    "    end "
                    ") chance, 'skinning' type "
                    "from skinning_loot_template lt "
                    "join creature_template ct on ct.SkinningLootId = lt.entry "
                    "join creature c on c.id = ct.entry "
                    "where lt.item = '%u' "
                    ") q; ",
                             itemId,itemId,itemId,itemId,itemId);

            if (results)
            {
                Field* fields = results->Fetch();
                float rarity = fields[0].GetFloat();
                if (rarity > 0.01)
                {
                    rarityCache[itemId] = rarity;

                    PlayerbotDatabase.PExecute("insert into ai_playerbot_rarity_cache (item, rarity) values (%u, %f)",
                            itemId, rarity);
                }
            }
        }
        sLog.outString("Item rarity cache built from %u items", sItemStorage.GetMaxEntry());
    }
}

float RandomItemMgr::GetItemRarity(uint32 itemId)
{
    return rarityCache[itemId];
}
