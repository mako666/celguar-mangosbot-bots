#pragma once
#include "../Value.h"
#include "TargetValue.h"
#include "NearestUnitsValue.h"

namespace ai
{
    class AttackersValue : public ObjectGuidListCalculatedValue
	{
	public:
        AttackersValue(PlayerbotAI* ai) : ObjectGuidListCalculatedValue(ai, "attackers", 2) {}
        list<ObjectGuid> Calculate();

	private:
        void AddAttackersOf(Group* group, set<Unit*>& targets);
        void AddAttackersOf(Player* player, set<Unit*>& targets);
		void RemoveNonThreating(set<Unit*>& targets);

    public:
        static bool IsPossibleTarget(Unit* attacker, Player *bot, float range = sPlayerbotAIConfig.sightDistance);
        static bool IsValidTarget(Unit* attacker, Player *bot);
    };

    class PossibleAddsValue : public BoolCalculatedValue
    {
    public:
        PossibleAddsValue(PlayerbotAI* const ai, string name = "possible adds") : BoolCalculatedValue(ai, name, 2) {}
        virtual bool Calculate();
    };
}
