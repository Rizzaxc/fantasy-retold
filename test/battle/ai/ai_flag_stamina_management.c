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
