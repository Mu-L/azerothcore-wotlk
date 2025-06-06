/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Cell.h"
#include "CellImpl.h"
#include "CreatureScript.h"
#include "GameObjectScript.h"
#include "GridNotifiers.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"

/// @todo: this import is not necessary for compilation and marked as unused by the IDE
//  however, for some reasons removing it would cause a damn linking issue
//  there is probably some underlying problem with imports which should properly addressed
//  see: https://github.com/azerothcore/azerothcore-wotlk/issues/9766
#include "GridNotifiersImpl.h"

enum deathsdoorfell
{
    SPELL_ARTILLERY_ON_THE_WRAP_GATE            = 39221,
    SPELL_IMP_AURA                              = 39227,
    SPELL_HOUND_AURA                            = 39275,
    SPELL_EXPLOSION                             = 30934,

    NPC_DEATHS_DOOR_FEL_CANNON_TARGET_BUNNY     = 22495,
    NPC_DEATHS_DOOR_FEL_CANNON                  = 22443,
    NPC_EXPLOSION_BUNNY                         = 22502,
    NPC_FEL_IMP                                 = 22474,
    NPC_HOUND                                   = 22500,
    NPC_NORTH_GATE                              = 22471,
    NPC_SOUTH_GATE                              = 22472,
    NPC_NORTH_GATE_CREDIT                       = 22503,
    NPC_SOUTH_GATE_CREDIT                       = 22504,

    GO_FIRE                                     = 185317,
    GO_BIG_FIRE                                 = 185319,

    EVENT_PARTY_TIMER                           = 1
};

class npc_deaths_door_fell_cannon_target_bunny : public CreatureScript
{
public:
    npc_deaths_door_fell_cannon_target_bunny() : CreatureScript("npc_deaths_door_fell_cannon_target_bunny") { }

    struct npc_deaths_door_fell_cannon_target_bunnyAI : public ScriptedAI
    {
        npc_deaths_door_fell_cannon_target_bunnyAI(Creature* creature) : ScriptedAI(creature), PartyTime(false) { }

        EventMap events;
        bool PartyTime;
        ObjectGuid PlayerGUID;
        ObjectGuid CannonGUID;
        uint8 count;

        void Reset() override
        {
            Initialize();
            events.Reset();
        }

        void Initialize()
        {
            PartyTime = false;
            PlayerGUID.Clear();
            CannonGUID.Clear();
            count = 0;
        }

        void SpellHit(Unit* caster, SpellInfo const* spell) override
        {
            if (spell->Id == SPELL_ARTILLERY_ON_THE_WRAP_GATE)
            {
                count++;

                if (count == 1)
                {
                    if (Player* player = caster->GetCharmerOrOwnerPlayerOrPlayerItself())
                        PlayerGUID = player->GetGUID();

                    CannonGUID = caster->GetGUID();
                    PartyTime = true;
                    events.ScheduleEvent(EVENT_PARTY_TIMER, 3000);
                }

                if (count >= 3)
                    me->SummonGameObject(GO_FIRE, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 130);

                if (count > 6)
                {
                    if (Player* player = ObjectAccessor::GetPlayer(*me, PlayerGUID))
                    {
                        if (GetClosestCreatureWithEntry(me, NPC_SOUTH_GATE, 200.0f))
                            player->RewardPlayerAndGroupAtEvent(NPC_SOUTH_GATE_CREDIT, player);
                        else if (GetClosestCreatureWithEntry(me, NPC_NORTH_GATE, 200.0f))
                            player->RewardPlayerAndGroupAtEvent(NPC_NORTH_GATE_CREDIT, player);
                        // complete quest part
                        if (Creature* bunny = GetClosestCreatureWithEntry(me, NPC_EXPLOSION_BUNNY, 200.0f))
                            bunny->CastSpell(nullptr, SPELL_EXPLOSION, TRIGGERED_NONE);
                        if (Creature* cannon = ObjectAccessor::GetCreature(*me, CannonGUID))
                            cannon->DespawnOrUnsummon(5000);
                    }

                    me->SummonGameObject(GO_BIG_FIRE, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 60);
                    Reset();
                }
            }

            return;
        }

        void JustSummoned(Creature* summoned) override
        {
            if (summoned->GetEntry() == NPC_FEL_IMP)
                summoned->CastSpell(summoned, SPELL_IMP_AURA, true);
            else if (summoned->GetEntry() == NPC_HOUND)
                summoned->CastSpell(summoned, SPELL_HOUND_AURA, true);

            if (Creature* Target = GetClosestCreatureWithEntry(me, NPC_DEATHS_DOOR_FEL_CANNON, 200.0f))
            {
                Target->RemoveUnitFlag(UNIT_FLAG_NOT_ATTACKABLE_1); // attack the cannon
                summoned->AI()->AttackStart(Target);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            if (PartyTime)
            {
                if (Creature* cannon = ObjectAccessor::GetCreature(*me, CannonGUID))
                {
                    if (!cannon || !cannon->GetCharmerOrOwnerGUID())
                        Reset();
                }

                switch (events.ExecuteEvent())
                {
                    case EVENT_PARTY_TIMER:
                        if (roll_chance_i(20))
                            me->SummonCreature(NPC_HOUND, 0, 0, 0, 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 10000);
                        else
                            me->SummonCreature(NPC_FEL_IMP, 0, 0, 0, 0, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 10000);
                        events.ScheduleEvent(EVENT_PARTY_TIMER, 3000);
                        break;
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_deaths_door_fell_cannon_target_bunnyAI(creature);
    }
};

class npc_deaths_fel_cannon : public CreatureScript
{
public:
    npc_deaths_fel_cannon() : CreatureScript("npc_deaths_fel_cannon") { }

    struct npc_deaths_fel_cannonAI : public ScriptedAI
    {
        npc_deaths_fel_cannonAI(Creature* creature) : ScriptedAI(creature) { }

        void Reset() override
        {
            me->SetUnitFlag(UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_NOT_ATTACKABLE_1);
        }

        void UpdateAI(uint32 /*diff*/) override
        {
            if (me->IsNonMeleeSpellCast(false))
                return;

            if (Creature* Target = GetClosestCreatureWithEntry(me, NPC_DEATHS_DOOR_FEL_CANNON_TARGET_BUNNY, 200.0f))
            {
                me->SetFacingToObject(Target);
                me->TauntFadeOut(Target);
                me->CombatStop(); // force
            }

            Reset();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_deaths_fel_cannonAI(creature);
    }
};

enum CrystalPrison
{
    SPELL_CRYSTAL_SHATTER = 40898
};

class spell_npc22275_crystal_prison_aura : public AuraScript
{
    PrepareAuraScript(spell_npc22275_crystal_prison_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CRYSTAL_SHATTER });
    }

    void OnPeriodic(AuraEffect const*  /*aurEff*/)
    {
        PreventDefaultAction();
        SetDuration(0);
        GetTarget()->CastSpell(GetTarget(), SPELL_CRYSTAL_SHATTER, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_npc22275_crystal_prison_aura::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

/*######
## npc_nether_drake
######*/

enum Netherdrake
{
    //Used by 20021, 21817, 21820, 21821, 21823 but not existing in database
    SAY_NIHIL_1                 = 0,
    SAY_NIHIL_2                 = 1,
    SAY_NIHIL_3                 = 2,
    SAY_NIHIL_4                 = 3,
    SAY_NIHIL_INTERRUPT         = 4,

    ENTRY_WHELP                 = 20021,
    ENTRY_PROTO                 = 21821,
    ENTRY_ADOLE                 = 21817,
    ENTRY_MATUR                 = 21820,
    ENTRY_NIHIL                 = 21823,

    SPELL_T_PHASE_MODULATOR     = 37573,

    SPELL_ARCANE_BLAST          = 38881,
    SPELL_MANA_BURN             = 38884,
    SPELL_INTANGIBLE_PRESENCE   = 36513
};

class npc_nether_drake : public CreatureScript
{
public:
    npc_nether_drake() : CreatureScript("npc_nether_drake") { }

    struct npc_nether_drakeAI : public ScriptedAI
    {
        npc_nether_drakeAI(Creature* creature) : ScriptedAI(creature) { }

        bool IsNihil;
        uint32 NihilSpeech_Timer;
        uint32 NihilSpeech_Phase;

        uint32 ArcaneBlast_Timer;
        uint32 ManaBurn_Timer;
        uint32 IntangiblePresence_Timer;

        void Reset() override
        {
            IsNihil = false;
            NihilSpeech_Timer = 3000;
            NihilSpeech_Phase = 0;

            ArcaneBlast_Timer = 7500;
            ManaBurn_Timer = 10000;
            IntangiblePresence_Timer = 15000;
        }

        void JustEngagedWith(Unit* /*who*/) override { }

        void MoveInLineOfSight(Unit* who) override

        {
            if (me->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
                return;

            ScriptedAI::MoveInLineOfSight(who);
        }

        //in case Creature was not summoned (not expected)
        void MovementInform(uint32 type, uint32 id) override
        {
            if (type != POINT_MOTION_TYPE)
                return;

            if (id == 0)
            {
                me->setDeathState(DeathState::JustDied);
                me->RemoveCorpse();
                me->SetHealth(0);
            }
        }

        void SpellHit(Unit* caster, SpellInfo const* spell) override
        {
            if (spell->Id == SPELL_T_PHASE_MODULATOR && caster->IsPlayer())
            {
                const uint32 entry_list[4] = {ENTRY_PROTO, ENTRY_ADOLE, ENTRY_MATUR, ENTRY_NIHIL};
                int cid = rand() % (4 - 1);

                if (entry_list[cid] == me->GetEntry())
                    ++cid;

                //we are nihil, so say before transform
                if (me->GetEntry() == ENTRY_NIHIL)
                {
                    Talk(SAY_NIHIL_INTERRUPT);
                    me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                    IsNihil = false;
                }

                if (me->UpdateEntry(entry_list[cid]))
                {
                    if (entry_list[cid] == ENTRY_NIHIL)
                    {
                        EnterEvadeMode();
                        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                        IsNihil = true;
                    }
                    else
                        AttackStart(caster);
                }
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (IsNihil)
            {
                if (NihilSpeech_Timer <= diff)
                {
                    switch (NihilSpeech_Phase)
                    {
                        case 0:
                            Talk(SAY_NIHIL_1);
                            ++NihilSpeech_Phase;
                            break;
                        case 1:
                            Talk(SAY_NIHIL_2);
                            ++NihilSpeech_Phase;
                            break;
                        case 2:
                            Talk(SAY_NIHIL_3);
                            ++NihilSpeech_Phase;
                            break;
                        case 3:
                            Talk(SAY_NIHIL_4);
                            ++NihilSpeech_Phase;
                            break;
                        case 4:
                            me->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
                            //take off to location above
                            me->GetMotionMaster()->MovePoint(0, me->GetPositionX() + 50.0f, me->GetPositionY(), me->GetPositionZ() + 50.0f);
                            ++NihilSpeech_Phase;
                            break;
                    }
                    NihilSpeech_Timer = 5000;
                }
                else NihilSpeech_Timer -= diff;

                //anything below here is not interesting for Nihil, so skip it
                return;
            }

            if (!UpdateVictim())
                return;

            if (IntangiblePresence_Timer <= diff)
            {
                DoCastVictim(SPELL_INTANGIBLE_PRESENCE);
                IntangiblePresence_Timer = 15000 + rand() % 15000;
            }
            else IntangiblePresence_Timer -= diff;

            if (ManaBurn_Timer <= diff)
            {
                Unit* target = me->GetVictim();
                if (target && target->getPowerType() == POWER_MANA)
                    DoCast(target, SPELL_MANA_BURN);
                ManaBurn_Timer = 8000 + rand() % 8000;
            }
            else ManaBurn_Timer -= diff;

            if (ArcaneBlast_Timer <= diff)
            {
                DoCastVictim(SPELL_ARCANE_BLAST);
                ArcaneBlast_Timer = 2500 + rand() % 5000;
            }
            else ArcaneBlast_Timer -= diff;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_nether_drakeAI(creature);
    }
};

/*######
## npc_daranelle
######*/

enum Daranelle
{
    SAY_SPELL_INFLUENCE       = 0,
    SPELL_LASHHAN_CHANNEL     = 36904,
    SPELL_DISPELLING_ANALYSIS = 37028,

    NPC_KALIRI_TOTEM          = 21468
};

class npc_daranelle : public CreatureScript
{
public:
    npc_daranelle() : CreatureScript("npc_daranelle") { }

    struct npc_daranelleAI : public ScriptedAI
    {
        npc_daranelleAI(Creature* creature) : ScriptedAI(creature) { }

        void Reset() override { }

        void JustEngagedWith(Unit* /*who*/) override { }

        void MoveInLineOfSight(Unit* who) override

        {
            if (who->IsPlayer())
            {
                if (who->HasAura(SPELL_LASHHAN_CHANNEL) && me->IsWithinDistInMap(who, 10.0f))
                {
                    if (Creature* bird = who->FindNearestCreature(NPC_KALIRI_TOTEM, 10.0f))
                    {
                        Talk(SAY_SPELL_INFLUENCE, who);
                        /// @todo Move the below to updateAI and run if this statement == true
                        DoCast(who, SPELL_DISPELLING_ANALYSIS, true);
                        bird->DespawnOrUnsummon(2000);
                    }
                }
            }

            ScriptedAI::MoveInLineOfSight(who);
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_daranelleAI(creature);
    }
};

enum SimonGame
{
    NPC_SIMON_BUNNY                 = 22923,
    NPC_APEXIS_GUARDIAN             = 22275,

    GO_APEXIS_RELIC                 = 185890,
    GO_APEXIS_MONUMENT              = 185944,
    GO_AURA_BLUE                    = 185872,
    GO_AURA_GREEN                   = 185873,
    GO_AURA_RED                     = 185874,
    GO_AURA_YELLOW                  = 185875,

    GO_BLUE_CLUSTER_DISPLAY         = 7369,
    GO_GREEN_CLUSTER_DISPLAY        = 7371,
    GO_RED_CLUSTER_DISPLAY          = 7373,
    GO_YELLOW_CLUSTER_DISPLAY       = 7375,
    GO_BLUE_CLUSTER_DISPLAY_LARGE   = 7364,
    GO_GREEN_CLUSTER_DISPLAY_LARGE  = 7365,
    GO_RED_CLUSTER_DISPLAY_LARGE    = 7366,
    GO_YELLOW_CLUSTER_DISPLAY_LARGE = 7367,

    SPELL_PRE_GAME_BLUE             = 40176,
    SPELL_PRE_GAME_GREEN            = 40177,
    SPELL_PRE_GAME_RED              = 40178,
    SPELL_PRE_GAME_YELLOW           = 40179,
    SPELL_VISUAL_BLUE               = 40244,
    SPELL_VISUAL_GREEN              = 40245,
    SPELL_VISUAL_RED                = 40246,
    SPELL_VISUAL_YELLOW             = 40247,

    SOUND_BLUE                      = 11588,
    SOUND_GREEN                     = 11589,
    SOUND_RED                       = 11590,
    SOUND_YELLOW                    = 11591,
    SOUND_DISABLE_NODE              = 11758,

    SPELL_AUDIBLE_GAME_TICK         = 40391,
    SPELL_VISUAL_START_PLAYER_LEVEL = 40436,
    SPELL_VISUAL_START_AI_LEVEL     = 40387,

    SPELL_BAD_PRESS_TRIGGER         = 41241,
    SPELL_BAD_PRESS_DAMAGE          = 40065,
    SPELL_REWARD_BUFF_1             = 40310,
    SPELL_REWARD_BUFF_2             = 40311,
    SPELL_REWARD_BUFF_3             = 40312,
};

enum SimonEvents
{
    EVENT_SIMON_SETUP_PRE_GAME         = 1,
    EVENT_SIMON_PLAY_SEQUENCE          = 2,
    EVENT_SIMON_RESET_CLUSTERS         = 3,
    EVENT_SIMON_PERIODIC_PLAYER_CHECK  = 4,
    EVENT_SIMON_TOO_LONG_TIME          = 5,
    EVENT_SIMON_GAME_TICK              = 6,
    EVENT_SIMON_ROUND_FINISHED         = 7,

    ACTION_SIMON_CORRECT_FULL_SEQUENCE = 8,
    ACTION_SIMON_WRONG_SEQUENCE        = 9,
    ACTION_SIMON_ROUND_FINISHED        = 10,
};

enum SimonColors
{
    SIMON_BLUE          = 0,
    SIMON_RED           = 1,
    SIMON_GREEN         = 2,
    SIMON_YELLOW        = 3,
    SIMON_MAX_COLORS    = 4,
};

class npc_simon_bunny : public CreatureScript
{
public:
    npc_simon_bunny() : CreatureScript("npc_simon_bunny") { }

    struct npc_simon_bunnyAI : public ScriptedAI
    {
        npc_simon_bunnyAI(Creature* creature) : ScriptedAI(creature) { }

        bool large;
        bool listening;
        uint8 gameLevel;
        uint8 fails;
        uint8 gameTicks;
        ObjectGuid playerGUID;
        uint32 clusterIds[SIMON_MAX_COLORS];
        float zCoordCorrection;
        float searchDistance;
        EventMap _events;
        std::list<uint8> colorSequence, playableSequence, playerSequence;

        void UpdateAI(uint32 diff) override
        {
            _events.Update(diff);

            switch (_events.ExecuteEvent())
            {
                case EVENT_SIMON_PERIODIC_PLAYER_CHECK:
                    if (!CheckPlayer())
                        ResetNode();
                    else
                        _events.ScheduleEvent(EVENT_SIMON_PERIODIC_PLAYER_CHECK, 2000);
                    break;
                case EVENT_SIMON_SETUP_PRE_GAME:
                    SetUpPreGame();
                    _events.CancelEvent(EVENT_SIMON_GAME_TICK);
                    _events.ScheduleEvent(EVENT_SIMON_PLAY_SEQUENCE, 1000);
                    break;
                case EVENT_SIMON_PLAY_SEQUENCE:
                    if (!playableSequence.empty())
                    {
                        PlayNextColor();
                        _events.ScheduleEvent(EVENT_SIMON_PLAY_SEQUENCE, 1500);
                    }
                    else
                    {
                        listening = true;
                        DoCast(SPELL_VISUAL_START_PLAYER_LEVEL);
                        playerSequence.clear();
                        PrepareClusters();
                        gameTicks = 0;
                        _events.ScheduleEvent(EVENT_SIMON_GAME_TICK, 3000);
                    }
                    break;
                case EVENT_SIMON_GAME_TICK:
                    DoCast(SPELL_AUDIBLE_GAME_TICK);

                    if (gameTicks > gameLevel)
                        _events.ScheduleEvent(EVENT_SIMON_TOO_LONG_TIME, 500);
                    else
                        _events.ScheduleEvent(EVENT_SIMON_GAME_TICK, 3000);
                    gameTicks++;
                    break;
                case EVENT_SIMON_RESET_CLUSTERS:
                    PrepareClusters(true);
                    break;
                case EVENT_SIMON_TOO_LONG_TIME:
                    DoAction(ACTION_SIMON_WRONG_SEQUENCE);
                    break;
                case EVENT_SIMON_ROUND_FINISHED:
                    DoAction(ACTION_SIMON_ROUND_FINISHED);
                    break;
            }
        }

        void DoAction(int32 action) override
        {
            switch (action)
            {
                case ACTION_SIMON_ROUND_FINISHED:
                    listening = false;
                    DoCast(SPELL_VISUAL_START_AI_LEVEL);
                    GiveRewardForLevel(gameLevel);
                    _events.CancelEventGroup(0);
                    if (gameLevel == 10)
                        ResetNode();
                    else
                        _events.ScheduleEvent(EVENT_SIMON_SETUP_PRE_GAME, 1000);
                    break;
                case ACTION_SIMON_CORRECT_FULL_SEQUENCE:
                    gameLevel++;
                    DoAction(ACTION_SIMON_ROUND_FINISHED);
                    break;
                case ACTION_SIMON_WRONG_SEQUENCE:
                    GivePunishment();
                    DoAction(ACTION_SIMON_ROUND_FINISHED);
                    break;
            }
        }

        // Called by color clusters script (go_simon_cluster) and used for knowing the button pressed by player
        void SetData(uint32 type, uint32 /*data*/) override
        {
            if (!listening)
                return;

            uint8 pressedColor = SIMON_MAX_COLORS;

            if (type == clusterIds[SIMON_RED])
                pressedColor = SIMON_RED;
            else if (type == clusterIds[SIMON_BLUE])
                pressedColor = SIMON_BLUE;
            else if (type == clusterIds[SIMON_GREEN])
                pressedColor = SIMON_GREEN;
            else if (type == clusterIds[SIMON_YELLOW])
                pressedColor = SIMON_YELLOW;

            PlayColor(pressedColor);
            playerSequence.push_back(pressedColor);
            _events.ScheduleEvent(EVENT_SIMON_RESET_CLUSTERS, 500);
            CheckPlayerSequence();
        }

        // Used for getting involved player guid. Parameter id is used for defining if is a large(Monument) or small(Relic) node
        void SetGUID(ObjectGuid guid, int32 id) override
        {
            me->SetCanFly(true);

            large = (bool)id;
            playerGUID = guid;
            StartGame();
        }

        /*
        Resets all variables and also find the ids of the four closests color clusters, since every simon
        node have diferent ids for clusters this is absolutely NECESSARY.
        */
        void StartGame()
        {
            listening = false;
            gameLevel = 0;
            fails = 0;
            gameTicks = 0;
            zCoordCorrection = large ? 8.0f : 2.75f;
            searchDistance = large ? 13.0f : 5.0f;
            colorSequence.clear();
            playableSequence.clear();
            playerSequence.clear();
            me->SetObjectScale(large ? 2.0f : 1.0f);

            std::list<WorldObject*> ClusterList;
            Acore::AllWorldObjectsInRange objects(me, searchDistance);
            Acore::WorldObjectListSearcher<Acore::AllWorldObjectsInRange> searcher(me, ClusterList, objects);
            Cell::VisitAllObjects(me, searcher, searchDistance);

            for (std::list<WorldObject*>::const_iterator i = ClusterList.begin(); i != ClusterList.end(); ++i)
            {
                if (GameObject* go = (*i)->ToGameObject())
                {
                    // We are checking for displayid because all simon nodes have 4 clusters with different entries
                    if (large)
                    {
                        switch (go->GetGOInfo()->displayId)
                        {
                            case GO_BLUE_CLUSTER_DISPLAY_LARGE:
                                clusterIds[SIMON_BLUE] = go->GetEntry();
                                break;

                            case GO_RED_CLUSTER_DISPLAY_LARGE:
                                clusterIds[SIMON_RED] = go->GetEntry();
                                break;

                            case GO_GREEN_CLUSTER_DISPLAY_LARGE:
                                clusterIds[SIMON_GREEN] = go->GetEntry();
                                break;

                            case GO_YELLOW_CLUSTER_DISPLAY_LARGE:
                                clusterIds[SIMON_YELLOW] = go->GetEntry();
                                break;
                        }
                    }
                    else
                    {
                        switch (go->GetGOInfo()->displayId)
                        {
                            case GO_BLUE_CLUSTER_DISPLAY:
                                clusterIds[SIMON_BLUE] = go->GetEntry();
                                break;

                            case GO_RED_CLUSTER_DISPLAY:
                                clusterIds[SIMON_RED] = go->GetEntry();
                                break;

                            case GO_GREEN_CLUSTER_DISPLAY:
                                clusterIds[SIMON_GREEN] = go->GetEntry();
                                break;

                            case GO_YELLOW_CLUSTER_DISPLAY:
                                clusterIds[SIMON_YELLOW] = go->GetEntry();
                                break;
                        }
                    }
                }
            }

            _events.Reset();
            _events.ScheduleEvent(EVENT_SIMON_ROUND_FINISHED, 1000);
            _events.ScheduleEvent(EVENT_SIMON_PERIODIC_PLAYER_CHECK, 2000);

            if (GameObject* relic = me->FindNearestGameObject(large ? GO_APEXIS_MONUMENT : GO_APEXIS_RELIC, searchDistance))
                relic->SetGameObjectFlag(GO_FLAG_NOT_SELECTABLE);
        }

        // Called when despawning the bunny. Sets all the node GOs to their default states.
        void ResetNode()
        {
            DoPlaySoundToSet(me, SOUND_DISABLE_NODE);

            for (uint32 clusterId = SIMON_BLUE; clusterId < SIMON_MAX_COLORS; clusterId++)
                if (GameObject* cluster = me->FindNearestGameObject(clusterIds[clusterId], searchDistance))
                    cluster->SetGameObjectFlag(GO_FLAG_NOT_SELECTABLE);

            for (uint32 auraId = GO_AURA_BLUE; auraId <= GO_AURA_YELLOW; auraId++)
                if (GameObject* auraGo = me->FindNearestGameObject(auraId, searchDistance))
                    auraGo->RemoveFromWorld();

            if (GameObject* relic = me->FindNearestGameObject(large ? GO_APEXIS_MONUMENT : GO_APEXIS_RELIC, searchDistance))
                relic->RemoveGameObjectFlag(GO_FLAG_NOT_SELECTABLE);

            me->DespawnOrUnsummon(1000);
        }

        /*
        Called on every button click of player. Adds the clicked color to the player created sequence and
        checks if it corresponds to the AI created sequence. If so, incremente gameLevel and start a new
        round, if not, give punishment and restart current level.
        */
        void CheckPlayerSequence()
        {
            bool correct = true;
            if (playerSequence.size() <= colorSequence.size())
                for (std::list<uint8>::const_iterator i = playerSequence.begin(), j = colorSequence.begin(); i != playerSequence.end(); ++i, ++j)
                    if ((*i) != (*j))
                        correct = false;

            if (correct && (playerSequence.size() == colorSequence.size()))
                DoAction(ACTION_SIMON_CORRECT_FULL_SEQUENCE);
            else if (!correct)
                DoAction(ACTION_SIMON_WRONG_SEQUENCE);
        }

        /*
        Generates a random sequence of colors depending on the gameLevel. We also copy this sequence to
        the playableSequence wich will be used when playing the sequence to the player.
        */
        void GenerateColorSequence()
        {
            colorSequence.clear();
            for (uint8 i = 0; i <= gameLevel; i++)
                colorSequence.push_back(RAND(SIMON_BLUE, SIMON_RED, SIMON_GREEN, SIMON_YELLOW));

            for (std::list<uint8>::const_iterator i = colorSequence.begin(); i != colorSequence.end(); ++i)
                playableSequence.push_back(*i);
        }

        // Remove any existant glowing auras over clusters and set clusters ready for interating with them.
        void PrepareClusters(bool clustersOnly = false)
        {
            for (uint32 clusterId = SIMON_BLUE; clusterId < SIMON_MAX_COLORS; clusterId++)
                if (GameObject* cluster = me->FindNearestGameObject(clusterIds[clusterId], searchDistance))
                    cluster->RemoveGameObjectFlag(GO_FLAG_NOT_SELECTABLE);

            if (clustersOnly)
                return;

            for (uint32 auraId = GO_AURA_BLUE; auraId <= GO_AURA_YELLOW; auraId++)
                if (GameObject* auraGo = me->FindNearestGameObject(auraId, searchDistance))
                    auraGo->RemoveFromWorld();
        }

        /*
        Called when AI is playing the sequence for player. We cast the visual spell and then remove the
        cast color from the casting sequence.
        */
        void PlayNextColor()
        {
            PlayColor(*playableSequence.begin());
            playableSequence.erase(playableSequence.begin());
        }

        // Casts a spell and plays a sound depending on parameter color.
        void PlayColor(uint8 color)
        {
            switch (color)
            {
                case SIMON_BLUE:
                    DoCast(SPELL_VISUAL_BLUE);
                    DoPlaySoundToSet(me, SOUND_BLUE);
                    break;
                case SIMON_GREEN:
                    DoCast(SPELL_VISUAL_GREEN);
                    DoPlaySoundToSet(me, SOUND_GREEN);
                    break;
                case SIMON_RED:
                    DoCast(SPELL_VISUAL_RED);
                    DoPlaySoundToSet(me, SOUND_RED);
                    break;
                case SIMON_YELLOW:
                    DoCast(SPELL_VISUAL_YELLOW);
                    DoPlaySoundToSet(me, SOUND_YELLOW);
                    break;
            }
        }

        /*
        Creates the transparent glowing auras on every cluster of this node.
        After calling this function bunny is teleported to the center of the node.
        */
        void SetUpPreGame()
        {
            for (uint32 clusterId = SIMON_BLUE; clusterId < SIMON_MAX_COLORS; clusterId++)
            {
                if (GameObject* cluster = me->FindNearestGameObject(clusterIds[clusterId], 2.0f * searchDistance))
                {
                    cluster->SetGameObjectFlag(GO_FLAG_NOT_SELECTABLE);

                    // break since we don't need glowing auras for large clusters
                    if (large)
                        break;

                    float x, y, z, o;
                    cluster->GetPosition(x, y, z, o);
                    me->NearTeleportTo(x, y, z, o);

                    uint32 preGameSpellId;
                    if (cluster->GetEntry() == clusterIds[SIMON_RED])
                        preGameSpellId = SPELL_PRE_GAME_RED;
                    else if (cluster->GetEntry() == clusterIds[SIMON_BLUE])
                        preGameSpellId = SPELL_PRE_GAME_BLUE;
                    else if (cluster->GetEntry() == clusterIds[SIMON_GREEN])
                        preGameSpellId = SPELL_PRE_GAME_GREEN;
                    else if (cluster->GetEntry() == clusterIds[SIMON_YELLOW])
                        preGameSpellId = SPELL_PRE_GAME_YELLOW;
                    else break;

                    me->CastSpell(cluster, preGameSpellId, true);
                }
            }

            if (GameObject* relic = me->FindNearestGameObject(large ? GO_APEXIS_MONUMENT : GO_APEXIS_RELIC, searchDistance))
            {
                float x, y, z, o;
                relic->GetPosition(x, y, z, o);
                me->NearTeleportTo(x, y, z + zCoordCorrection, o);
            }

            GenerateColorSequence();
        }

        // Handles the spell rewards. The spells also have the QuestCompleteEffect, so quests credits are working.
        void GiveRewardForLevel(uint8 level)
        {
            uint32 rewSpell = 0;
            switch (level)
            {
                case 6:
                    if (large)
                        GivePunishment();
                    else
                        rewSpell = SPELL_REWARD_BUFF_1;
                    break;
                case 8:
                    rewSpell = SPELL_REWARD_BUFF_2;
                    break;
                case 10:
                    rewSpell = SPELL_REWARD_BUFF_3;
                    break;
            }

            if (rewSpell)
                if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
                    DoCast(player, rewSpell, true);
        }

        /*
        Depending on the number of failed pushes for player the damage of the spell scales, so we first
        cast the spell on the target that hits for 50 and shows the visual and then forces the player
        to cast the damaging spell on it self with the modified basepoints.
        4 fails = death.
        On large nodes punishment and reward are the same, summoning the Apexis Guardian.
        */
        void GivePunishment()
        {
            if (large)
            {
                if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
                    if (Creature* guardian = me->SummonCreature(NPC_APEXIS_GUARDIAN, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ() - zCoordCorrection, 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 20000))
                        guardian->AI()->AttackStart(player);

                ResetNode();
            }
            else
            {
                fails++;

                if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
                    DoCast(player, SPELL_BAD_PRESS_TRIGGER, true);

                if (fails >= 4)
                    ResetNode();
            }
        }

        void SpellHitTarget(Unit* target, SpellInfo const* spell) override
        {
            // Cast SPELL_BAD_PRESS_DAMAGE with scaled basepoints when the visual hits the target.
            // Need Fix: When SPELL_BAD_PRESS_TRIGGER hits target it triggers spell SPELL_BAD_PRESS_DAMAGE by itself
            // so player gets damage equal to calculated damage  dbc basepoints for SPELL_BAD_PRESS_DAMAGE (~50)
            if (spell->Id == SPELL_BAD_PRESS_TRIGGER)
            {
                int32 bp = (int32)((float)(fails) * 0.33f * target->GetMaxHealth());
                target->CastCustomSpell(target, SPELL_BAD_PRESS_DAMAGE, &bp, nullptr, nullptr, true);
            }
        }

        // Checks if player has already die or has get too far from the current node
        bool CheckPlayer()
        {
            if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
            {
                if (player->isDead())
                    return false;
                if (player->GetDistance2d(me) >= 2.0f * searchDistance)
                {
                    GivePunishment();
                    return false;
                }
            }
            else
                return false;

            return true;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_simon_bunnyAI(creature);
    }
};

class go_simon_cluster : public GameObjectScript
{
public:
    go_simon_cluster() : GameObjectScript("go_simon_cluster") { }

    bool OnGossipHello(Player* player, GameObject* go) override
    {
        if (Creature* bunny = go->FindNearestCreature(NPC_SIMON_BUNNY, 12.0f, true))
            bunny->AI()->SetData(go->GetEntry(), 0);

        player->CastSpell(player, go->GetGOInfo()->goober.spellId, true);
        go->AddUse();
        return true;
    }
};

enum ApexisRelic
{
    QUEST_CRYSTALS            = 11025,
    GOSSIP_TEXT_ID            = 10948,

    ITEM_APEXIS_SHARD         = 32569,
    SPELL_TAKE_REAGENTS_SOLO  = 41145,
    SPELL_TAKE_REAGENTS_GROUP = 41146,
};

class go_apexis_relic : public GameObjectScript
{
public:
    go_apexis_relic() : GameObjectScript("go_apexis_relic") { }

    bool OnGossipHello(Player* player, GameObject* go) override
    {
        player->PrepareGossipMenu(go, go->GetGOInfo()->questgiver.gossipID);
        player->SendPreparedGossip(go);
        return true;
    }

    bool OnGossipSelect(Player* player, GameObject* go, uint32 /*sender*/, uint32 /*action*/) override
    {
        CloseGossipMenuFor(player);

        bool large = (go->GetEntry() == GO_APEXIS_MONUMENT);
        if (player->HasItemCount(ITEM_APEXIS_SHARD, large ? 35 : 1))
        {
            player->CastSpell(player, large ? SPELL_TAKE_REAGENTS_GROUP : SPELL_TAKE_REAGENTS_SOLO, false);

            if (Creature* bunny = player->SummonCreature(NPC_SIMON_BUNNY, go->GetPositionX(), go->GetPositionY(), go->GetPositionZ()))
                bunny->AI()->SetGUID(player->GetGUID(), large);
        }

        return true;
    }
};

/*######
## npc_oscillating_frequency_scanner_master_bunny used for quest 10594 "Gauging the Resonant Frequency"
######*/

enum ScannerMasterBunny
{
    NPC_OSCILLATING_FREQUENCY_SCANNER_TOP_BUNNY = 21759,
    GO_OSCILLATING_FREQUENCY_SCANNER            = 184926,
    SPELL_OSCILLATION_FIELD                     = 37408,
    QUEST_GAUGING_THE_RESONANT_FREQUENCY        = 10594
};

class npc_oscillating_frequency_scanner_master_bunny : public CreatureScript
{
public:
    npc_oscillating_frequency_scanner_master_bunny() : CreatureScript("npc_oscillating_frequency_scanner_master_bunny") { }

    struct npc_oscillating_frequency_scanner_master_bunnyAI : public ScriptedAI
    {
        npc_oscillating_frequency_scanner_master_bunnyAI(Creature* creature) : ScriptedAI(creature)
        {
            playerGuid.Clear();
            timer = 500;
        }

        void Reset() override
        {
            if (GetClosestCreatureWithEntry(me, NPC_OSCILLATING_FREQUENCY_SCANNER_TOP_BUNNY, 25.0f))
                me->DespawnOrUnsummon();
            else
            {
                // Spell 37392 does not exist in dbc, manually spawning
                me->SummonCreature(NPC_OSCILLATING_FREQUENCY_SCANNER_TOP_BUNNY, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->GetOrientation(), TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 50000);
                me->SummonGameObject(GO_OSCILLATING_FREQUENCY_SCANNER, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->GetOrientation(), 0, 0, 0, 0, 50);
                me->DespawnOrUnsummon(50000);
            }

            timer = 500;
        }

        void IsSummonedBy(WorldObject* summoner) override
        {
            if (summoner && summoner->isType(TYPEMASK_PLAYER))
                playerGuid = summoner->GetGUID();
        }

        void UpdateAI(uint32 diff) override
        {
            if (timer <= diff)
            {
                if (Player* player = ObjectAccessor::GetPlayer(*me, playerGuid))
                    DoCast(player, SPELL_OSCILLATION_FIELD);

                timer = 3000;
            }
            else
                timer -= diff;
        }

    private:
        ObjectGuid playerGuid;
        uint32 timer;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_oscillating_frequency_scanner_master_bunnyAI(creature);
    }
};

class spell_oscillating_field : public SpellScript
{
    PrepareSpellScript(spell_oscillating_field);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_OSCILLATION_FIELD });
    }

    void HandleEffect(SpellEffIndex /*effIndex*/)
    {
        if (Player* player = GetHitPlayer())
            if (player->GetAuraCount(SPELL_OSCILLATION_FIELD) == 5 && player->GetQuestStatus(QUEST_GAUGING_THE_RESONANT_FREQUENCY) == QUEST_STATUS_INCOMPLETE)
                player->CompleteQuest(QUEST_GAUGING_THE_RESONANT_FREQUENCY);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_oscillating_field::HandleEffect, EFFECT_0, SPELL_EFFECT_APPLY_AURA);
    }
};

void AddSC_blades_edge_mountains()
{
    new npc_deaths_door_fell_cannon_target_bunny();
    new npc_deaths_fel_cannon();
    RegisterSpellScript(spell_npc22275_crystal_prison_aura);
    new npc_nether_drake();
    new npc_daranelle();
    new npc_simon_bunny();
    new go_simon_cluster();
    new go_apexis_relic();
    new npc_oscillating_frequency_scanner_master_bunny();
    RegisterSpellScript(spell_oscillating_field);
}
