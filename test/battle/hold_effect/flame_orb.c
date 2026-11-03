#include "global.h"
#include "test/battle.h"

// Fantasy Retold: Flame Orb lands Burn x2 (Lv2) through the status rail, once, without touching the
// holder's other lines (combat.md §6).

ASSUMPTIONS
{
    ASSUME(gItemsInfo[ITEM_FLAME_ORB].holdEffect == HOLD_EFFECT_FLAME_ORB);
}

SINGLE_BATTLE_TEST("Flame Orb burns its holder only once")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Item(ITEM_FLAME_ORB); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN {}
        TURN {}
        TURN {}
    } SCENE {
        MESSAGE("Wobbuffet was burned!");
        STATUS_ICON(player, burn: TRUE);
        NONE_OF { MESSAGE("Wobbuffet was burned!"); }
    }
}

SINGLE_BATTLE_TEST("Flame Orb's burn counts for Guts", s16 damage)
{
    enum Item item;
    PARAMETRIZE { item = ITEM_NONE; }
    PARAMETRIZE { item = ITEM_FLAME_ORB; }
    GIVEN {
        ASSUME(GetMoveCategory(MOVE_SCRATCH) == DAMAGE_CATEGORY_PHYSICAL);
        PLAYER(SPECIES_URSARING) { Ability(ABILITY_GUTS); Item(item); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN {}
        TURN { MOVE(player, MOVE_SCRATCH); }
    } SCENE {
        HP_BAR(opponent, captureDamage: &results[i].damage);
    } FINALLY {
        EXPECT_MUL_EQ(results[0].damage, Q_4_12(1.5), results[1].damage);
    }
}

SINGLE_BATTLE_TEST("Flame Orb doesn't cure the holder's other status lines")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Item(ITEM_FLAME_ORB); Status1(STATUS1_TOXIC_POISON); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN {}
        TURN {}
    } SCENE {
        MESSAGE("Wobbuffet was burned!");
    } THEN {
        EXPECT(player->status1 & STATUS1_BURN);
        EXPECT(player->status1 & STATUS1_TOXIC_POISON);
    }
}
