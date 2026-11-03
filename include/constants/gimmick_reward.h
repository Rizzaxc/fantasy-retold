#ifndef GUARD_CONSTANTS_GIMMICK_REWARD_H
#define GUARD_CONSTANTS_GIMMICK_REWARD_H

// Fantasy Retold: Mega Ring / Z-Ring from Wattson, Birch's starter gift, Game Corner prizes (design.md §2).

// VAR_BIRCH_GIMMICK_STATE
#define BIRCH_GIMMICK_NONE      0 // no ring yet
#define BIRCH_GIMMICK_RING      1 // ring received, Birch hasn't called yet
#define BIRCH_GIMMICK_CALLED    2 // Birch called; the gift waits in the lab
#define BIRCH_GIMMICK_GIFT_DONE 3

// Game Corner stock: VAR_0x8004 = category
#define GAME_CORNER_STOCK_MEGA_STONE 0
#define GAME_CORNER_STOCK_Z_CRYSTAL  1
#define GAME_CORNER_STOCK_SLOTS      3

// Game Corner prices
#define GAME_CORNER_MEGA_STONE_COINS 4000
#define GAME_CORNER_Z_CRYSTAL_COINS  3000

#endif // GUARD_CONSTANTS_GIMMICK_REWARD_H
