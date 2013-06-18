/*
 * Copyright (C) 2008-2011 TrinityCore <http://www.trinitycore.org/>
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

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "ulduar.h"

enum Spells
{
    // Any boss
    SPELL_SUPERCHARGE                   = 61920,
    SPELL_BERSERK                       = 47008, // Hard enrage, don't know the correct ID.
    SPELL_CREDIT_MARKER                 = 65195, // spell_dbc
    SPELL_IRON_BOOT_FLASK               = 58501,

    // Steelbreaker
    SPELL_HIGH_VOLTAGE                  = 61890,
    SPELL_HIGH_VOLTAGE_H                = 63498,
    SPELL_FUSION_PUNCH                  = 61903,
    SPELL_FUSION_PUNCH_H                = 63493,
    SPELL_STATIC_DISRUPTION             = 61911,
    SPELL_STATIC_DISRUPTION_H           = 63495,
    SPELL_OVERWHELMING_POWER_H          = 61888,
    SPELL_OVERWHELMING_POWER            = 64637,
    SPELL_MELTDOWN                      = 61889,
    SPELL_ELECTRICAL_CHARGE             = 61900,
    SPELL_ELECTRICAL_CHARGE_TRIGGER     = 61901,
    SPELL_ELECTRICAL_CHARGE_TRIGGERED   = 61902,

    // Runemaster Molgeim
    SPELL_SHIELD_OF_RUNES               = 62274,
    SPELL_SHIELD_OF_RUNES_BUFF          = 62277,
    SPELL_SHIELD_OF_RUNES_H             = 63489,
    SPELL_SHIELD_OF_RUNES_H_BUFF        = 63967,
    SPELL_SUMMON_RUNE_OF_POWER          = 63513,
    SPELL_RUNE_OF_POWER                 = 61974,
    SPELL_RUNE_OF_DEATH                 = 62269,
    SPELL_RUNE_OF_SUMMONING             = 62273, // This is the spell that summons the rune
    SPELL_RUNE_OF_SUMMONING_VIS         = 62019, // Visual
    SPELL_RUNE_OF_SUMMONING_SUMMON      = 62020, // Spell that summons
    SPELL_LIGHTNING_BLAST_10            = 62054,
    SPELL_LIGHTNING_BLAST_25            = 63491,

    // Stormcaller Brundir
    SPELL_CHAIN_LIGHTNING_N             = 61879,
    SPELL_CHAIN_LIGHTNING_H             = 63479,
    SPELL_OVERLOAD                      = 61869,
    SPELL_OVERLOAD_H                    = 63481,
    SPELL_LIGHTNING_WHIRL               = 61915,
    SPELL_LIGHTNING_WHIRL_H             = 63483,
    SPELL_LIGHTNING_TENDRILS            = 61887,
    SPELL_LIGHTNING_TENDRILS_H          = 63486,
    SPELL_STORMSHIELD                   = 64187
};

enum Events
{
    EVENT_UPDATEPHASE = 1,
    EVENT_ENRAGE,
    // Steelbreaker
    EVENT_FUSION_PUNCH,
    EVENT_STATIC_DISRUPTION,
    EVENT_OVERWHELMING_POWER,
    // Molgeim
    EVENT_RUNE_OF_POWER,
    EVENT_SHIELD_OF_RUNES,
    EVENT_RUNE_OF_DEATH,
    EVENT_RUNE_OF_SUMMONING,
    EVENT_LIGHTNING_BLAST,
    // Brundir
    EVENT_CHAIN_LIGHTNING,
    EVENT_OVERLOAD,
    EVENT_LIGHTNING_WHIRL,
    EVENT_LIGHTNING_TENDRILS_START,
    EVENT_LIGHTNING_TENDRILS_END,
    EVENT_THREAT_WIPE,
    EVENT_STORMSHIELD,
};

enum Yells
{
    SAY_STEELBREAKER_AGGRO                      = 0,
    SAY_STEELBREAKER_SLAY_1                     = 1,
    SAY_STEELBREAKER_SLAY_2                     = 2,
    SAY_STEELBREAKER_POWER                      = 3,
    SAY_STEELBREAKER_DEATH_1                    = 4,
    SAY_STEELBREAKER_DEATH_2                    = 5,
    SAY_STEELBREAKER_BERSERK                    = 6,

    SAY_MOLGEIM_AGGRO                           = 7,
    SAY_MOLGEIM_SLAY_1                          = 8,
    SAY_MOLGEIM_SLAY_2                          = 9,
    SAY_MOLGEIM_RUNE_DEATH                      = 10,
    SAY_MOLGEIM_SUMMON                          = 11,
    SAY_MOLGEIM_DEATH_1                         = 12,
    SAY_MOLGEIM_DEATH_2                         = 13,
    SAY_MOLGEIM_BERSERK                         = 14,

    SAY_BRUNDIR_AGGRO                           = 15,
    SAY_BRUNDIR_SLAY_1                          = 16,
    SAY_BRUNDIR_SLAY_2                          = 17,
    SAY_BRUNDIR_SPECIAL                         = 18,
    SAY_BRUNDIR_FLIGHT                          = 19,
    SAY_BRUNDIR_DEATH_1                         = 20,
    SAY_BRUNDIR_DEATH_2                         = 21,
    SAY_BRUNDIR_BERSERK                         = 22
};

enum MovePoints
{
    POINT_FLY = 1,
    POINT_LAND,
    POINT_CHASE
};

enum Data
{
    DATA_I_CHOOSE_YOU = 1
};

#define EMOTE_OVERLOAD "Stormcaller Brundir begins to Overload!"

bool IsEncounterComplete(InstanceScript* instance, Creature* me)
{
   if (!instance || !me)
        return false;

    for (uint8 i = 0; i < 3; ++i)
    {
        uint64 guid = instance->GetData64(BOSS_STEELBREAKER + i);
        if (!guid)
            return false;

        if (Creature* boss = Unit::GetCreature(*me, guid))
        {
            if (boss->IsAlive())
                return false;
        }
        else
            return false;
    }
    return true;
}

void RespawnEncounter(InstanceScript* instance, Creature* me)
{
    for (uint8 i = 0; i < 3; ++i)
    {
        uint64 guid = instance->GetData64(BOSS_STEELBREAKER + i);
        if (!guid)
            continue;

        if (Creature* boss = Unit::GetCreature(*me, guid))
        {
            if (!boss->IsAlive())
            {
                boss->Respawn();
                boss->GetMotionMaster()->MoveTargetedHome();
            }
        }
    }
}

void StartEncounter(InstanceScript* instance, Creature* me, Unit* /*target*/)
{
    if (instance->GetBossState(BOSS_ASSEMBLY_OF_IRON) == IN_PROGRESS)
        return; // Prevent recursive calls

    instance->SetBossState(BOSS_ASSEMBLY_OF_IRON, IN_PROGRESS);

    for (uint8 i = 0; i < 3; ++i)
    {
        uint64 guid = instance->GetData64(BOSS_STEELBREAKER + i);
        if (!guid)
            continue;

        if (Creature* boss = Unit::GetCreature(*me, guid))
            boss->SetInCombatWithZone();
    }
}

bool UpdateSupercharge(Creature* target)
{
    if (Aura* supercharge = target->GetAura(SPELL_SUPERCHARGE))
    {
        supercharge->ModStackAmount(1);
        if (UnitAI* AI = target->GetAI())
        {
            AI->DoAction(EVENT_UPDATEPHASE);
            return true;
        }
    }

    return false;
}

class boss_steelbreaker : public CreatureScript
{
    public:
        boss_steelbreaker() : CreatureScript("boss_steelbreaker") { }

        struct boss_steelbreakerAI : public BossAI
        {
            boss_steelbreakerAI(Creature* c) : BossAI(c, BOSS_ASSEMBLY_OF_IRON)
            {
            }

            void Reset()
            {
                _Reset();
                _phase = 0;
                me->RemoveAllAuras();
                me->RemoveLootMode(LOOT_MODE_DEFAULT);
                if (instance)
                    RespawnEncounter(instance, me);
            }

            void EnterCombat(Unit* who)
            {
                me->setActive(true);
                StartEncounter(instance, me, who);
                Talk(SAY_STEELBREAKER_AGGRO);
                DoZoneInCombat();
                DoCast(me, RAID_MODE(SPELL_HIGH_VOLTAGE, SPELL_HIGH_VOLTAGE_H));
                events.ScheduleEvent(EVENT_ENRAGE, 900000);
                events.ScheduleEvent(EVENT_FUSION_PUNCH, 15000);
                DoAction(EVENT_UPDATEPHASE);
            }

            uint32 GetData(uint32 type) const
            {
                if (type == DATA_I_CHOOSE_YOU)
                    return (_phase >= 3) ? 1 : 0;

                return 0;
            }

            void DoAction(int32 action)
            {
                switch (action)
                {
                    case EVENT_UPDATEPHASE:
                        events.SetPhase(++_phase);
                        if (_phase == 2)
                            events.ScheduleEvent(EVENT_STATIC_DISRUPTION, 30000);
                        if (_phase == 3)
                        {
                            me->ResetLootMode();
                            DoCast(me, SPELL_ELECTRICAL_CHARGE, true);
                            events.ScheduleEvent(EVENT_OVERWHELMING_POWER, 3000);
                        }
                    break;
                }
            }

            void DamageTaken(Unit* /*attacker*/, uint32 &damage)
            {
                if (damage >= me->GetHealth())
                {
                    bool has_supercharge = false;

                    if (Creature* Brundir = Unit::GetCreature(*me, instance ? instance->GetData64(BOSS_BRUNDIR) : 0))
                    {
                        if (Brundir->IsAlive())
                        {
                            Brundir->SetFullHealth();
                            has_supercharge = UpdateSupercharge(Brundir);
                        }
                    }

                    if (Creature* Molgeim = Unit::GetCreature(*me, instance ? instance->GetData64(BOSS_MOLGEIM) : 0))
                    {
                        if (Molgeim->IsAlive())
                        {
                            Molgeim->SetFullHealth();
                            has_supercharge = UpdateSupercharge(Molgeim);
                        }
                    }

                    if (!has_supercharge)
                        DoCast(me, SPELL_SUPERCHARGE, true);
                }
            }

            void JustDied(Unit* /*killer*/)
            {
                Talk(RAND(SAY_STEELBREAKER_DEATH_1, SAY_STEELBREAKER_DEATH_2));
                if (IsEncounterComplete(instance, me) && instance)
                {
                    _JustDied();
                    instance->DoUpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET, SPELL_CREDIT_MARKER);
                }
            }

            void KilledUnit(Unit* who)
            {
                Talk(RAND(SAY_STEELBREAKER_SLAY_1, SAY_STEELBREAKER_SLAY_2));
            }

            void SpellHit(Unit* /*from*/, SpellInfo const* spell)
            {
                switch (spell->Id)
                {
                    case SPELL_SUPERCHARGE:
                        DoAction(EVENT_UPDATEPHASE);
                        break;
                    case SPELL_ELECTRICAL_CHARGE_TRIGGERED:
                        if (!me->IsInCombat())
                            me->RemoveAurasDueToSpell(SPELL_ELECTRICAL_CHARGE_TRIGGERED);
                        break;
                }
            }

            void SpellHitTarget(Unit* target, SpellInfo const* spell)
            {
                if (spell->Id == SPELL_MELTDOWN && target && target->ToCreature())
                    target->CastSpell(me, SPELL_ELECTRICAL_CHARGE_TRIGGER, true);
            }

            // try to prefer ranged targets
            Unit* GetDisruptionTarget()
            {
                Map* map = me->GetMap();
                if (map && map->IsDungeon())
                {
                    std::list<Player*> playerList;
                    Map::PlayerList const& Players = map->GetPlayers();
                    for (Map::PlayerList::const_iterator itr = Players.begin(); itr != Players.end(); ++itr)
                    {
                        if (Player* player = itr->GetSource())
                        {
                            if (player->isDead() || player->IsGameMaster())
                                continue;

                            float Distance = player->GetDistance(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ());
                            if (Distance < 15.0f || Distance > 100.0f)
                                continue;

                            playerList.push_back(player);
                        }
                    }

                    if (playerList.empty())
                        return NULL;

                    std::list<Player*>::const_iterator itr = playerList.begin();
                    std::advance(itr, urand(0, playerList.size() - 1));
                    return *itr;
                }
                else
                    return NULL;
            }

            void UpdateAI(uint32 diff)
            {
                if (!UpdateVictim())
                    return;

                events.Update(diff);

                if (me->HasUnitState(UNIT_STATE_CASTING))
                    return;

                while (uint32 eventId = events.ExecuteEvent())
                {
                    switch (eventId)
                    {
                        case EVENT_ENRAGE:
                            Talk(SAY_STEELBREAKER_BERSERK);
                            DoCast(SPELL_BERSERK);
                            break;
                        case EVENT_FUSION_PUNCH:
                            DoCastVictim(RAID_MODE<uint32>(SPELL_FUSION_PUNCH, SPELL_FUSION_PUNCH_H));
                            events.ScheduleEvent(EVENT_FUSION_PUNCH, urand(13000, 22000));
                            break;
                        case EVENT_STATIC_DISRUPTION:
                        {
                            Unit* target = GetDisruptionTarget();
                            if (!target)
                                target = SelectTarget(SELECT_TARGET_RANDOM, 0, 100.0f, true);
                            if (target)
                                DoCast(target, RAID_MODE<uint32>(SPELL_STATIC_DISRUPTION, SPELL_STATIC_DISRUPTION_H));
                            events.ScheduleEvent(EVENT_STATIC_DISRUPTION, 20000);
                            break;
                        }
                        case EVENT_OVERWHELMING_POWER:
                            if (me->GetVictim() && !me->GetVictim()->HasAura(RAID_MODE<uint32>(SPELL_OVERWHELMING_POWER, SPELL_OVERWHELMING_POWER_H)))
                            {
                                Talk(SAY_STEELBREAKER_POWER);
                                DoCastVictim(RAID_MODE<uint32>(SPELL_OVERWHELMING_POWER, SPELL_OVERWHELMING_POWER_H));
                            }
                            events.ScheduleEvent(EVENT_OVERWHELMING_POWER, 2000);
                            break;
                    }
                }

                DoMeleeAttackIfReady();
            }

        private:
            uint32 _phase;
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new boss_steelbreakerAI(creature);
        }
};

class spell_steelbreaker_static_disruption : public SpellScriptLoader
{
    public:
        spell_steelbreaker_static_disruption() : SpellScriptLoader("spell_steelbreaker_static_disruption") { }

        class spell_steelbreaker_static_disruption_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_steelbreaker_static_disruption_SpellScript);

            bool Validate(SpellInfo const* /*spell*/)
            {
                if (!sSpellMgr->GetSpellInfo(61912))
                    return false;
                if (!sSpellMgr->GetSpellInfo(63494))
                    return false;
                return true;
            }

            void HandleTriggerMissile(SpellEffIndex effIndex)
            {
                PreventHitDefaultEffect(effIndex);
                Unit* caster = GetCaster();
                Unit* target = GetExplTargetUnit();
                if (caster && target)
                {
                    uint32 id = uint32(caster->GetMap()->GetDifficulty() == RAID_DIFFICULTY_10MAN_NORMAL ? 61912 : 63494);
                    caster->CastSpell(target, id, true);
                }
            }

            void Register()
            {
                OnEffectHitTarget += SpellEffectFn(spell_steelbreaker_static_disruption_SpellScript::HandleTriggerMissile, EFFECT_0, SPELL_EFFECT_TRIGGER_MISSILE);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_steelbreaker_static_disruption_SpellScript();
        }
};

class spell_steelbreaker_electrical_charge : public SpellScriptLoader
{
    public:
        spell_steelbreaker_electrical_charge() : SpellScriptLoader("spell_steelbreaker_electrical_charge") { }

        class spell_steelbreaker_electrical_charge_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_steelbreaker_electrical_charge_AuraScript);

            void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                Unit* target = GetTarget();
                Unit* caster = GetCaster();
                if (target && target->ToPlayer() && caster && GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_DEATH)
                    target->CastSpell(caster, GetSpellInfo()->Effects[EFFECT_0].CalcValue(), true);
            }

            void Register()
            {
                AfterEffectRemove += AuraEffectRemoveFn(spell_steelbreaker_electrical_charge_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_steelbreaker_electrical_charge_AuraScript();
        }
};

class boss_runemaster_molgeim : public CreatureScript
{
    public:
        boss_runemaster_molgeim() : CreatureScript("boss_runemaster_molgeim") { }

        struct boss_runemaster_molgeimAI : public BossAI
        {
            boss_runemaster_molgeimAI(Creature* c) : BossAI(c, BOSS_ASSEMBLY_OF_IRON)
            {
            }

            void Reset()
            {
                _Reset();
                _phase = 0;
                me->RemoveAllAuras();
                me->RemoveLootMode(LOOT_MODE_DEFAULT);
                if (instance)
                    RespawnEncounter(instance, me);
            }

            void EnterCombat(Unit* who)
            {
                me->setActive(true);
                StartEncounter(instance, me, who);
                Talk(SAY_MOLGEIM_AGGRO);
                DoZoneInCombat();
                events.ScheduleEvent(EVENT_ENRAGE, 900000);
                events.ScheduleEvent(EVENT_SHIELD_OF_RUNES, 27000);
                events.ScheduleEvent(EVENT_RUNE_OF_POWER, 60000);
                DoAction(EVENT_UPDATEPHASE);
            }

            uint32 GetData(uint32 type) const
            {
                if (type == DATA_I_CHOOSE_YOU)
                    return (_phase >= 3) ? 1 : 0;

                return 0;
            }

            void DoAction(int32 action)
            {
                switch (action)
                {
                    case EVENT_UPDATEPHASE:
                        events.SetPhase(++_phase);
                        if (_phase == 2)
                            events.ScheduleEvent(EVENT_RUNE_OF_DEATH, 30000);
                        if (_phase == 3)
                        {
                            me->ResetLootMode();
                            events.ScheduleEvent(EVENT_RUNE_OF_SUMMONING, urand(20000, 30000));
                        }
                    break;
                }
            }

            void DamageTaken(Unit* /*attacker*/, uint32 &damage)
            {
                if (damage >= me->GetHealth())
                {
                    bool has_supercharge = false;

                    if (Creature* Steelbreaker = Unit::GetCreature(*me, instance ? instance->GetData64(BOSS_STEELBREAKER) : 0))
                    {
                        if (Steelbreaker->IsAlive())
                        {
                            Steelbreaker->SetFullHealth();
                            has_supercharge = UpdateSupercharge(Steelbreaker);
                        }
                    }

                    if (Creature* Brundir = Unit::GetCreature((*me), instance ? instance->GetData64(BOSS_BRUNDIR) : 0))
                    {
                        if (Brundir->IsAlive())
                        {
                            Brundir->SetFullHealth();
                            has_supercharge = UpdateSupercharge(Brundir);
                        }
                    }

                    if (!has_supercharge)
                        DoCast(me, SPELL_SUPERCHARGE, true);
                }
            }

            void JustDied(Unit* /*killer*/)
            {
                Talk(RAND(SAY_MOLGEIM_DEATH_1, SAY_MOLGEIM_DEATH_2));
                if (IsEncounterComplete(instance, me) && instance)
                {
                    _JustDied();
                    instance->DoUpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET, SPELL_CREDIT_MARKER);
                }
            }

            void JustSummoned(Creature* summon)
            {
                summons.Summon(summon);
            }

            void KilledUnit(Unit* /*who*/)
            {
                Talk(RAND(SAY_MOLGEIM_SLAY_1, SAY_MOLGEIM_SLAY_2));
            }

            void SpellHit(Unit* /*from*/, SpellInfo const* spell)
            {
                if (spell->Id == SPELL_SUPERCHARGE)
                    DoAction(EVENT_UPDATEPHASE);
            }

            void UpdateAI(uint32 diff)
            {
                if (!UpdateVictim())
                    return;

                events.Update(diff);

                if (me->HasUnitState(UNIT_STATE_CASTING))
                    return;

                while (uint32 eventId = events.ExecuteEvent())
                {
                    switch (eventId)
                    {
                        case EVENT_ENRAGE:
                            Talk(SAY_MOLGEIM_BERSERK);
                            DoCast(SPELL_BERSERK);
                            break;
                        case EVENT_RUNE_OF_POWER:
                        {
                            Unit* target = DoSelectLowestHpFriendly(60);
                            if (!target || !target->IsAlive())
                                target = me;
                            DoCast(target, SPELL_SUMMON_RUNE_OF_POWER);
                            events.ScheduleEvent(EVENT_RUNE_OF_POWER, 60000);
                            break;
                        }
                        case EVENT_SHIELD_OF_RUNES:
                            DoCast(me, RAID_MODE(SPELL_SHIELD_OF_RUNES, SPELL_SHIELD_OF_RUNES_H));
                            events.ScheduleEvent(EVENT_SHIELD_OF_RUNES, urand(27000, 34000));
                            break;
                        case EVENT_RUNE_OF_DEATH:
                            Talk(SAY_MOLGEIM_RUNE_DEATH);
                            if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM))
                                DoCast(target, SPELL_RUNE_OF_DEATH);
                            events.ScheduleEvent(EVENT_RUNE_OF_DEATH, urand(30000, 40000));
                            break;
                        case EVENT_RUNE_OF_SUMMONING:
                            Talk(SAY_MOLGEIM_SUMMON);
                            if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM))
                                DoCast(target, SPELL_RUNE_OF_SUMMONING);
                            events.ScheduleEvent(EVENT_RUNE_OF_SUMMONING, urand(35000, 45000));
                            break;
                    }
                }

                DoMeleeAttackIfReady();
            }

        private:
            uint32 _phase;
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new boss_runemaster_molgeimAI(creature);
        }
};

class mob_rune_of_power : public CreatureScript
{
    public:
        mob_rune_of_power() : CreatureScript("mob_rune_of_power") { }

        CreatureAI* GetAI(Creature* creature) const
        {
            return new mob_rune_of_powerAI(creature);
        }

        struct mob_rune_of_powerAI : public ScriptedAI
        {
            mob_rune_of_powerAI(Creature* c) : ScriptedAI(c)
            {
                me->SetInCombatWithZone();
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                me->setFaction(16); // Same faction as bosses
                DoCast(SPELL_RUNE_OF_POWER);
                me->DespawnOrUnsummon(60000);
            }
        };
};

class mob_lightning_elemental : public CreatureScript
{
    public:
        mob_lightning_elemental() : CreatureScript("mob_lightning_elemental") { }

        struct mob_lightning_elementalAI : public ScriptedAI
        {
            mob_lightning_elementalAI(Creature* c) : ScriptedAI(c)
            {
                _Casted = false;
                SetVictim();
            }

            void SetVictim()
            {
                me->SetInCombatWithZone();
                if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM))
                {
                    me->AddThreat(target, 99999.9f);
                    AttackStart(target);
                }
            }

            void UpdateAI(uint32 /*diff*/)
            {
                if (!UpdateVictim())
                    return;

                if (me->IsWithinMeleeRange(me->GetVictim()) && !_Casted)
                {
                    me->CastSpell(me, RAID_MODE(SPELL_LIGHTNING_BLAST_10, SPELL_LIGHTNING_BLAST_25), true);
                    me->DespawnOrUnsummon(500);
                    _Casted = true;
                }
            }

        private:
            bool _Casted;
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new mob_lightning_elementalAI(creature);
        }
};

class mob_rune_of_summoning : public CreatureScript
{
    public:
        mob_rune_of_summoning() : CreatureScript("mob_rune_of_summoning") { }

        struct mob_rune_of_summoningAI : public ScriptedAI
        {
            mob_rune_of_summoningAI(Creature* c) : ScriptedAI(c)
            {
                _summonCount = 0;
                _summonTimer = 5000;
                _instance = c->GetInstanceScript();
                me->AddAura(SPELL_RUNE_OF_SUMMONING_VIS, me);
            }

            void JustSummoned(Creature* summon)
            {
                if (Creature* Molgeim = Unit::GetCreature(*me, _instance ? _instance->GetData64(BOSS_MOLGEIM) : 0))
                    Molgeim->AI()->JustSummoned(summon);
            }

            void SummonLightningElemental()
            {
                me->CastSpell(me, SPELL_RUNE_OF_SUMMONING_SUMMON, false);
                if (++_summonCount == 10)
                    me->DespawnOrUnsummon();
                else
                    _summonTimer = 2000;
            }

            void UpdateAI(uint32 diff)
            {
                if (_summonTimer <= diff)
                    SummonLightningElemental();
                else
                    _summonTimer -= diff;
            }

        private:
            InstanceScript* _instance;
            uint32 _summonCount;
            uint32 _summonTimer;
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new mob_rune_of_summoningAI(creature);
        }
};

class boss_stormcaller_brundir : public CreatureScript
{
    public:
        boss_stormcaller_brundir() : CreatureScript("boss_stormcaller_brundir") { }

        struct boss_stormcaller_brundirAI : public BossAI
        {
            boss_stormcaller_brundirAI(Creature* c) : BossAI(c, BOSS_ASSEMBLY_OF_IRON)
            {
            }

            void Reset()
            {
                _Reset();
                _phase = 0;
                _forceLand = false;
                me->RemoveAllAuras();
                me->RemoveLootMode(LOOT_MODE_DEFAULT);
                me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_STUN, false);
                if (instance)
                    RespawnEncounter(instance, me);

                me->SetSpeed(MOVE_RUN, 1.42857f);
                if (me->HasUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY))
                {
                    me->RemoveUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY);
                    me->SendMovementFlagUpdate();
                }
            }

            void EnterCombat(Unit* who)
            {
                me->setActive(true);
                StartEncounter(instance, me, who);
                Talk(SAY_BRUNDIR_AGGRO);
                DoZoneInCombat();
                events.ScheduleEvent(EVENT_ENRAGE, 900000);
                events.ScheduleEvent(EVENT_CHAIN_LIGHTNING, urand(9000, 17000), 1);
                events.ScheduleEvent(EVENT_OVERLOAD, urand(60000, 80000), 1);
                DoAction(EVENT_UPDATEPHASE);
            }

            uint32 GetData(uint32 type) const
            {
                if (type == DATA_I_CHOOSE_YOU)
                    return (_phase >= 3) ? 1 : 0;

                return 0;
            }

            void DoAction(int32 action)
            {
                switch (action)
                {
                    case EVENT_UPDATEPHASE:
                        events.SetPhase(++_phase);
                        if (_phase == 2)
                            events.ScheduleEvent(EVENT_LIGHTNING_WHIRL, urand(20000, 40000), 1);
                        if (_phase == 3)
                        {
                            me->ResetLootMode();
                            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_STUN, true);
                            DoCast(me, SPELL_STORMSHIELD, true);
                            events.ScheduleEvent(EVENT_LIGHTNING_TENDRILS_START, urand(40000, 80000));
                        }
                    break;

                }
            }

            void DamageTaken(Unit* /*attacker*/, uint32 &damage)
            {
                if (damage >= me->GetHealth())
                {
                    // do not die flying
                    if (me->HasUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY))
                    {
                        damage = me->GetHealth() - 1;

                        if (!_forceLand)
                        {
                            me->GetMotionMaster()->MovePoint(POINT_LAND, me->GetPositionX(), me->GetPositionY(), 427.28f);
                            me->RemoveAurasDueToSpell(RAID_MODE(SPELL_LIGHTNING_TENDRILS, SPELL_LIGHTNING_TENDRILS_H));
                            events.CancelEvent(EVENT_LIGHTNING_TENDRILS_END);
                            events.CancelEvent(EVENT_THREAT_WIPE);
                            _forceLand = true;
                        }
                        return;
                    }

                    bool has_supercharge = false;

                    if (Creature* Steelbreaker = Unit::GetCreature(*me, instance ? instance->GetData64(BOSS_STEELBREAKER) : 0))
                    {
                        if (Steelbreaker->IsAlive())
                        {
                            Steelbreaker->SetFullHealth();
                            has_supercharge = UpdateSupercharge(Steelbreaker);
                        }
                    }

                    if (Creature* Molgeim = Unit::GetCreature(*me, instance ? instance->GetData64(BOSS_MOLGEIM) : 0))
                    {
                        if (Molgeim->IsAlive())
                        {
                            Molgeim->SetFullHealth();
                            has_supercharge = UpdateSupercharge(Molgeim);
                        }
                    }

                    if (!has_supercharge)
                        DoCast(me, SPELL_SUPERCHARGE, true);
                }
            }

            void JustDied(Unit* /*killer*/)
            {
                Talk(RAND(SAY_BRUNDIR_DEATH_1, SAY_BRUNDIR_DEATH_2));
                if (IsEncounterComplete(instance, me) && instance)
                {
                    _JustDied();
                    instance->DoUpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BE_SPELL_TARGET, SPELL_CREDIT_MARKER);
                }
            }

            void KilledUnit(Unit* /*who*/)
            {
                Talk(RAND(SAY_BRUNDIR_SLAY_1, SAY_BRUNDIR_SLAY_2));
            }

            void SpellHit(Unit* /*from*/, SpellInfo const* spell)
            {
                if (spell->Id == SPELL_SUPERCHARGE)
                    DoAction(EVENT_UPDATEPHASE);
            }

            void MovementInform(uint32 type, uint32 id)
            {
                if (type != POINT_MOTION_TYPE)
                    return;

                switch (id)
                {
                    case POINT_FLY:
                    {
                        DoCast(RAID_MODE(SPELL_LIGHTNING_TENDRILS, SPELL_LIGHTNING_TENDRILS_H));
                        events.ScheduleEvent(EVENT_LIGHTNING_TENDRILS_END, 30000);
                        events.ScheduleEvent(EVENT_THREAT_WIPE, 0);
                        break;
                    }
                    case POINT_LAND:
                    {
                        me->SetReactState(REACT_AGGRESSIVE);
                        me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_TAUNT, false);
                        me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_ATTACK_ME, false);
                        me->SetSpeed(MOVE_RUN, 1.42857f);
                        me->RemoveUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY);
                        me->SendMovementFlagUpdate();
                        if (me->GetVictim())
                            me->GetMotionMaster()->MoveChase(me->GetVictim());
                        break;
                    }
                    case POINT_CHASE:
                    {
                        if (Unit* target = me->GetVictim())
                            me->GetMotionMaster()->MovePoint(POINT_CHASE, target->GetPositionX(), target->GetPositionY(), 435.0f);
                        break;
                    }
                }
            }

            void UpdateAI(uint32 diff)
            {
                if (!UpdateVictim())
                    return;

                events.Update(diff);

                if (me->HasUnitState(UNIT_STATE_CASTING))
                    return;

                while (uint32 eventId = events.ExecuteEvent())
                {
                    switch (eventId)
                    {
                        case EVENT_ENRAGE:
                            Talk(SAY_BRUNDIR_BERSERK);
                            DoCast(SPELL_BERSERK);
                            break;
                        case EVENT_CHAIN_LIGHTNING:
                            if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM))
                                DoCast(target, RAID_MODE(SPELL_CHAIN_LIGHTNING_N, SPELL_CHAIN_LIGHTNING_H));
                            events.ScheduleEvent(EVENT_CHAIN_LIGHTNING, urand(3000, 5000), 1);
                            break;
                        case EVENT_OVERLOAD:
                            me->MonsterTextEmote(EMOTE_OVERLOAD, 0, true);
                            DoCast(RAID_MODE(SPELL_OVERLOAD, SPELL_OVERLOAD_H));
                            events.ScheduleEvent(EVENT_OVERLOAD, urand(60000, 80000), 1);
                            break;
                        case EVENT_LIGHTNING_WHIRL:
                            DoCast(RAID_MODE(SPELL_LIGHTNING_WHIRL, SPELL_LIGHTNING_WHIRL_H));
                            events.ScheduleEvent(EVENT_LIGHTNING_WHIRL, urand(20000, 40000), 1);
                            break;
                        case EVENT_THREAT_WIPE:
                            DoResetThreat();
                            if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0))
                            {
                                me->AddThreat(target, 99999.9f);
                                me->GetMotionMaster()->MovePoint(POINT_CHASE, target->GetPositionX(), target->GetPositionY(), 435.0f);
                            }
                            events.ScheduleEvent(EVENT_THREAT_WIPE, 5000);
                            break;
                        case EVENT_LIGHTNING_TENDRILS_START:
                            me->SetSpeed(MOVE_RUN, 0.7f);
                            me->SetReactState(REACT_PASSIVE);
                            me->ApplySpellImmune(0, IMMUNITY_STATE, SPELL_AURA_MOD_TAUNT, true);
                            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_ATTACK_ME, true);
                            me->AddUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY);
                            me->SendMovementFlagUpdate();
                            me->GetMotionMaster()->MovePoint(POINT_FLY, me->GetPositionX(), me->GetPositionY(), 435.0f);
                            events.DelayEvents(35000, 1);
                            break;
                        case EVENT_LIGHTNING_TENDRILS_END:
                            me->GetMotionMaster()->MovePoint(POINT_LAND, me->GetPositionX(), me->GetPositionY(), 427.28f);
                            me->RemoveAurasDueToSpell(RAID_MODE(SPELL_LIGHTNING_TENDRILS, SPELL_LIGHTNING_TENDRILS_H));
                            events.ScheduleEvent(EVENT_LIGHTNING_TENDRILS_START, urand(40000, 80000));
                            events.CancelEvent(EVENT_THREAT_WIPE);
                            break;
                    }
                }

                if (!me->HasAura(RAID_MODE(SPELL_LIGHTNING_TENDRILS, SPELL_LIGHTNING_TENDRILS_H)))
                    DoMeleeAttackIfReady();
            }

        private:
            uint32 _phase;
            bool _forceLand;
        };

        CreatureAI* GetAI(Creature* creature) const
        {
            return new boss_stormcaller_brundirAI(creature);
        }
};

class spell_shield_of_runes : public SpellScriptLoader
{
    public:
        spell_shield_of_runes() : SpellScriptLoader("spell_shield_of_runes") { }

        class spell_shield_of_runes_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_shield_of_runes_AuraScript);

            void OnAbsorb(AuraEffect* /*aurEff*/, DamageInfo& dmgInfo, uint32& absorbAmount)
            {
                uint32 damage = dmgInfo.GetDamage();

                if (absorbAmount > damage)
                    return;

                if (Unit* caster = GetCaster())
                    caster->CastSpell(caster, SPELL_SHIELD_OF_RUNES_BUFF, true);
            }

            void Register()
            {
                OnEffectAbsorb += AuraEffectAbsorbFn(spell_shield_of_runes_AuraScript::OnAbsorb, EFFECT_0);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_shield_of_runes_AuraScript();
        }
};

class achievement_i_choose_you : public AchievementCriteriaScript
{
    public:
        achievement_i_choose_you() : AchievementCriteriaScript("achievement_i_choose_you")
        {
        }

        bool OnCheck(Player* /*player*/, Unit* target)
        {
            if (!target)
                return false;

            if (Creature* boss = target->ToCreature())
                if (boss->AI()->GetData(DATA_I_CHOOSE_YOU))
                    return true;

            return false;
        }
};

class achievement_but_i_am_on_your_side : public AchievementCriteriaScript
{
    public:
        achievement_but_i_am_on_your_side() : AchievementCriteriaScript("achievement_but_i_am_on_your_side")
        {
        }

        bool OnCheck(Player* player, Unit* target)
        {
            if (!target || !player)
                return false;

            if (Creature* boss = target->ToCreature())
                if (boss->AI()->GetData(DATA_I_CHOOSE_YOU) && player->HasAura(SPELL_IRON_BOOT_FLASK))
                    return true;

            return false;
        }
};

void AddSC_boss_assembly_of_iron()
{
    new boss_steelbreaker();
    new spell_steelbreaker_static_disruption();
    new spell_steelbreaker_electrical_charge();
    new boss_runemaster_molgeim();
    new boss_stormcaller_brundir();
    new mob_lightning_elemental();
    new mob_rune_of_summoning();
    new mob_rune_of_power();
    new spell_shield_of_runes();
    new achievement_i_choose_you();
    new achievement_but_i_am_on_your_side();
}