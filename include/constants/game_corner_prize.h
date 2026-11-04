#ifndef GUARD_CONSTANTS_GAME_CORNER_PRIZE_H
#define GUARD_CONSTANTS_GAME_CORNER_PRIZE_H

// Fantasy Retold: the Game Corner's prize rows, which replace its TMs (design.md §2, Game Corner Prizes).

#define GAME_CORNER_PRIZE_ROWS 5

// Row categories, each rolled evenly
#define GAME_CORNER_PRIZE_SPECIES_ITEM 0
#define GAME_CORNER_PRIZE_HELD_ITEM    1
#define GAME_CORNER_PRIZE_STAT_ITEM    2
#define GAME_CORNER_PRIZE_EGG          3
#define GAME_CORNER_PRIZE_CATEGORIES   4

// Coin prices
#define GAME_CORNER_SPECIES_ITEM_COINS 2000
#define GAME_CORNER_HELD_ITEM_COINS    3000
#define GAME_CORNER_STAT_ITEM_COINS    3000
#define GAME_CORNER_EGG_COINS          4000

// GameCornerPrize_BufferOffer
#define GAME_CORNER_OFFER_SOLD 0
#define GAME_CORNER_OFFER_ITEM 1
#define GAME_CORNER_OFFER_EGG  2

#endif // GUARD_CONSTANTS_GAME_CORNER_PRIZE_H
