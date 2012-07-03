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

enum Yells
{
    YELL_AGGRO                                    = 0,
    YELL_EVADE                                    = 1,
    YELL_RESPAWN1                                 = -1810010, // no creature_text
    YELL_RESPAWN2                                 = -1810011, // no creature_text
    YELL_RANDOM                                   = 2,
    YELL_SPELL                                    = 3,
};

enum Spells
{
    SPELL_AVATAR                                  = 19135,
    SPELL_THUNDERCLAP                             = 15588,
    SPELL_STORMBOLT                               = 20685, // not sure

    SPELL_COMPLETE_ALTERAC_VALLEY_QUEST           = 23658,
};

enum Quests
{
    QUEST_THE_BATTLE_OF_ALTERAC_H   = 7142,
    QUEST_THE_BATTLE_OF_ALTERAC_A   = 7141,
};

class boss_vanndar : public CreatureScript
{
public:
    boss_vanndar() : CreatureScript("boss_vanndar") { }

    struct boss_vanndarAI : public ScriptedAI
    {
        boss_vanndarAI(Creature* creature) : ScriptedAI(creature) {}

        uint32 AvatarTimer;
        uint32 ThunderclapTimer;
        uint32 StormboltTimer;
        uint32 ResetTimer;
        uint32 YellTimer;

        void Reset()
        {
            AvatarTimer        = 3000;
            ThunderclapTimer   = 4000;
            StormboltTimer     = 6000;
            ResetTimer         = 5000;
            YellTimer = urand(20000, 30000);
        }

        void EnterCombat(Unit* /*who*/)
        {
            Talk(YELL_AGGRO);
        }

        void JustRespawned()
        {
            Reset();
            DoScriptText(RAND(YELL_RESPAWN1, YELL_RESPAWN2), me);
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

            if (AvatarTimer <= diff)
            {
                DoCast(me->getVictim(), SPELL_AVATAR);
                AvatarTimer =  urand(15000, 20000);
            } else AvatarTimer -= diff;

            if (ThunderclapTimer <= diff)
            {
                DoCast(me->getVictim(), SPELL_THUNDERCLAP);
                ThunderclapTimer = urand(5000, 15000);
            } else ThunderclapTimer -= diff;

            if (StormboltTimer <= diff)
            {
                DoCast(me->getVictim(), SPELL_STORMBOLT);
                StormboltTimer = urand(10000, 25000);
            } else StormboltTimer -= diff;

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
        return new boss_vanndarAI(creature);
    }
};

void AddSC_boss_vanndar()
{
    new boss_vanndar;
}
