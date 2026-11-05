#ifndef GUARD_CONSTANTS_GAME_CORNER_MINIGAMES_H
#define GUARD_CONSTANTS_GAME_CORNER_MINIGAMES_H

// Fantasy Retold: Game Corner minigames (design.md §2, Game Corner Minigames).
// Tuned so the games can be farmed for coins, the card games especially.

// Card games (Blackjack, Video Poker): the stake is picked in steps.
#define GC_CARD_STAKE_MIN  100
#define GC_CARD_STAKE_MAX  500
#define GC_CARD_STAKE_STEP 50

// Blackjack payouts, as numerator / denominator of the stake (net win).
#define GC_BLACKJACK_WIN_NUM     3 // a won hand pays 3:2
#define GC_BLACKJACK_WIN_DEN     2
#define GC_BLACKJACK_NATURAL_NUM 2 // a natural blackjack pays 2:1
#define GC_BLACKJACK_NATURAL_DEN 1

// Video Poker (Jacks or Better): total returned, x the stake.
#define GC_POKER_PAY_ROYAL_FLUSH    250
#define GC_POKER_PAY_STRAIGHT_FLUSH 60
#define GC_POKER_PAY_FOUR_KIND      30
#define GC_POKER_PAY_FULL_HOUSE     12
#define GC_POKER_PAY_FLUSH          8
#define GC_POKER_PAY_STRAIGHT       6
#define GC_POKER_PAY_THREE_KIND     5
#define GC_POKER_PAY_TWO_PAIR       2
#define GC_POKER_PAY_JACKS_OR_BETTER 1

// Arcade games: entry fee, and what a run pays.
#define GC_ARCADE_ENTRY_FEE        100
#define GC_SNAKE_COINS_PER_BERRY   10
#define GC_FLAPPY_COINS_PER_PIPE   10
#define GC_STACKER_PRIZE_MINOR     150  // cleared row 6
#define GC_STACKER_PRIZE_MAJOR     500  // cleared row 7
#define GC_STACKER_PRIZE_TOP       1500 // cleared the top row

// Arcade game ids, for MauvilleCity_GameCorner_EventScript_Arcade (VAR_0x8004)
#define GC_ARCADE_SNAKE         0
#define GC_ARCADE_FLAPPY        1
#define GC_ARCADE_BLOCK_STACKER 2

#endif // GUARD_CONSTANTS_GAME_CORNER_MINIGAMES_H
