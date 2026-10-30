#include "global.h"
#include "test/battle.h"

AI_SINGLE_BATTLE_TEST("AI_FLAG_STAMINA_MANAGEMENT: tankier AI recuperates at low stamina")
{
  GIVEN
  {
    AI_FLAGS(AI_FLAG_CHECK_BAD_MOVE | AI_FLAG_TRY_TO_FAINT | AI_FLAG_CHECK_VIABILITY | AI_FLAG_STAMINA_MANAGEMENT);
    PLAYER(SPECIES_CHARIZARD) { Moves(MOVE_TACKLE); }
    OPPONENT(SPECIES_BLISSEY) { Moves(MOVE_TACKLE); }
  }
  WHEN
  {
    TURN
    {
      gBattleStruct->battlerState[B_BATTLER_1].currentStamina = GetBaseStamina(B_BATTLER_1) * RECUPERATE_STAMINA_PERCENT / 100;
      MOVE(player, MOVE_TACKLE);
      EXPECT(gChosenActionByBattler[B_BATTLER_1] == B_ACTION_RECUPERATE);
    }
  }
}

AI_SINGLE_BATTLE_TEST("AI_FLAG_STAMINA_MANAGEMENT: attacks a target at 0 stamina")
{
  GIVEN
  {
    AI_FLAGS(AI_FLAG_CHECK_BAD_MOVE | AI_FLAG_TRY_TO_FAINT | AI_FLAG_CHECK_VIABILITY | AI_FLAG_STAMINA_MANAGEMENT);
    PLAYER(SPECIES_CHARIZARD) { Moves(MOVE_TACKLE); }
    OPPONENT(SPECIES_BLISSEY) { Moves(MOVE_TACKLE); }
  }
  WHEN
  {
    TURN
    {
      gBattleStruct->battlerState[B_BATTLER_1].currentStamina = GetBaseStamina(B_BATTLER_1);
      gBattleStruct->battlerState[B_BATTLER_0].currentStamina = 0;
      MOVE(player, MOVE_TACKLE);
      EXPECT(gChosenActionByBattler[B_BATTLER_1] == B_ACTION_USE_MOVE);
      EXPECT_MOVE(opponent, MOVE_TACKLE);
    }
  }
}

AI_SINGLE_BATTLE_TEST("AI_FLAG_STAMINA_MANAGEMENT: tankier AI exhausts a nearly drained target instead of recuperating")
{
  GIVEN
  {
    AI_FLAGS(AI_FLAG_CHECK_BAD_MOVE | AI_FLAG_TRY_TO_FAINT | AI_FLAG_CHECK_VIABILITY | AI_FLAG_STAMINA_MANAGEMENT);
    PLAYER(SPECIES_CHARIZARD) { Moves(MOVE_TACKLE); }
    OPPONENT(SPECIES_BLISSEY) { Moves(MOVE_TACKLE); }
  }
  WHEN
  {
    TURN
    {
      gBattleStruct->battlerState[B_BATTLER_1].currentStamina = GetBaseStamina(B_BATTLER_1) * RECUPERATE_STAMINA_PERCENT / 100;
      gBattleStruct->battlerState[B_BATTLER_0].currentStamina = 2;
      MOVE(player, MOVE_TACKLE);
      EXPECT(gChosenActionByBattler[B_BATTLER_1] == B_ACTION_USE_MOVE);
      EXPECT_MOVE(opponent, MOVE_TACKLE);
    }
  }
}

AI_SINGLE_BATTLE_TEST("AI_FLAG_STAMINA_MANAGEMENT: AI presses a status on a slow threat instead of recuperating")
{
  GIVEN
  {
    AI_FLAGS(AI_FLAG_CHECK_BAD_MOVE | AI_FLAG_TRY_TO_FAINT | AI_FLAG_CHECK_VIABILITY | AI_FLAG_STAMINA_MANAGEMENT);
    PLAYER(SPECIES_WURMPLE) { Moves(MOVE_TACKLE); }
    OPPONENT(SPECIES_WURMPLE) { Moves(MOVE_TACKLE, MOVE_POISON_STING); }
  }
  WHEN
  {
    TURN
    {
      gBattleStruct->battlerState[B_BATTLER_1].currentStamina = GetBaseStamina(B_BATTLER_1) * RECUPERATE_STAMINA_PERCENT / 100;
      MOVE(player, MOVE_TACKLE);
      EXPECT(gChosenActionByBattler[B_BATTLER_1] == B_ACTION_USE_MOVE);
      EXPECT_MOVE(opponent, MOVE_POISON_STING);
    }
  }
}

AI_SINGLE_BATTLE_TEST("AI_FLAG_STAMINA_MANAGEMENT: AI trades hits when both battlers are drained")
{
  GIVEN
  {
    AI_FLAGS(AI_FLAG_CHECK_BAD_MOVE | AI_FLAG_TRY_TO_FAINT | AI_FLAG_CHECK_VIABILITY | AI_FLAG_STAMINA_MANAGEMENT);
    PLAYER(SPECIES_WURMPLE) { Moves(MOVE_TACKLE); }
    OPPONENT(SPECIES_WURMPLE) { Moves(MOVE_TACKLE); }
  }
  WHEN
  {
    TURN
    {
      gBattleStruct->battlerState[B_BATTLER_1].currentStamina = GetBaseStamina(B_BATTLER_1) * RECUPERATE_STAMINA_PERCENT / 100;
      gBattleStruct->battlerState[B_BATTLER_0].currentStamina = GetBaseStamina(B_BATTLER_0) * 30 / 100;
      MOVE(player, MOVE_TACKLE);
      EXPECT(gChosenActionByBattler[B_BATTLER_1] == B_ACTION_USE_MOVE);
      EXPECT_MOVE(opponent, MOVE_TACKLE);
    }
  }
}

AI_SINGLE_BATTLE_TEST("AI_FLAG_STAMINA_MANAGEMENT: AI with a small pool attacks instead of looping Recuperate")
{
  GIVEN
  {
    AI_FLAGS(AI_FLAG_CHECK_BAD_MOVE | AI_FLAG_TRY_TO_FAINT | AI_FLAG_CHECK_VIABILITY | AI_FLAG_STAMINA_MANAGEMENT);
    PLAYER(SPECIES_TREECKO) { Level(5); Moves(MOVE_POUND); }
    OPPONENT(SPECIES_TORCHIC) { Level(5); Moves(MOVE_SCRATCH, MOVE_EMBER); }
  }
  WHEN
  {
    TURN
    {
      gBattleStruct->battlerState[B_BATTLER_1].currentStamina = 2;
      MOVE(player, MOVE_POUND);
      EXPECT(gChosenActionByBattler[B_BATTLER_1] == B_ACTION_USE_MOVE);
      EXPECT_MOVE(opponent, MOVE_EMBER);
    }
  }
}
