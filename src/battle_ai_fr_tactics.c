#include "global.h"
#include "battle.h"
#include "battle_ai_fr_tactics.h"
#include "battle_ai_main.h"
#include "battle_ai_util.h"
#include "battle_controllers.h"
#include "battle_main.h"
#include "battle_util.h"
#include "item.h"
#include "pokemon.h"
#include "random.h"
#include "status_rail.h"
#include "constants/abilities.h"
#include "constants/battle_ai.h"
#include "constants/battle_move_effects.h"
#include "constants/hold_effects.h"

// Fantasy Retold: AI_FLAG_FR_TACTICS (design.md §10). Everything here is a score adjustment for one move against
// one target, added after the upstream scoring:
//  - it never lowers a move that KOs the target, and its total is capped at FR_TACTICS_MAX_BONUS, below a KO's
//    weight (FAST_KILL / SLOW_KILL + BEST_DAMAGE_MOVE), so lethals, the type chart (already in the damage numbers),
//    hazards and the rest of the upstream logic still win;
//  - the one exception is decisive speed control (FR_TACTICS_DECISIVE), and only when the AI has no KO on that
//    target this turn.

#define FR_TACTICS_MAX_BONUS     GOOD_EFFECT
#define FR_TACTICS_MIN_BONUS     (-2 * GOOD_EFFECT)
#define FR_TACTICS_DECISIVE      BEST_EFFECT
#define FR_STATUS_MIN_CHANCE     30 // a doubled secondary chance at least this high counts as applying the status

enum DodgeOutcome
{
    DODGE_OUTCOME_UNAFFECTED,
    DODGE_OUTCOME_HALVED,
    DODGE_OUTCOME_MISS,
};

// The player's innate is only played around once it has visibly fired (RecordNatureInnate); the AI always knows
// its own side's.
bool32 AI_IsInnateKnown(enum BattlerId battler, enum BattlerId observer)
{
    if (IsBattlerAlly(battler, observer))
        return TRUE;
    return gAiPartyData->mons[GetBattlerSide(battler)][gBattlerPartyIndexes[battler]].innateRevealed;
}

// Crit-immune as far as `observer` knows: Battle Armor / Shell Armor, Lucky Chant, or a known Bold / Impish innate.
// That also rules out the 0-Stamina auto-crit.
bool32 AI_IsCritImmune(enum BattlerId battler, enum BattlerId observer)
{
    enum Ability ability = gAiLogicData->abilities[battler];

    if (ability == ABILITY_BATTLE_ARMOR || ability == ABILITY_SHELL_ARMOR)
        return TRUE;
    if (gSideStatuses[GetBattlerSide(battler)] & SIDE_STATUS_LUCKY_CHANT)
        return TRUE;
    return BattlerHasInnate(battler, INNATE_BATTLE_ARMOR) && AI_IsInnateKnown(battler, observer);
}

static enum StatusRail GetRailFromMoveEffect(enum MoveEffect effect)
{
    switch (effect)
    {
    case MOVE_EFFECT_POISON:
    case MOVE_EFFECT_TOXIC:     return RAIL_POISON;
    case MOVE_EFFECT_BURN:      return RAIL_BURN;
    case MOVE_EFFECT_FREEZE:
    case MOVE_EFFECT_FROSTBITE: return RAIL_CHILL;
    case MOVE_EFFECT_PARALYSIS: return RAIL_ELECTRIC;
    case MOVE_EFFECT_SLEEP:     return RAIL_SLEEP;
    case MOVE_EFFECT_CONFUSION: return RAIL_CONFUSION;
    default:                    return NUM_STATUS_RAILS;
    }
}

static enum Type GetAIMoveType(enum BattlerId battler, enum Move move)
{
    return CheckDynamicMoveType(GetBattlerMon(battler), move, battler, MON_IN_BATTLE);
}

// The status line `move` applies to its target, and whether as a status move (Lv2) or a likely secondary (Lv1).
// NUM_STATUS_RAILS if none. Sheer Force (ability or Adamant / Modest innate) drops the Lv1 secondaries.
static enum StatusRail GetMoveStatusRail(enum BattlerId battlerAtk, enum Move move, bool32 *isStatusMove)
{
    enum StatusRail rail;

    *isStatusMove = FALSE;
    if (GetMovePower(move) == 0)
    {
        *isStatusMove = TRUE;
        if (GetMoveEffect(move) == EFFECT_CONFUSE)
            return RAIL_CONFUSION;
        return GetRailFromMoveEffect(GetMoveNonVolatileStatus(move));
    }

    if (gAiLogicData->abilities[battlerAtk] == ABILITY_SHEER_FORCE || BattlerHasInnate(battlerAtk, INNATE_SHEER_FORCE))
        return NUM_STATUS_RAILS;

    for (u32 i = 0; i < GetMoveAdditionalEffectCount(move); i++)
    {
        const struct AdditionalEffect *effect = GetMoveAdditionalEffectById(move, i);
        u32 chance = (effect->chance == 0) ? 100 : effect->chance * 2;

        if (effect->self || chance < FR_STATUS_MIN_CHANCE)
            continue;
        rail = GetRailFromMoveEffect(effect->moveEffect);
        if (rail != NUM_STATUS_RAILS)
            return rail;
    }
    return NUM_STATUS_RAILS;
}

// Whether the line can still land or climb on the target (the engine's own checks: immunities, Safeguard,
// terrains, a line at its top level, Burn vs Soaked / Chill, ...).
static bool32 CanProgressRail(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum StatusRail rail, enum Move move)
{
    switch (rail)
    {
    case RAIL_POISON:   return CanSetNonVolatileStatus(battlerAtk, battlerDef, gAiLogicData->abilities[battlerAtk], gAiLogicData->abilities[battlerDef], MOVE_EFFECT_TOXIC, CHECK_TRIGGER);
    case RAIL_BURN:     return CanSetNonVolatileStatus(battlerAtk, battlerDef, gAiLogicData->abilities[battlerAtk], gAiLogicData->abilities[battlerDef], MOVE_EFFECT_BURN, CHECK_TRIGGER);
    case RAIL_CHILL:    return CanSetNonVolatileStatus(battlerAtk, battlerDef, gAiLogicData->abilities[battlerAtk], gAiLogicData->abilities[battlerDef], MOVE_EFFECT_FREEZE, CHECK_TRIGGER);
    case RAIL_ELECTRIC: return CanSetNonVolatileStatus(battlerAtk, battlerDef, gAiLogicData->abilities[battlerAtk], gAiLogicData->abilities[battlerDef], MOVE_EFFECT_PARALYSIS, CHECK_TRIGGER);
    case RAIL_SLEEP:    return CanSetNonVolatileStatus(battlerAtk, battlerDef, gAiLogicData->abilities[battlerAtk], gAiLogicData->abilities[battlerDef], MOVE_EFFECT_SLEEP, CHECK_TRIGGER);
    case RAIL_CONFUSION: return AI_CanBeConfused(battlerAtk, battlerDef, move, gAiLogicData->abilities[battlerDef]);
    default:            return FALSE;
    }
}

// The level the line reaches if this move lands (design.md §6): status moves land at Lv2 (Chill at Chilled), hits
// add one, and Dazed + Soaked skips straight to Paralyzed.
static u32 GetNextRailLevel(enum BattlerId battlerDef, enum StatusRail rail, bool32 isStatusMove)
{
    u32 level = GetStatusRailLevel(battlerDef, rail);
    u32 next = level + 1;

    if (isStatusMove && rail != RAIL_CHILL && next < 2)
        next = 2;
    if (rail == RAIL_ELECTRIC && level == 1 && GetStatusRailLevel(battlerDef, RAIL_SOAK) != 0)
        next = 3;
    if (next > GetStatusRailMaxLevel(rail))
        next = GetStatusRailMaxLevel(rail);
    return next;
}

static bool32 IsCleansingBerryFor(enum HoldEffect holdEffect, enum StatusRail rail)
{
    switch (holdEffect)
    {
    case HOLD_EFFECT_CURE_STATUS:    return TRUE;
    case HOLD_EFFECT_CURE_PSN:       return rail == RAIL_POISON;
    case HOLD_EFFECT_CURE_BRN:       return rail == RAIL_BURN;
    case HOLD_EFFECT_CURE_FRZ:       return rail == RAIL_CHILL;
    case HOLD_EFFECT_CURE_PAR:       return rail == RAIL_ELECTRIC;
    case HOLD_EFFECT_CURE_SLP:       return rail == RAIL_SLEEP;
    case HOLD_EFFECT_CURE_CONFUSION: return rail == RAIL_CONFUSION;
    default:                         return FALSE;
    }
}

// The target gains from having a status: an Orb with Guts / Poison Heal / Quick Feet / Marvel Scale, or a boost
// ability that wants this line.
static bool32 TargetWantsStatus(enum BattlerId battlerDef, enum StatusRail rail)
{
    enum Ability ability = gAiLogicData->abilities[battlerDef];
    enum HoldEffect holdEffect = gAiLogicData->holdEffects[battlerDef];

    if (ability == ABILITY_GUTS || ability == ABILITY_QUICK_FEET || ability == ABILITY_MARVEL_SCALE)
        return holdEffect == HOLD_EFFECT_FLAME_ORB || holdEffect == HOLD_EFFECT_TOXIC_ORB || rail != RAIL_SLEEP;
    if (ability == ABILITY_POISON_HEAL || ability == ABILITY_TOXIC_BOOST)
        return rail == RAIL_POISON || holdEffect == HOLD_EFFECT_TOXIC_ORB;
    if (ability == ABILITY_FLARE_BOOST)
        return rail == RAIL_BURN || holdEffect == HOLD_EFFECT_FLAME_ORB;
    return FALSE;
}

// Rough crit stage for the crit-upgrade combos (Ghost crit -> Nightmare, Psychic crit -> Frightened).
static u32 GetAICritStage(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move)
{
    u32 stage;
    enum HoldEffect holdEffect = gAiLogicData->holdEffects[battlerAtk];

    if (AI_IsCritImmune(battlerDef, battlerAtk))
        return 0;
    if (MoveAlwaysCrits(move) || gBattleMons[battlerAtk].volatiles.laserFocusTimer > 0)
        return 3;
    stage = GetMoveCriticalHitStage(move)
          + (gBattleMons[battlerAtk].volatiles.focusEnergy ? 2 : 0)
          + (gAiLogicData->abilities[battlerAtk] == ABILITY_SUPER_LUCK ? 1 : 0)
          + (BattlerHasInnate(battlerAtk, INNATE_SUPER_LUCK) ? 1 : 0)
          + ((holdEffect == HOLD_EFFECT_SCOPE_LENS) ? 1 : 0);
    return stage;
}

static s32 CritStageBonus(u32 stage)
{
    if (stage >= 3)
        return GOOD_EFFECT;
    if (stage == 2)
        return DECENT_EFFECT;
    if (stage == 1)
        return WEAK_EFFECT;
    return NO_INCREASE;
}

// ---- Status: rail value, cures, combos, self-sabotage -----------------------------------------------------

static s32 GetStatusApplicationBonus(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move)
{
    bool32 isStatusMove;
    enum StatusRail rail = GetMoveStatusRail(battlerAtk, move, &isStatusMove);
    s32 bonus = 0;
    u32 level, next;

    if (rail == NUM_STATUS_RAILS || !CanProgressRail(battlerAtk, battlerDef, rail, move))
        return 0;

    if (TargetWantsStatus(battlerDef, rail))
        return -DECENT_EFFECT;

    level = GetStatusRailLevel(battlerDef, rail);
    next = GetNextRailLevel(battlerDef, rail, isStatusMove);

    // Climbing an existing line: to its top level (Paralyzed, Nightmare, Frightened, Burn x3, Toxic) is the big
    // step; Lv1 -> Lv2 turns a volatile status into a lasting one.
    if (level != 0 && next == GetStatusRailMaxLevel(rail))
        bonus += DECENT_EFFECT;
    else if (level == 1 && next >= 2)
        bonus += WEAK_EFFECT;

    // Dazed + Soaked: the Electric application skips Shocked.
    if (rail == RAIL_ELECTRIC && level == 1 && GetStatusRailLevel(battlerDef, RAIL_SOAK) != 0)
        bonus += DECENT_EFFECT;

    // Cleansing berries ignore Lv1: chip the line with Lv1 procs, keep the Lv2 status move for after the berry.
    if (IsCleansingBerryFor(gAiLogicData->holdEffects[battlerDef], rail))
        bonus += (isStatusMove && level == 0) ? -DECENT_EFFECT : (isStatusMove ? 0 : WEAK_EFFECT);

    // A known Natural Cure innate clears everything below Lv3 on its next switch-out.
    if (BattlerHasInnate(battlerDef, INNATE_NATURAL_CURE) && AI_IsInnateKnown(battlerDef, battlerAtk)
     && next < 3 && !(rail == RAIL_POISON && next == 2))
        bonus -= WEAK_EFFECT;

    // Poison blocks Leftovers and other small heals.
    if (rail == RAIL_POISON && gAiLogicData->holdEffects[battlerDef] == HOLD_EFFECT_LEFTOVERS)
        bonus += WEAK_EFFECT;

    // Burn cuts physical damage: aim it at a physical attacker, all the more when the AI is frail against it.
    if (rail == RAIL_BURN && HasMoveWithCategory(battlerDef, DAMAGE_CATEGORY_PHYSICAL)
     && gBattleMons[battlerDef].attack >= gBattleMons[battlerDef].spAttack)
    {
        bonus += DECENT_EFFECT;
        if (NoOfHitsForTargetToFaintBattler(battlerDef, battlerAtk, AI_DEFENDING, DONT_CONSIDER_ENDURE) <= 2)
            bonus += WEAK_EFFECT;
    }

    return bonus;
}

// Moves that undo the AI's own pressure on the target (or lift a status off the AI itself).
static s32 GetRailInteractionBonus(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move)
{
    enum Type moveType;
    s32 bonus = 0;

    if (GetMovePower(move) == 0)
        return 0;
    moveType = GetAIMoveType(battlerAtk, move);

    // Ice takes a Burn stack off (Water only through a Soak, too rare to score); Fire thaws the whole Chill line;
    // Ground drains the Electric line. Unwelcome on a target the AI is pressuring, unless that status helps the target.
    if (moveType == TYPE_ICE && GetStatusRailLevel(battlerDef, RAIL_BURN) != 0)
        bonus += TargetWantsStatus(battlerDef, RAIL_BURN) ? WEAK_EFFECT : -DECENT_EFFECT;
    if (moveType == TYPE_FIRE && GetStatusRailLevel(battlerDef, RAIL_CHILL) != 0
     && !(GetStatusRailLevel(battlerDef, RAIL_CHILL) >= 2 && GetStatusRailBranch(battlerDef, RAIL_CHILL) == CHILL_BRANCH_FROZEN)) // upstream handles Frozen
        bonus -= DECENT_EFFECT;
    if (moveType == TYPE_GROUND && GetStatusRailLevel(battlerDef, RAIL_ELECTRIC) != 0)
        bonus += TargetWantsStatus(battlerDef, RAIL_ELECTRIC) ? WEAK_EFFECT : -DECENT_EFFECT;

    // The Sleep valve: a direct non-Ghost hit may wake a sleeper; Ghost hits never do.
    if (GetStatusRailLevel(battlerDef, RAIL_SLEEP) >= 2)
        bonus += (moveType == TYPE_GHOST) ? WEAK_EFFECT : -WEAK_EFFECT;

    // The same interactions on the AI itself are cures.
    if (moveType == TYPE_ICE && GetStatusRailLevel(battlerAtk, RAIL_BURN) != 0)
        bonus += WEAK_EFFECT;
    if (moveType == TYPE_FIRE && GetStatusRailLevel(battlerAtk, RAIL_CHILL) != 0)
        bonus += WEAK_EFFECT;
    if (moveType == TYPE_GROUND && GetStatusRailLevel(battlerAtk, RAIL_ELECTRIC) != 0)
        bonus += WEAK_EFFECT;

    // Crit upgrades: a Ghost crit sinks Drowsy / Asleep into Nightmare, a Psychic crit makes Confused / Stupefied
    // Frightened. Worth more the likelier the crit (Super Luck innate, high-crit moves, Focus Energy).
    if (moveType == TYPE_GHOST && GetStatusRailLevel(battlerDef, RAIL_SLEEP) != 0 && GetStatusRailLevel(battlerDef, RAIL_SLEEP) < 3)
        bonus += CritStageBonus(GetAICritStage(battlerAtk, battlerDef, move));
    if (moveType == TYPE_PSYCHIC && GetStatusRailLevel(battlerDef, RAIL_CONFUSION) != 0 && GetStatusRailLevel(battlerDef, RAIL_CONFUSION) < 3)
        bonus += CritStageBonus(GetAICritStage(battlerAtk, battlerDef, move));

    return bonus;
}

static bool32 HasKnownDamagingMoveOfType(enum BattlerId battler, enum Type type)
{
    for (u32 slot = 0; slot < AI_GetMoveSlotCount(battler); slot++)
    {
        enum Move move = AI_GetKnownMoveAtSlot(battler, slot);
        if (move != MOVE_NONE && GetMovePower(move) != 0 && GetAIMoveType(battler, move) == type)
            return TRUE;
    }
    return FALSE;
}

// Doubles: combos with the partner, and what a Stupefied battler might do to its own ally.
static s32 GetDoublesStatusBonus(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move)
{
    enum BattlerId partner = GetPartnerBattler(battlerAtk);
    enum BattlerId defPartner = GetPartnerBattler(battlerDef);
    enum MoveTarget target = AI_GetBattlerMoveTargetType(battlerAtk, move);
    s32 bonus = 0;

    if (!IsDoubleBattle())
        return 0;

    // Soak the foes ahead of an Electric / Ice partner (Electric and Ice deal x1.5 to Soaked, Dazed + Soaked
    // paralyzes outright). A spread Water move soaks both.
    if (IsBattlerAlive(partner) && GetMovePower(move) != 0 && GetAIMoveType(battlerAtk, move) == TYPE_WATER
     && !IS_BATTLER_OF_TYPE(battlerDef, TYPE_WATER) && GetStatusRailLevel(battlerDef, RAIL_SOAK) == 0
     && GetStatusRailLevel(battlerDef, RAIL_BURN) == 0
     && (HasKnownDamagingMoveOfType(partner, TYPE_ELECTRIC) || HasKnownDamagingMoveOfType(partner, TYPE_ICE)))
        bonus += IsSpreadMove(target) ? DECENT_EFFECT : WEAK_EFFECT;

    // A Stupefied battler may turn a single-target hit on its ally: keep strong single-target hits off when that
    // could KO the ally.
    if (IsBattlerAlive(partner) && GetStatusRailLevel(battlerAtk, RAIL_CONFUSION) >= 2 && GetMovePower(move) != 0
     && target == TARGET_SELECTED)
    {
        u32 slot = gAiThinkingStruct->movesetIndex;
        if (CanIndexMoveFaintTarget(battlerAtk, partner, slot, AI_ATTACKING_PARTNER))
            bonus -= DECENT_EFFECT;
    }

    // Stupefy the foe whose own ally is frail: its procs then hit that ally.
    if (IsBattlerAlive(defPartner) && GetMovePower(move) == 0 && GetMoveEffect(move) == EFFECT_CONFUSE
     && CanProgressRail(battlerAtk, battlerDef, RAIL_CONFUSION, move))
    {
        bool32 canKOAlly = FALSE;
        for (u32 slot = 0; slot < AI_GetMoveSlotCount(battlerDef) && !canKOAlly; slot++)
        {
            enum Move defMove = AI_GetKnownMoveAtSlot(battlerDef, slot);
            if (defMove != MOVE_NONE && GetMovePower(defMove) != 0 && AI_GetBattlerMoveTargetType(battlerDef, defMove) == TARGET_SELECTED
             && CanIndexMoveFaintTarget(battlerDef, defPartner, slot, AI_ATTACKING))
                canKOAlly = TRUE;
        }
        if (canKOAlly)
            bonus += DECENT_EFFECT;
        else if (gAiLogicData->hpPercents[defPartner] < 50)
            bonus += WEAK_EFFECT;
    }

    return bonus;
}

// ---- Speed control --------------------------------------------------------------------------------------

// The target's Speed after `move` lands, in percent (100 = no change). Tailwind is folded in as halving the target,
// Trick Room as 0 (whoever is slower moves first).
static u32 GetSpeedControlPercent(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move)
{
    bool32 isStatusMove;
    enum StatusRail rail;
    u32 percent = 100;

    switch (GetMoveEffect(move))
    {
    case EFFECT_TAILWIND:
        return (gSideStatuses[GetBattlerSide(battlerAtk)] & SIDE_STATUS_TAILWIND) ? 100 : 50;
    case EFFECT_TRICK_ROOM:
        return (gFieldStatuses & STATUS_FIELD_TRICK_ROOM) ? 100 : 0;
    case EFFECT_YAWN:
        return (GetStatusRailLevel(battlerDef, RAIL_SLEEP) == 0 && CanProgressRail(battlerAtk, battlerDef, RAIL_SLEEP, move)) ? STATUS_RAIL_SLOW_PERCENT : 100;
    default:
        break;
    }

    rail = GetMoveStatusRail(battlerAtk, move, &isStatusMove);
    if (rail != NUM_STATUS_RAILS && CanProgressRail(battlerAtk, battlerDef, rail, move))
    {
        if (rail == RAIL_ELECTRIC && GetNextRailLevel(battlerDef, rail, isStatusMove) == 3)
            percent = 25; // Paralyzed: Speed / 4
        else if (rail == RAIL_CHILL && GetStatusRailLevel(battlerDef, RAIL_CHILL) == 0)
            percent = STATUS_RAIL_SLOW_PERCENT;
    }

    for (u32 i = 0; i < GetMoveAdditionalEffectCount(move); i++)
    {
        const struct AdditionalEffect *effect = GetMoveAdditionalEffectById(move, i);
        if (effect->self || (effect->moveEffect != MOVE_EFFECT_STAT_MINUS && effect->moveEffect != STAT_CHANGE_EFFECT_MINUS) || effect->speed == 0)
            continue;
        if ((effect->chance != 0 && effect->chance < 50) || !CanLowerStat(battlerAtk, battlerDef, gAiLogicData, STAT_SPEED))
            continue;
        percent = percent * ((effect->speed >= 2) ? 50 : 67) / 100;
    }
    return percent;
}

static bool32 SpeedControlFlips(enum BattlerId battler, enum BattlerId battlerDef, u32 percent)
{
    u32 speed = gAiLogicData->speedStats[battler];
    u32 defSpeed = gAiLogicData->speedStats[battlerDef];

    if (!IsBattlerAlive(battler))
        return FALSE;
    if (percent == 0) // Trick Room
        return speed < defSpeed;
    return speed <= defSpeed && speed > defSpeed * percent / 100;
}

// Score for slowing the target; *decisive when the flipped battler (the AI or its partner) then OHKOs the target,
// so the target has to switch out or go down.
static s32 GetSpeedControlBonus(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move, bool32 *decisive)
{
    u32 percent = GetSpeedControlPercent(battlerAtk, battlerDef, move);
    enum BattlerId partner = GetPartnerBattler(battlerAtk);
    bool32 flipsAi, flipsPartner;

    *decisive = FALSE;
    if (percent >= 100)
        return 0;

    flipsAi = SpeedControlFlips(battlerAtk, battlerDef, percent);
    flipsPartner = IsDoubleBattle() && SpeedControlFlips(partner, battlerDef, percent);
    if (!flipsAi && !flipsPartner)
        return (gAiLogicData->speedStats[battlerAtk] <= gAiLogicData->speedStats[battlerDef]) ? WEAK_EFFECT : NO_INCREASE;

    if (!CanAIFaintTarget(battlerAtk, battlerDef, 0)
     && ((flipsAi && GetBestNoOfHitsToKO(battlerAtk, battlerDef, AI_ATTACKING) == 1)
      || (flipsPartner && GetBestNoOfHitsToKO(partner, battlerDef, AI_ATTACKING) == 1)))
        *decisive = TRUE;
    return DECENT_EFFECT;
}

// ---- Reading the player's Dodge --------------------------------------------------------------------------

// What a Dodge would do to this move (the engine's rules: IsMoveFullyDodged / IsMoveHalvedByDodge, GetDodgeBypass).
static enum DodgeOutcome PredictDodgeOutcome(enum BattlerId battlerAtk, enum BattlerId battlerDef, u32 slot, enum Move move)
{
    enum DodgeBypass bypass = GetDodgeBypass(battlerAtk, slot);
    bool32 repeated = (move == gBattleStruct->moveUsedLastTurn[battlerAtk]);
    bool32 repeatTraps = repeated && bypass == DODGE_BYPASS_NONE;

    if (IsBattleMoveStatus(move))
        return repeatTraps ? DODGE_OUTCOME_MISS : DODGE_OUTCOME_UNAFFECTED;
    if (bypass == DODGE_BYPASS_IGNORED)
        return DODGE_OUTCOME_UNAFFECTED;
    if (bypass == DODGE_BYPASS_NEVER_FULL)
        return DODGE_OUTCOME_HALVED;
    if (repeatTraps || !AI_IsFaster(battlerAtk, battlerDef, move, MOVE_NONE, CONSIDER_PRIORITY))
        return DODGE_OUTCOME_MISS;
    return DODGE_OUTCOME_HALVED;
}

// A Dodge spot: the target can Dodge this turn and has a reason to (the AI threatens a KO, or is about to repeat
// last turn's hit).
static bool32 IsDodgeSpot(enum BattlerId battlerAtk, enum BattlerId battlerDef)
{
    enum Move lastMove = gBattleStruct->moveUsedLastTurn[battlerAtk];

    if (BattlerHasAi(battlerDef) || IsBattlerAlly(battlerAtk, battlerDef) || !IsBattlerAlive(battlerDef))
        return FALSE;
    if (IsInnateCommandBlockedByChoice(battlerDef)
     || gBattleStruct->battlerState[battlerDef].dodgeConsecutiveUses != 0
     || gBattleStruct->battlerState[battlerDef].currentStamina < GetDodgeCost(battlerDef))
        return FALSE;
    return CanAIFaintTarget(battlerAtk, battlerDef, 0)
        || (lastMove != MOVE_NONE && GetMovePower(lastMove) != 0 && IsBestDmgMove(battlerAtk, battlerDef, AI_ATTACKING, lastMove));
}

// Rolled once per turn and pair, at this player's own rate of Dodging in such spots this battle (starting at 50%).
static bool32 IsDodgeExpected(enum BattlerId battlerAtk, enum BattlerId battlerDef)
{
    u32 bit = 1u << (battlerAtk * MAX_BATTLERS_COUNT + battlerDef);

    if (!IsDodgeSpot(battlerAtk, battlerDef))
        return FALSE;
    if (!gAiLogicData->aiPredictionInProgress && !gAiLogicData->partnerMoveSimulation)
        gAiBattleData->dodgeSpotPending |= 1u << battlerDef;

    if (!(gAiLogicData->dodgeReadDone & bit))
    {
        u32 percent = (gAiBattleData->dodgesInSpots[battlerDef] + 1) * 100 / (gAiBattleData->dodgeSpots[battlerDef] + 2);
        gAiLogicData->dodgeReadDone |= bit;
        if (RandomPercentage(RNG_AI_FR_DODGE_READ, percent))
            gAiLogicData->dodgeExpected |= bit;
    }
    return (gAiLogicData->dodgeExpected & bit) != 0;
}

// End of turn: count this turn's Dodge spots and whether the battler actually Dodged in them.
void AI_RecordDodgeSpots(void)
{
    for (enum BattlerId battler = 0; battler < gBattlersCount; battler++)
    {
        if (!(gAiBattleData->dodgeSpotPending & (1u << battler)))
            continue;
        if (gAiBattleData->dodgeSpots[battler] >= 200) // keep the rate, forget the oldest weight
        {
            gAiBattleData->dodgeSpots[battler] /= 2;
            gAiBattleData->dodgesInSpots[battler] /= 2;
        }
        gAiBattleData->dodgeSpots[battler]++;
        if (gChosenActionByBattler[battler] == B_ACTION_DODGE)
            gAiBattleData->dodgesInSpots[battler]++;
    }
    gAiBattleData->dodgeSpotPending = 0;
}

static s32 GetDodgeReadBonus(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move, bool32 canKO)
{
    u32 slot = gAiThinkingStruct->movesetIndex;

    if (!IsDodgeExpected(battlerAtk, battlerDef))
        return 0;

    switch (PredictDodgeOutcome(battlerAtk, battlerDef, slot, move))
    {
    case DODGE_OUTCOME_MISS:
        return canKO ? 0 : -GOOD_EFFECT;
    case DODGE_OUTCOME_HALVED:
        // Halved but still a KO (a Signature Move into a faster dodger, say).
        if (AI_GetDamage(battlerAtk, battlerDef, slot, AI_ATTACKING, gAiLogicData) / 2 >= gBattleMons[battlerDef].hp)
            return GOOD_EFFECT;
        return canKO ? 0 : -WEAK_EFFECT;
    default:
        break;
    }

    if (GetMovePower(move) != 0)
        return DECENT_EFFECT; // lands in full through the Dodge
    switch (AI_GetBattlerMoveTargetType(battlerAtk, move))
    {
    case TARGET_USER:
    case TARGET_FIELD:
    case TARGET_OPPONENTS_FIELD:
    case TARGET_USER_AND_ALLY:
    case TARGET_ALL_BATTLERS:
        return DECENT_EFFECT; // a Dodging target wastes its turn: free setup / hazards / field
    default:
        return NO_INCREASE;
    }
}

// ---- Held items -------------------------------------------------------------------------------------------

static s32 GetHeldItemBonus(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move)
{
    // Rocky Helmet: a contact move that would knock the AI itself out.
    if (gAiLogicData->holdEffects[battlerDef] == HOLD_EFFECT_ROCKY_HELMET && GetMovePower(move) != 0
     && MoveMakesContact(move) && gAiLogicData->abilities[battlerAtk] != ABILITY_MAGIC_GUARD
     && gAiLogicData->holdEffects[battlerAtk] != HOLD_EFFECT_PROTECTIVE_PADS
     && gBattleMons[battlerAtk].maxHP / 6 >= gBattleMons[battlerAtk].hp)
        return -GOOD_EFFECT;
    return NO_INCREASE;
}

// ---- Entry points ------------------------------------------------------------------------------------------

s32 AI_FrTactics(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move, s32 score)
{
    u32 slot = gAiThinkingStruct->movesetIndex;
    bool32 canKO, decisive;
    s32 bonus = 0;

    if (IsTargetingPartner(battlerAtk, battlerDef) || IsBattlerAlly(battlerAtk, battlerDef))
        return score;

    canKO = GetMovePower(move) != 0 && CanIndexMoveFaintTarget(battlerAtk, battlerDef, slot, AI_ATTACKING);

    bonus += GetStatusApplicationBonus(battlerAtk, battlerDef, move);
    bonus += GetRailInteractionBonus(battlerAtk, battlerDef, move);
    bonus += GetDoublesStatusBonus(battlerAtk, battlerDef, move);
    bonus += GetDodgeReadBonus(battlerAtk, battlerDef, move, canKO);
    bonus += GetHeldItemBonus(battlerAtk, battlerDef, move);
    bonus += GetSpeedControlBonus(battlerAtk, battlerDef, move, &decisive);

    if (canKO && bonus < 0)
        bonus = 0;
    if (bonus > FR_TACTICS_MAX_BONUS)
        bonus = FR_TACTICS_MAX_BONUS;
    if (bonus < FR_TACTICS_MIN_BONUS)
        bonus = FR_TACTICS_MIN_BONUS;
    if (decisive)
        bonus = FR_TACTICS_DECISIVE;

    if (bonus != 0)
        ADJUST_SCORE(bonus);
    return score;
}

// After the damage comparison: the Signature Move (8 PP) and Itemized Move (4 PP) give way to an equally scored
// slot move unless they KO, so they are saved for when they are the best or the finishing option.
void AI_FrTacticsPostScore(enum BattlerId battlerAtk, enum BattlerId battlerDef)
{
    s32 bestSlotScore = 0;

    if (IsTargetingPartner(battlerAtk, battlerDef))
        return;

    for (u32 slot = 0; slot < MAX_MON_MOVES; slot++)
    {
        if (gAiThinkingStruct->score[slot] > bestSlotScore)
            bestSlotScore = gAiThinkingStruct->score[slot];
    }

    for (u32 slot = MAX_MON_MOVES; slot < AI_GetMoveSlotCount(battlerAtk); slot++)
    {
        enum Move move = AI_GetBattlerMoveAtSlot(battlerAtk, slot);

        if (gAiThinkingStruct->score[slot] <= 0 || gAiThinkingStruct->score[slot] != bestSlotScore)
            continue;
        if (GetMovePower(move) != 0 && CanIndexMoveFaintTarget(battlerAtk, battlerDef, slot, AI_ATTACKING))
            continue;
        gAiThinkingStruct->score[slot]--;
    }
}
