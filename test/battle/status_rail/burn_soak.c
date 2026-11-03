#include "global.h"
#include "test/battle.h"
#include "status_rail.h"

// Fantasy Retold: Water damage alone doesn't ease a Burn; only a Soak proc does, taking one stack off
// (combat.md §6). Ice still takes a stack off on every hit.

ASSUMPTIONS
{
    ASSUME(GetMoveType(MOVE_WATER_GUN) == TYPE_WATER);
    ASSUME(GetMoveNonVolatileStatus(MOVE_WILL_O_WISP) == MOVE_EFFECT_BURN);
}

SINGLE_BATTLE_TEST("Status rail: a Water hit without a Soak leaves a Burn alone")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_WILL_O_WISP); }
        TURN { MOVE(player, MOVE_WATER_GUN, WITH_RNG(RNG_STATUS_RAIL_SOAK, FALSE)); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_WATER_GUN, player);
        NONE_OF {
            MESSAGE("The opposing Wobbuffet's burn eased!");
            MESSAGE("The opposing Wobbuffet's burn was doused!");
        }
    } THEN {
        EXPECT_EQ(GetStatusRailLevel(B_BATTLER_1, RAIL_BURN), 2);
    }
}

SINGLE_BATTLE_TEST("Status rail: a Soak on a burned target takes one Burn stack off")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_WILL_O_WISP); }
        TURN { MOVE(player, MOVE_WATER_GUN, WITH_RNG(RNG_STATUS_RAIL_SOAK, TRUE)); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_WATER_GUN, player);
        MESSAGE("The opposing Wobbuffet's burn eased!");
        NONE_OF { MESSAGE("The opposing Wobbuffet got soaked!"); }
    } THEN {
        EXPECT_EQ(GetStatusRailLevel(B_BATTLER_1, RAIL_BURN), 1);
        EXPECT_EQ(GetStatusRailLevel(B_BATTLER_1, RAIL_SOAK), 0);
    }
}

SINGLE_BATTLE_TEST("Status rail: a Soak on a single Burn stack puts it out")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET) { Status1(STATUS1_BURN); }
    } WHEN {
        TURN { MOVE(player, MOVE_WATER_GUN, WITH_RNG(RNG_STATUS_RAIL_SOAK, TRUE)); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_WATER_GUN, player);
        MESSAGE("The opposing Wobbuffet's burn was doused!");
    } THEN {
        EXPECT_EQ(opponent->status1 & STATUS1_BURN, 0);
    }
}
