#include "global.h"
#include "test/battle.h"

// Fantasy Retold: Poison -> Toxic status rail (design.md §6). The rail in PartyState is authoritative;
// the legacy status1 bits are a read-mirror (regular poison -> "poison" icon; Toxic -> "badPoison" icon).
// - Attacking-move secondary  => Level 1 (regular Poison, healable 1/8 chip)
// - Status move               => Level 2 (Toxic, escalating + unhealable / logged as Trauma)
// - Reapplying advances Lv1 -> Lv2
// - Level 1 clears on switch-out; Level 2+ persists across switches.

ASSUMPTIONS
{
    ASSUME(MoveHasAdditionalEffect(MOVE_POISON_STING, MOVE_EFFECT_POISON) == TRUE);
    ASSUME(GetMoveEffect(MOVE_TOXIC) == EFFECT_NON_VOLATILE_STATUS);
    ASSUME(GetMoveNonVolatileStatus(MOVE_TOXIC) == MOVE_EFFECT_TOXIC);
}

SINGLE_BATTLE_TEST("Status rail: an attacking move's poison secondary applies Level 1 (regular poison)")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_POISON_STING); }
        TURN {}
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_POISON_STING, player);
        ANIMATION(ANIM_TYPE_STATUS, B_ANIM_STATUS_PSN, opponent);
        STATUS_ICON(opponent, poison: TRUE); // Lv1 -> regular poison icon, not badPoison
    } THEN {
        EXPECT_EQ(opponent->status1 & STATUS1_POISON, STATUS1_POISON);
        EXPECT_EQ(opponent->status1 & STATUS1_TOXIC_POISON, 0);
    }
}

SINGLE_BATTLE_TEST("Status rail: a status move applies Level 2 (Toxic) directly")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_TOXIC); }
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_TOXIC, player);
        STATUS_ICON(opponent, badPoison: TRUE); // Lv2 -> badPoison icon
    } THEN {
        EXPECT_EQ(opponent->status1 & STATUS1_TOXIC_POISON, STATUS1_TOXIC_POISON);
    }
}

SINGLE_BATTLE_TEST("Status rail: reapplying poison advances Level 1 -> Level 2 (Toxic)")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_POISON_STING); } // Lv1 regular poison
        TURN { MOVE(player, MOVE_POISON_STING); } // reapply -> Lv2 Toxic
    } SCENE {
        STATUS_ICON(opponent, poison: TRUE);    // turn 1: forces the secondary to land -> Lv1
        STATUS_ICON(opponent, badPoison: TRUE); // turn 2: reapplication advances -> Lv2 Toxic
    } THEN {
        EXPECT_EQ(opponent->status1 & STATUS1_TOXIC_POISON, STATUS1_TOXIC_POISON);
        EXPECT_EQ(opponent->status1 & STATUS1_POISON, 0);
    }
}

SINGLE_BATTLE_TEST("Status rail: Level 1 poison clears when the mon switches out")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WYNAUT);
    } WHEN {
        TURN { MOVE(player, MOVE_POISON_STING); }   // poison opponent slot 0 (Lv1)
        TURN { MOVE(player, MOVE_CELEBRATE); SWITCH(opponent, 1); } // slot 0 leaves -> Lv1 drops
        TURN { MOVE(player, MOVE_CELEBRATE); SWITCH(opponent, 0); } // poisoned mon returns, now clean
    } SCENE {
        STATUS_ICON(opponent, poison: TRUE); // force the turn-1 secondary to actually land
    } THEN {
        EXPECT_EQ(opponent->status1 & STATUS1_PSN_ANY, 0);
    }
}

SINGLE_BATTLE_TEST("Status rail: Level 2 Toxic persists across a switch")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WYNAUT);
    } WHEN {
        TURN { MOVE(player, MOVE_TOXIC); }          // Toxic opponent slot 0 (Lv2)
        TURN { MOVE(player, MOVE_CELEBRATE); SWITCH(opponent, 1); }
        TURN { MOVE(player, MOVE_CELEBRATE); SWITCH(opponent, 0); } // returns still badly poisoned
    } THEN {
        EXPECT_EQ(opponent->status1 & STATUS1_TOXIC_POISON, STATUS1_TOXIC_POISON);
    }
}

SINGLE_BATTLE_TEST("Status rail: Toxic damage is unhealable (logged as Trauma)")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN { MOVE(player, MOVE_TOXIC); }
        TURN {} // first escalating tick
        TURN {} // second, larger tick
    } SCENE {
        ANIMATION(ANIM_TYPE_MOVE, MOVE_TOXIC, player);
        STATUS_ICON(opponent, badPoison: TRUE);
    } THEN {
        u32 battler = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
        // The Toxic ticks lowered the effective max HP — i.e. they were logged as (unhealable) Trauma.
        EXPECT_GT(GetBattlerPartyState(battler)->trauma, 0);
        EXPECT_LT(GetBattlerEffectiveMaxHP(battler), opponent->maxHP);
    }
}

SINGLE_BATTLE_TEST("Status rail: Poison- and Steel-types remain immune to the rail")
{
    u32 species;
    PARAMETRIZE { species = SPECIES_NIDORAN_M; } // Poison
    PARAMETRIZE { species = SPECIES_REGISTEEL; } // Steel
    GIVEN {
        ASSUME(GetSpeciesType(SPECIES_NIDORAN_M, 0) == TYPE_POISON);
        ASSUME(GetSpeciesType(SPECIES_REGISTEEL, 0) == TYPE_STEEL);
        PLAYER(SPECIES_WOBBUFFET);
        OPPONENT(species);
    } WHEN {
        TURN { MOVE(player, MOVE_TOXIC, hit: FALSE); }
    } THEN {
        EXPECT_EQ(opponent->status1 & STATUS1_PSN_ANY, 0);
    }
}
