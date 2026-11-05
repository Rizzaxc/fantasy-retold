#include "global.h"
#include "game_corner_cards.h"
#include "decompress.h"
#include "random.h"
#include "sprite.h"

// Fantasy Retold: the playing cards shared by the Game Corner card games
// (Blackjack, Video Poker; design.md §2, Game Corner Minigames). Art from
// heyopc/pokeemerald-gamecorner-expansion.

const u32 gCards_2_Hearts_Gfx[] = INCBIN_U32("graphics/blackjack/cards/hearts/cards_hearts_2.4bpp.lz");
const u32 gCards_3_Hearts_Gfx[] = INCBIN_U32("graphics/blackjack/cards/hearts/cards_hearts_3.4bpp.lz");
const u32 gCards_4_Hearts_Gfx[] = INCBIN_U32("graphics/blackjack/cards/hearts/cards_hearts_4.4bpp.lz");
const u32 gCards_5_Hearts_Gfx[] = INCBIN_U32("graphics/blackjack/cards/hearts/cards_hearts_5.4bpp.lz");
const u32 gCards_6_Hearts_Gfx[] = INCBIN_U32("graphics/blackjack/cards/hearts/cards_hearts_6.4bpp.lz");
const u32 gCards_7_Hearts_Gfx[] = INCBIN_U32("graphics/blackjack/cards/hearts/cards_hearts_7.4bpp.lz");
const u32 gCards_8_Hearts_Gfx[] = INCBIN_U32("graphics/blackjack/cards/hearts/cards_hearts_8.4bpp.lz");
const u32 gCards_9_Hearts_Gfx[] = INCBIN_U32("graphics/blackjack/cards/hearts/cards_hearts_9.4bpp.lz");
const u32 gCards_10_Hearts_Gfx[] = INCBIN_U32("graphics/blackjack/cards/hearts/cards_hearts_10.4bpp.lz");
const u32 gCards_J_Hearts_Gfx[] = INCBIN_U32("graphics/blackjack/cards/hearts/cards_hearts_j.4bpp.lz");
const u32 gCards_Q_Hearts_Gfx[] = INCBIN_U32("graphics/blackjack/cards/hearts/cards_hearts_q.4bpp.lz");
const u32 gCards_K_Hearts_Gfx[] = INCBIN_U32("graphics/blackjack/cards/hearts/cards_hearts_k.4bpp.lz");
const u32 gCards_A_Hearts_Gfx[] = INCBIN_U32("graphics/blackjack/cards/hearts/cards_hearts_a.4bpp.lz");
const u32 gCards_2_Diamonds_Gfx[] = INCBIN_U32("graphics/blackjack/cards/diamonds/cards_diamonds_2.4bpp.lz");
const u32 gCards_3_Diamonds_Gfx[] = INCBIN_U32("graphics/blackjack/cards/diamonds/cards_diamonds_3.4bpp.lz");
const u32 gCards_4_Diamonds_Gfx[] = INCBIN_U32("graphics/blackjack/cards/diamonds/cards_diamonds_4.4bpp.lz");
const u32 gCards_5_Diamonds_Gfx[] = INCBIN_U32("graphics/blackjack/cards/diamonds/cards_diamonds_5.4bpp.lz");
const u32 gCards_6_Diamonds_Gfx[] = INCBIN_U32("graphics/blackjack/cards/diamonds/cards_diamonds_6.4bpp.lz");
const u32 gCards_7_Diamonds_Gfx[] = INCBIN_U32("graphics/blackjack/cards/diamonds/cards_diamonds_7.4bpp.lz");
const u32 gCards_8_Diamonds_Gfx[] = INCBIN_U32("graphics/blackjack/cards/diamonds/cards_diamonds_8.4bpp.lz");
const u32 gCards_9_Diamonds_Gfx[] = INCBIN_U32("graphics/blackjack/cards/diamonds/cards_diamonds_9.4bpp.lz");
const u32 gCards_10_Diamonds_Gfx[] = INCBIN_U32("graphics/blackjack/cards/diamonds/cards_diamonds_10.4bpp.lz");
const u32 gCards_J_Diamonds_Gfx[] = INCBIN_U32("graphics/blackjack/cards/diamonds/cards_diamonds_j.4bpp.lz");
const u32 gCards_Q_Diamonds_Gfx[] = INCBIN_U32("graphics/blackjack/cards/diamonds/cards_diamonds_q.4bpp.lz");
const u32 gCards_K_Diamonds_Gfx[] = INCBIN_U32("graphics/blackjack/cards/diamonds/cards_diamonds_k.4bpp.lz");
const u32 gCards_A_Diamonds_Gfx[] = INCBIN_U32("graphics/blackjack/cards/diamonds/cards_diamonds_a.4bpp.lz");
const u32 gCards_2_Clubs_Gfx[] = INCBIN_U32("graphics/blackjack/cards/clubs/cards_clubs_2.4bpp.lz");
const u32 gCards_3_Clubs_Gfx[] = INCBIN_U32("graphics/blackjack/cards/clubs/cards_clubs_3.4bpp.lz");
const u32 gCards_4_Clubs_Gfx[] = INCBIN_U32("graphics/blackjack/cards/clubs/cards_clubs_4.4bpp.lz");
const u32 gCards_5_Clubs_Gfx[] = INCBIN_U32("graphics/blackjack/cards/clubs/cards_clubs_5.4bpp.lz");
const u32 gCards_6_Clubs_Gfx[] = INCBIN_U32("graphics/blackjack/cards/clubs/cards_clubs_6.4bpp.lz");
const u32 gCards_7_Clubs_Gfx[] = INCBIN_U32("graphics/blackjack/cards/clubs/cards_clubs_7.4bpp.lz");
const u32 gCards_8_Clubs_Gfx[] = INCBIN_U32("graphics/blackjack/cards/clubs/cards_clubs_8.4bpp.lz");
const u32 gCards_9_Clubs_Gfx[] = INCBIN_U32("graphics/blackjack/cards/clubs/cards_clubs_9.4bpp.lz");
const u32 gCards_10_Clubs_Gfx[] = INCBIN_U32("graphics/blackjack/cards/clubs/cards_clubs_10.4bpp.lz");
const u32 gCards_J_Clubs_Gfx[] = INCBIN_U32("graphics/blackjack/cards/clubs/cards_clubs_j.4bpp.lz");
const u32 gCards_Q_Clubs_Gfx[] = INCBIN_U32("graphics/blackjack/cards/clubs/cards_clubs_q.4bpp.lz");
const u32 gCards_K_Clubs_Gfx[] = INCBIN_U32("graphics/blackjack/cards/clubs/cards_clubs_k.4bpp.lz");
const u32 gCards_A_Clubs_Gfx[] = INCBIN_U32("graphics/blackjack/cards/clubs/cards_clubs_a.4bpp.lz");
const u32 gCards_2_Spades_Gfx[] = INCBIN_U32("graphics/blackjack/cards/spades/cards_spades_2.4bpp.lz");
const u32 gCards_3_Spades_Gfx[] = INCBIN_U32("graphics/blackjack/cards/spades/cards_spades_3.4bpp.lz");
const u32 gCards_4_Spades_Gfx[] = INCBIN_U32("graphics/blackjack/cards/spades/cards_spades_4.4bpp.lz");
const u32 gCards_5_Spades_Gfx[] = INCBIN_U32("graphics/blackjack/cards/spades/cards_spades_5.4bpp.lz");
const u32 gCards_6_Spades_Gfx[] = INCBIN_U32("graphics/blackjack/cards/spades/cards_spades_6.4bpp.lz");
const u32 gCards_7_Spades_Gfx[] = INCBIN_U32("graphics/blackjack/cards/spades/cards_spades_7.4bpp.lz");
const u32 gCards_8_Spades_Gfx[] = INCBIN_U32("graphics/blackjack/cards/spades/cards_spades_8.4bpp.lz");
const u32 gCards_9_Spades_Gfx[] = INCBIN_U32("graphics/blackjack/cards/spades/cards_spades_9.4bpp.lz");
const u32 gCards_10_Spades_Gfx[] = INCBIN_U32("graphics/blackjack/cards/spades/cards_spades_10.4bpp.lz");
const u32 gCards_J_Spades_Gfx[] = INCBIN_U32("graphics/blackjack/cards/spades/cards_spades_j.4bpp.lz");
const u32 gCards_Q_Spades_Gfx[] = INCBIN_U32("graphics/blackjack/cards/spades/cards_spades_q.4bpp.lz");
const u32 gCards_K_Spades_Gfx[] = INCBIN_U32("graphics/blackjack/cards/spades/cards_spades_k.4bpp.lz");
const u32 gCards_A_Spades_Gfx[] = INCBIN_U32("graphics/blackjack/cards/spades/cards_spades_a.4bpp.lz");

const u32 gGameCornerCardFaceDown_Gfx[] = INCBIN_U32("graphics/blackjack/facedown.4bpp.lz");
const u16 gGameCornerCards_Pal[] = INCBIN_U16("graphics/blackjack/cards.gbapal");

// Indexed by card id: suit * 13 + rank (see game_corner_cards.h)
static const u32 *const sCardGfx[GC_DECK_SIZE] =
{
    gCards_2_Hearts_Gfx,
    gCards_3_Hearts_Gfx,
    gCards_4_Hearts_Gfx,
    gCards_5_Hearts_Gfx,
    gCards_6_Hearts_Gfx,
    gCards_7_Hearts_Gfx,
    gCards_8_Hearts_Gfx,
    gCards_9_Hearts_Gfx,
    gCards_10_Hearts_Gfx,
    gCards_J_Hearts_Gfx,
    gCards_Q_Hearts_Gfx,
    gCards_K_Hearts_Gfx,
    gCards_A_Hearts_Gfx,
    gCards_2_Diamonds_Gfx,
    gCards_3_Diamonds_Gfx,
    gCards_4_Diamonds_Gfx,
    gCards_5_Diamonds_Gfx,
    gCards_6_Diamonds_Gfx,
    gCards_7_Diamonds_Gfx,
    gCards_8_Diamonds_Gfx,
    gCards_9_Diamonds_Gfx,
    gCards_10_Diamonds_Gfx,
    gCards_J_Diamonds_Gfx,
    gCards_Q_Diamonds_Gfx,
    gCards_K_Diamonds_Gfx,
    gCards_A_Diamonds_Gfx,
    gCards_2_Clubs_Gfx,
    gCards_3_Clubs_Gfx,
    gCards_4_Clubs_Gfx,
    gCards_5_Clubs_Gfx,
    gCards_6_Clubs_Gfx,
    gCards_7_Clubs_Gfx,
    gCards_8_Clubs_Gfx,
    gCards_9_Clubs_Gfx,
    gCards_10_Clubs_Gfx,
    gCards_J_Clubs_Gfx,
    gCards_Q_Clubs_Gfx,
    gCards_K_Clubs_Gfx,
    gCards_A_Clubs_Gfx,
    gCards_2_Spades_Gfx,
    gCards_3_Spades_Gfx,
    gCards_4_Spades_Gfx,
    gCards_5_Spades_Gfx,
    gCards_6_Spades_Gfx,
    gCards_7_Spades_Gfx,
    gCards_8_Spades_Gfx,
    gCards_9_Spades_Gfx,
    gCards_10_Spades_Gfx,
    gCards_J_Spades_Gfx,
    gCards_Q_Spades_Gfx,
    gCards_K_Spades_Gfx,
    gCards_A_Spades_Gfx,
};

static const struct OamData sOamData_Card =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(32x64),
    .size = SPRITE_SIZE(32x64),
    .priority = 1,
};

void GameCornerCards_LoadGfx(u8 card, u16 tileTag)
{
    struct CompressedSpriteSheet sheet;

    FreeSpriteTilesByTag(tileTag);
    sheet.data = (card < GC_DECK_SIZE) ? sCardGfx[card] : gGameCornerCardFaceDown_Gfx;
    sheet.size = GC_CARD_GFX_SIZE;
    sheet.tag = tileTag;
    LoadCompressedSpriteSheet(&sheet);
}

u8 GameCornerCards_CreateSprite(u16 tileTag, u16 paletteTag, s16 x, s16 y, u8 subpriority)
{
    struct SpriteTemplate template =
    {
        .tileTag = tileTag,
        .paletteTag = paletteTag,
        .oam = &sOamData_Card,
        .anims = gDummySpriteAnimTable,
        .images = NULL,
        .affineAnims = gDummySpriteAffineAnimTable,
        .callback = SpriteCallbackDummy,
    };
    return CreateSprite(&template, x, y, subpriority);
}

void GameCornerCards_Shuffle(u8 *deck)
{
    u32 i;

    for (i = 0; i < GC_DECK_SIZE; i++)
        deck[i] = i;
    for (i = GC_DECK_SIZE - 1; i > 0; i--)
    {
        u32 j = Random() % (i + 1);
        u8 tmp = deck[i];
        deck[i] = deck[j];
        deck[j] = tmp;
    }
}
