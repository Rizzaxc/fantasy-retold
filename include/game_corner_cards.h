#ifndef GUARD_GAME_CORNER_CARDS_H
#define GUARD_GAME_CORNER_CARDS_H

// Shared playing cards for the Game Corner card games (design.md §2, Game Corner Minigames).
// Card id = suit * 13 + rank; suits Hearts, Diamonds, Clubs, Spades;
// rank 0 = '2' ... 8 = '10', 9 = J, 10 = Q, 11 = K, 12 = A.

#define GC_DECK_SIZE       52
#define GC_RANKS           13
#define GC_CARD_FACE_DOWN  0xFF
#define GC_CARD_GFX_SIZE   0x400 // one 32x64 card
#define GC_CARD_RANK(card) ((card) % GC_RANKS)
#define GC_CARD_SUIT(card) ((card) / GC_RANKS)
#define GC_RANK_JACK       9
#define GC_RANK_ACE        12

extern const u32 gCards_2_Hearts_Gfx[];
extern const u32 gCards_3_Hearts_Gfx[];
extern const u32 gCards_4_Hearts_Gfx[];
extern const u32 gCards_5_Hearts_Gfx[];
extern const u32 gCards_6_Hearts_Gfx[];
extern const u32 gCards_7_Hearts_Gfx[];
extern const u32 gCards_8_Hearts_Gfx[];
extern const u32 gCards_9_Hearts_Gfx[];
extern const u32 gCards_10_Hearts_Gfx[];
extern const u32 gCards_J_Hearts_Gfx[];
extern const u32 gCards_Q_Hearts_Gfx[];
extern const u32 gCards_K_Hearts_Gfx[];
extern const u32 gCards_A_Hearts_Gfx[];
extern const u32 gCards_2_Diamonds_Gfx[];
extern const u32 gCards_3_Diamonds_Gfx[];
extern const u32 gCards_4_Diamonds_Gfx[];
extern const u32 gCards_5_Diamonds_Gfx[];
extern const u32 gCards_6_Diamonds_Gfx[];
extern const u32 gCards_7_Diamonds_Gfx[];
extern const u32 gCards_8_Diamonds_Gfx[];
extern const u32 gCards_9_Diamonds_Gfx[];
extern const u32 gCards_10_Diamonds_Gfx[];
extern const u32 gCards_J_Diamonds_Gfx[];
extern const u32 gCards_Q_Diamonds_Gfx[];
extern const u32 gCards_K_Diamonds_Gfx[];
extern const u32 gCards_A_Diamonds_Gfx[];
extern const u32 gCards_2_Clubs_Gfx[];
extern const u32 gCards_3_Clubs_Gfx[];
extern const u32 gCards_4_Clubs_Gfx[];
extern const u32 gCards_5_Clubs_Gfx[];
extern const u32 gCards_6_Clubs_Gfx[];
extern const u32 gCards_7_Clubs_Gfx[];
extern const u32 gCards_8_Clubs_Gfx[];
extern const u32 gCards_9_Clubs_Gfx[];
extern const u32 gCards_10_Clubs_Gfx[];
extern const u32 gCards_J_Clubs_Gfx[];
extern const u32 gCards_Q_Clubs_Gfx[];
extern const u32 gCards_K_Clubs_Gfx[];
extern const u32 gCards_A_Clubs_Gfx[];
extern const u32 gCards_2_Spades_Gfx[];
extern const u32 gCards_3_Spades_Gfx[];
extern const u32 gCards_4_Spades_Gfx[];
extern const u32 gCards_5_Spades_Gfx[];
extern const u32 gCards_6_Spades_Gfx[];
extern const u32 gCards_7_Spades_Gfx[];
extern const u32 gCards_8_Spades_Gfx[];
extern const u32 gCards_9_Spades_Gfx[];
extern const u32 gCards_10_Spades_Gfx[];
extern const u32 gCards_J_Spades_Gfx[];
extern const u32 gCards_Q_Spades_Gfx[];
extern const u32 gCards_K_Spades_Gfx[];
extern const u32 gCards_A_Spades_Gfx[];
extern const u32 gGameCornerCardFaceDown_Gfx[];
extern const u16 gGameCornerCards_Pal[];

// Loads a card's tiles (GC_CARD_FACE_DOWN for the back) under tileTag, replacing what was there.
void GameCornerCards_LoadGfx(u8 card, u16 tileTag);
u8 GameCornerCards_CreateSprite(u16 tileTag, u16 paletteTag, s16 x, s16 y, u8 subpriority);
void GameCornerCards_Shuffle(u8 *deck); // fills deck[GC_DECK_SIZE] with a shuffled deck

#endif // GUARD_GAME_CORNER_CARDS_H
