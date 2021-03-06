#pragma once

#include "../Strategy.h"
#include "../generic/CombatStrategy.h"

namespace ai
{
    class GenericPriestStrategy : public CombatStrategy
    {
    public:
        GenericPriestStrategy(PlayerbotAI* ai);

    public:
        virtual void InitTriggers(std::list<TriggerNode*> &triggers);

    };

    class PriestCureStrategy : public Strategy
    {
    public:
        PriestCureStrategy(PlayerbotAI* ai);

    public:
        virtual void InitTriggers(std::list<TriggerNode*> &triggers);
        virtual string getName() { return "cure"; }
    };

    class PriestBoostStrategy : public Strategy
    {
    public:
        PriestBoostStrategy(PlayerbotAI* ai) : Strategy(ai) {}

    public:
        virtual void InitTriggers(std::list<TriggerNode*> &triggers);
        virtual string getName() { return "boost"; }
    };

    class PriestCcStrategy : public Strategy
    {
    public:
        PriestCcStrategy(PlayerbotAI* ai) : Strategy(ai) {}

    public:
        virtual void InitTriggers(std::list<TriggerNode*> &triggers);
        virtual string getName() { return "cc"; }
    };
}
