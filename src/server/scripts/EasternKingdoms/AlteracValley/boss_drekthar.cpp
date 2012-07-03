/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptPCH.h"

enum Spells
{
    SPELL_WHIRLWIND                               = 13736,
    SPELL_KNOCKDOWN                               = 19128,
    SPELL_FRENZY                                  = 8269,
    SPELL_SWEEPING_STRIKES                        = 18765, // not sure
    SPELL_CLEAVE                                  = 20677, // not sure
    SPELL_COMPLETE_ALTERAC_VALLEY_QUEST           = 23658,
};

enum Yells
{
    YELL_AGGRO                                    = 0,
    YELL_EVADE                                    = 1,
    YELL_RESPAWN                                  = 2,
    YELL_RANDOM                                   = 3,
};

enum Quests
{
    QUEST_THE_BATTLE_OF_ALTERAC_H   = 7142,
    QUEST_THE_BATTLE_OF_ALTERAC_A   = 7141,
};

class boss_drekthar : public CreatureScript
{
public:
    boss_drekthar() : CreatureScript("boss_drekthar") { }

    struct boss_drektharAI : public ScriptedAI
    {
        boss_drektharAI(Creature* creature) : ScriptedAI(creature) {}

        uint32 WhirlwindTimer;
        uint32 KnockdownTimer;
        uint32 FrenzyTimer;
        uint32 YellTimer;
        uint32 ResetTimer;

        void Reset()
        {
            WhirlwindTimer    = urand(1000, 20000);
            KnockdownTimer    = 12000;
            FrenzyTimer       = 6000;
            ResetTimer        = 5000;
            YellTimer         = urand(20000, 30000); //20 to 30 seconds
        }

        void EnterCombat(Unit* /*who*/)
        {
            Talk(YELL_AGGRO);
        }

        void JustRespawned()
        {
            Reset();
            Talk(YELL_RESPAWN);
        }

        void JustDied(Unit* killer)
        {
            if (!killer->ToPlayer() || (killer->ToPlayer() && !killer->ToPlayer()->InBattleground()) || (killer->ToPlayer() && (killer->ToPlayer()->GetQuestStatus(QUEST_THE_BATTLE_OF_ALTERAC_H) != QUEST_STATUS_INCOMPLETE || killer->ToPlayer()->GetQuestStatus(QUEST_THE_BATTLE_OF_ALTERAC_A) != QUEST_STATUS_INCOMPLETE)))
                return;

            if (Battleground* bg = killer->ToPlayer()->GetBattleground())
                bg->CastSpellOnTeam(SPELL_COMPLETE_ALTERAC_VALLEY_QUEST, killer->ToPlayer()->GetTeam());
        }

        void UpdateAI(const uint32 diff)
        {
            if (!UpdateVictim())
                return;

            if (WhirlwindTimer <= diff)
            {
                DoCast(me->getVictim(), SPELL_WHIRLWIND);
                WhirlwindTimer = urand(7000, 25000);
            } else WhirlwindTimer -= diff;

            if (KnockdownTimer <= diff)
            {
                DoCast(me->getVictim(), SPELL_KNOCKDOWN);
                KnockdownTimer = urand(10000, 15000);
            } else KnockdownTimer -= diff;

            if (FrenzyTimer <= diff)
            {
                DoCast(me->getVictim(), SPELL_FRENZY);
                FrenzyTimer = urand(20000, 30000);
            } else FrenzyTimer -= diff;

            if (YellTimer <= diff)
            {
                Talk(YELL_RANDOM);
                YellTimer = urand(20000, 30000); //20 to 30 seconds
            } else YellTimer -= diff;

            // check if creature is not outside of building
            if (ResetTimer <= diff)
            {
                if (me->GetDistance2d(me->GetHomePosition().GetPositionX(), me->GetHomePosition().GetPositionY()) > 50)
                {
                    EnterEvadeMode();
                    Talk(YELL_EVADE);
                }
                ResetTimer = 5000;
            } else ResetTimer -= diff;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new boss_drektharAI(creature);
    }
};

void AddSC_boss_drekthar()
{
    new boss_drekthar;
}
