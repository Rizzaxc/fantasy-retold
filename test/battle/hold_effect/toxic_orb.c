#include "global.h"
#include "test/battle.h"

// Fantasy Retold: Toxic Orb lands Toxic (Lv2) through the status rail, once, without touching the
// holder's other lines (combat.md §6).

ASSUMPTIONS
{
    ASSUME(gItemsInfo[ITEM_TOXIC_ORB].holdEffect == HOLD_EFFECT_TOXIC_ORB);
}

SINGLE_BATTLE_TEST("Toxic Orb badly poisons its holder only once")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Item(ITEM_TOXIC_ORB); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN {}
        TURN {}
        TURN {}
    } SCENE {
        MESSAGE("Wobbuffet was badly poisoned!");
        STATUS_ICON(player, badPoison: TRUE);
        NONE_OF { MESSAGE("Wobbuffet was badly poisoned!"); }
    }
}

SINGLE_BATTLE_TEST("Toxic Orb doesn't cure the holder's other status lines")
{
    GIVEN {
        PLAYER(SPECIES_WOBBUFFET) { Item(ITEM_TOXIC_ORB); Status1(STATUS1_BURN); }
        OPPONENT(SPECIES_WOBBUFFET);
    } WHEN {
        TURN {}
        TURN {}
    } SCENE {
        MESSAGE("Wobbuffet was badly poisoned!");
    } THEN {
        EXPECT(player->status1 & STATUS1_TOXIC_POISON);
        EXPECT(player->status1 & STATUS1_BURN);
    }
}
