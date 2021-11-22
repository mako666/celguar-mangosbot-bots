#include "../botpch.h"
#include "PlayerbotMgr.h"
#include "playerbot.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotAI.h"
#include "ChatHelper.h"
#include "ServerFacade.h"

PlayerbotSecurity::PlayerbotSecurity(Player* const bot) : bot(bot)
{
    if (bot)
        account = sObjectMgr.GetPlayerAccountIdByGUID(bot->GetObjectGuid());
}

PlayerbotSecurityLevel PlayerbotSecurity::LevelFor(Player* from, DenyReason* reason, bool ignoreGroup)
{
    if (from->GetSession()->GetSecurity() >= SEC_GAMEMASTER)
        return PLAYERBOT_SECURITY_ALLOW_ALL;

    if (bot->GetPlayerbotAI()->IsOpposing(from))
    {
        if (reason) *reason = PLAYERBOT_DENY_OPPOSING;
        return PLAYERBOT_SECURITY_DENY_ALL;
    }

    if (sPlayerbotAIConfig.IsInRandomAccountList(account))
    {
        if (bot->GetPlayerbotAI()->IsOpposing(from))
        {
            if (reason) *reason = PLAYERBOT_DENY_OPPOSING;
            return PLAYERBOT_SECURITY_DENY_ALL;
        }

        Group* group = from->GetGroup();
        if (group)
        {
            for (GroupReference *gref = group->GetFirstMember(); gref; gref = gref->next())
            {
                Player* player = gref->getSource();
                if (player == bot && !ignoreGroup)
                    return PLAYERBOT_SECURITY_ALLOW_ALL;
            }
        }

        if ((int)bot->getLevel() - (int)from->getLevel() > 30)
        {
            if (!bot->GetGuildId() || bot->GetGuildId() != from->GetGuildId())
            {
                if (reason) *reason = PLAYERBOT_DENY_LOW_LEVEL;
                return PLAYERBOT_SECURITY_TALK;
            }
        }

        int botGS = (int)bot->GetPlayerbotAI()->GetEquipGearScore(bot, false, false);
        int fromGS = (int)bot->GetPlayerbotAI()->GetEquipGearScore(from, false, false);
        if (sPlayerbotAIConfig.gearscorecheck)
        {
            if (botGS && bot->getLevel() > 15 && botGS > fromGS && (100 * (botGS - fromGS) / botGS) >= 12 * sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL) / from->getLevel())
            {
                if (reason) *reason = PLAYERBOT_DENY_GEARSCORE;
                return PLAYERBOT_SECURITY_TALK;
            }
        }

        if (bot->InBattleGroundQueue())
        {
            if (!bot->GetGuildId() || bot->GetGuildId() != from->GetGuildId())
            {
                if (reason) *reason = PLAYERBOT_DENY_BG;
                return PLAYERBOT_SECURITY_TALK;
            }
        }

#ifdef MANGOSBOT_ONE
        if (bot->GetPlayerbotAI()->HasRealPlayerMaster() && !bot->m_lookingForGroup.Empty() &&
            (!bot->m_lookingForGroup.slots[0].Empty() && bot->m_lookingForGroup.slots[0].type == LFG_TYPE_DUNGEON ||
            (!bot->m_lookingForGroup.slots[1].Empty() && bot->m_lookingForGroup.slots[1].type == LFG_TYPE_DUNGEON) ||
            (!bot->m_lookingForGroup.slots[2].Empty() && bot->m_lookingForGroup.slots[2].type == LFG_TYPE_DUNGEON) ||
                (!bot->m_lookingForGroup.more.Empty() && bot->m_lookingForGroup.more.type == LFG_TYPE_DUNGEON)))
#endif
#ifdef MANGOSBOT_ZERO
        if (sLFGMgr.IsPlayerInQueue(bot->GetObjectGuid()))
#endif
#ifdef MANGOSBOT_TWO
        if (sLFGMgr.GetQueueInfo(bot->GetObjectGuid()))
#endif
        {
            if (!bot->GetGuildId() || bot->GetGuildId() != from->GetGuildId())
            {
                if (reason) *reason = PLAYERBOT_DENY_LFG;
                return PLAYERBOT_SECURITY_TALK;
            }
        }

        /*if (sServerFacade.UnitIsDead(bot))
        {
            if (reason) *reason = PLAYERBOT_DENY_DEAD;
            return PLAYERBOT_SECURITY_TALK;
        }*/

        group = bot->GetGroup();
        if (!group)
        {
            /*if (bot->GetMapId() != from->GetMapId() || bot->GetDistance(from) > sPlayerbotAIConfig.whisperDistance)
            {
                if (!bot->GetGuildId() || bot->GetGuildId() != from->GetGuildId())
                {
                    if (reason) *reason = PLAYERBOT_DENY_FAR;
                    return PLAYERBOT_SECURITY_TALK;
                }
            }*/

            if (reason) *reason = PLAYERBOT_DENY_INVITE;
            return PLAYERBOT_SECURITY_INVITE;
        }

        for (GroupReference *gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* player = gref->getSource();
            if (player == from)
                return PLAYERBOT_SECURITY_ALLOW_ALL;
        }

        if (group->IsFull())
        {
            if (reason) *reason = PLAYERBOT_DENY_FULL_GROUP;
            return PLAYERBOT_SECURITY_TALK;
        }

        if (group->GetLeaderGuid() != bot->GetObjectGuid())
        {
            if (reason) *reason = PLAYERBOT_DENY_NOT_LEADER;
            return PLAYERBOT_SECURITY_TALK;
        }
        else
        {
            if (reason) *reason = PLAYERBOT_DENY_IS_LEADER;
            return PLAYERBOT_SECURITY_INVITE;
        }

        if (reason) *reason = PLAYERBOT_DENY_INVITE;
        return PLAYERBOT_SECURITY_INVITE;
    }

    return PLAYERBOT_SECURITY_ALLOW_ALL;
}

bool PlayerbotSecurity::CheckLevelFor(PlayerbotSecurityLevel level, bool silent, Player* from, bool ignoreGroup)
{
    DenyReason reason = PLAYERBOT_DENY_NONE;
    PlayerbotSecurityLevel realLevel = LevelFor(from, &reason, ignoreGroup);
    if (realLevel >= level)
        return true;

    if (silent || (from->GetPlayerbotAI() && !from->GetPlayerbotAI()->IsRealPlayer()))
        return false;

    Player* master = bot->GetPlayerbotAI()->GetMaster();
    if (master && bot->GetPlayerbotAI() && bot->GetPlayerbotAI()->IsOpposing(master) && master->GetSession()->GetSecurity() < SEC_GAMEMASTER)
        return false;

    ostringstream out;
    switch (realLevel)
    {
    case PLAYERBOT_SECURITY_DENY_ALL:
        out << "I'm kind of busy now";
        break;
    case PLAYERBOT_SECURITY_TALK:
        switch (reason)
        {
        case PLAYERBOT_DENY_NONE:
            out << "I'll do it later";
            break;
        case PLAYERBOT_DENY_LOW_LEVEL:
            out << "You are too low level: |cffff0000" << (uint32)from->getLevel() << "|cffffffff/|cff00ff00" << (uint32)bot->getLevel();
            break;
        case PLAYERBOT_DENY_GEARSCORE:
            {
                int botGS = (int)bot->GetPlayerbotAI()->GetEquipGearScore(bot, false, false);
                int fromGS = (int)bot->GetPlayerbotAI()->GetEquipGearScore(from, false, false);
                int diff = (100 * (botGS - fromGS) / botGS);
                int req = 12 * sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL) / from->getLevel();
                out << "Your gearscore is too low: |cffff0000" << fromGS << "|cffffffff/|cff00ff00" << botGS << " |cffff0000" << diff << "%|cffffffff/|cff00ff00" << req << "%";
            }
            break;
        case PLAYERBOT_DENY_NOT_YOURS:
            out << "I have a master already";
            break;
        case PLAYERBOT_DENY_IS_BOT:
            out << "You are a bot";
            break;
        case PLAYERBOT_DENY_OPPOSING:
            out << "You are the enemy";
            break;
        case PLAYERBOT_DENY_DEAD:
            out << "I'm dead. Will do it later";
            break;
        case PLAYERBOT_DENY_INVITE:
            out << "Invite me to your group first";
            break;
        case PLAYERBOT_DENY_FAR:
            {
                out << "You must be closer to invite me to your group. I am in ";

                uint32 area = bot->GetAreaId();
                if (area)
                {
					const AreaTableEntry* entry = GetAreaEntryByAreaID(area);
                    if (entry)
                    {
                        out << " |cffffffff(|cffff0000" << entry->area_name[0] << "|cffffffff)";
                    }
                }
            }
            break;
        case PLAYERBOT_DENY_FULL_GROUP:
            out << "I am in a full group. Will do it later";
            break;
        case PLAYERBOT_DENY_IS_LEADER:
            out << "I am currently leading a group. I can invite you if you want.";
            break;
        case PLAYERBOT_DENY_NOT_LEADER:
            out << "I am in a group with " << bot->GetPlayerbotAI()->GetGroupMaster()->GetName() << ". You can ask him for invite.";
            break;
        case PLAYERBOT_DENY_BG:
            out << "I am in a queue for BG. Will do it later";
            break;
        case PLAYERBOT_DENY_LFG:
            out << "I am in a queue for dungeon. Will do it later";
            break;
        default:
            out << "I can't do that";
            break;
        }
        break;
    case PLAYERBOT_SECURITY_INVITE:
        out << "Invite me to your group first";
        break;
        default:
            out << "I can't do that";
            break;
    }

    string text = out.str();
    uint64 guid = from->GetObjectGuid().GetRawValue();
    time_t lastSaid = whispers[guid][text];
    if (!lastSaid || (time(0) - lastSaid) >= sPlayerbotAIConfig.repeatDelay / 1000)
    {
        whispers[guid][text] = time(0);
        bot->Whisper(text, LANG_UNIVERSAL, ObjectGuid(guid));
    }
    return false;
}
