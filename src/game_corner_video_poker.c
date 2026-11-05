#include "global.h"
#include "game_corner_video_poker.h"
#include "game_corner_cards.h"
#include "bg.h"
#include "coins.h"
#include "gpu_regs.h"
#include "main.h"
#include "malloc.h"
#include "menu.h"
#include "menu_helpers.h"
#include "overworld.h"
#include "palette.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/coins.h"
#include "constants/game_corner_minigames.h"
#include "constants/rgb.h"
#include "constants/songs.h"

// Fantasy Retold: Video Poker (Jacks or Better), the Game Corner's second card
// game (design.md §2, Game Corner Minigames). Built on the shared Blackjack cards.
//
// Pick a stake (GC_CARD_STAKE_MIN-MAX), deal 5 cards, hold any of them, draw
// once, and the final hand pays from the GC_POKER_PAY_* table.

#define HAND_SIZE 5

enum
{
    POKER_HAND_NOTHING,
    POKER_HAND_JACKS_OR_BETTER,
    POKER_HAND_TWO_PAIR,
    POKER_HAND_THREE_KIND,
    POKER_HAND_STRAIGHT,
    POKER_HAND_FLUSH,
    POKER_HAND_FULL_HOUSE,
    POKER_HAND_FOUR_KIND,
    POKER_HAND_STRAIGHT_FLUSH,
    POKER_HAND_ROYAL_FLUSH,
    POKER_HAND_COUNT,
};

enum
{
    STATE_FADE_IN,
    STATE_BET,
    STATE_HOLD,
    STATE_RESULT,
    STATE_FADE_OUT,
    STATE_EXIT,
};

enum
{
    WIN_PAYTABLE,
    WIN_HOLD,
    WIN_INFO,
};

#define TAG_CARD_PAL    0x5A00
#define TAG_CARD_TILES  0x5A00 // + slot

#define CARD_Y          88
#define CARD_X(slot)    (40 + (slot) * 40)

struct VideoPoker
{
    u8 state;
    u8 cursor;
    u8 deckPos;
    u8 lastHand;
    u16 stake;
    u32 lastPayout;
    bool8 held[HAND_SIZE];
    u8 hand[HAND_SIZE];
    u8 cardSpriteIds[HAND_SIZE];
    u8 deck[GC_DECK_SIZE];
};

static EWRAM_DATA struct VideoPoker *sPoker = NULL;

static void CB2_VideoPoker(void);
static void VBlankCB_VideoPoker(void);
static void Task_VideoPoker(u8 taskId);
static void InitPokerScreen(void);

static const u16 sPokerBgPal[16] =
{
    RGB(3, 13, 7),   // 0: felt (backdrop)
    RGB(31, 31, 31), // 1: white
    RGB(2, 6, 3),    // 2: dark shadow
    RGB(20, 26, 20), // 3: light
    RGB(31, 10, 8),  // 4: red
    RGB(31, 27, 6),  // 5: yellow
    RGB(13, 4, 3),   // 6: red shadow
    RGB(17, 13, 0),  // 7: yellow shadow
};

static const u8 sColor_Normal[] = {TEXT_COLOR_TRANSPARENT, 1, 2};
static const u8 sColor_Gold[]   = {TEXT_COLOR_TRANSPARENT, 5, 7};
static const u8 sColor_Red[]    = {TEXT_COLOR_TRANSPARENT, 4, 6};

static const u16 sHandPayout[POKER_HAND_COUNT] =
{
    [POKER_HAND_NOTHING]         = 0,
    [POKER_HAND_JACKS_OR_BETTER] = GC_POKER_PAY_JACKS_OR_BETTER,
    [POKER_HAND_TWO_PAIR]        = GC_POKER_PAY_TWO_PAIR,
    [POKER_HAND_THREE_KIND]      = GC_POKER_PAY_THREE_KIND,
    [POKER_HAND_STRAIGHT]        = GC_POKER_PAY_STRAIGHT,
    [POKER_HAND_FLUSH]           = GC_POKER_PAY_FLUSH,
    [POKER_HAND_FULL_HOUSE]      = GC_POKER_PAY_FULL_HOUSE,
    [POKER_HAND_FOUR_KIND]       = GC_POKER_PAY_FOUR_KIND,
    [POKER_HAND_STRAIGHT_FLUSH]  = GC_POKER_PAY_STRAIGHT_FLUSH,
    [POKER_HAND_ROYAL_FLUSH]     = GC_POKER_PAY_ROYAL_FLUSH,
};

static const u8 *const sHandNames[POKER_HAND_COUNT] =
{
    [POKER_HAND_NOTHING]         = COMPOUND_STRING("NO WIN"),
    [POKER_HAND_JACKS_OR_BETTER] = COMPOUND_STRING("JACKS OR BETTER"),
    [POKER_HAND_TWO_PAIR]        = COMPOUND_STRING("TWO PAIR"),
    [POKER_HAND_THREE_KIND]      = COMPOUND_STRING("THREE OF A KIND"),
    [POKER_HAND_STRAIGHT]        = COMPOUND_STRING("STRAIGHT"),
    [POKER_HAND_FLUSH]           = COMPOUND_STRING("FLUSH"),
    [POKER_HAND_FULL_HOUSE]      = COMPOUND_STRING("FULL HOUSE"),
    [POKER_HAND_FOUR_KIND]       = COMPOUND_STRING("FOUR OF A KIND"),
    [POKER_HAND_STRAIGHT_FLUSH]  = COMPOUND_STRING("STRAIGHT FLUSH"),
    [POKER_HAND_ROYAL_FLUSH]     = COMPOUND_STRING("ROYAL FLUSH"),
};

// Pay table, shortest names first so three columns fit
static const u8 *const sPaytableNames[POKER_HAND_COUNT] =
{
    [POKER_HAND_JACKS_OR_BETTER] = COMPOUND_STRING("JACKS+"),
    [POKER_HAND_TWO_PAIR]        = COMPOUND_STRING("2 PAIR"),
    [POKER_HAND_THREE_KIND]      = COMPOUND_STRING("3 KIND"),
    [POKER_HAND_STRAIGHT]        = COMPOUND_STRING("STRAIGHT"),
    [POKER_HAND_FLUSH]           = COMPOUND_STRING("FLUSH"),
    [POKER_HAND_FULL_HOUSE]      = COMPOUND_STRING("FULL HSE"),
    [POKER_HAND_FOUR_KIND]       = COMPOUND_STRING("4 KIND"),
    [POKER_HAND_STRAIGHT_FLUSH]  = COMPOUND_STRING("STR FLUSH"),
    [POKER_HAND_ROYAL_FLUSH]     = COMPOUND_STRING("ROYAL"),
};

static const u8 sText_Hold[] = _("HOLD");
static const u8 sText_Cursor[] = _("{UP_ARROW}");
static const u8 sText_BetLine[] = _("BET {STR_VAR_1}   COINS {STR_VAR_2}");
static const u8 sText_HelpBet[] = _("{DPAD_UPDOWN}BET {A_BUTTON}DEAL {B_BUTTON}EXIT");
static const u8 sText_HelpHold[] = _("{DPAD_LEFTRIGHT}PICK {A_BUTTON}HOLD {START_BUTTON}DRAW");
static const u8 sText_HelpResult[] = _("{A_BUTTON}NEXT HAND {B_BUTTON}EXIT");
static const u8 sText_HelpBroke[] = _("Not enough COINS.  {B_BUTTON}EXIT");
static const u8 sText_Won[] = _("{STR_VAR_3}! WON {STR_VAR_1}   COINS {STR_VAR_2}");
static const u8 sText_Lost[] = _("NO WIN   COINS {STR_VAR_2}");

static const struct BgTemplate sPokerBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0,
    },
};

static const struct WindowTemplate sPokerWinTemplates[] =
{
    [WIN_PAYTABLE] = {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 0,
        .width = 30,
        .height = 6,
        .paletteNum = 0,
        .baseBlock = 1,
    },
    [WIN_HOLD] = {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 15,
        .width = 30,
        .height = 2,
        .paletteNum = 0,
        .baseBlock = 1 + 30 * 6,
    },
    [WIN_INFO] = {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 17,
        .width = 30,
        .height = 3,
        .paletteNum = 0,
        .baseBlock = 1 + 30 * 6 + 30 * 2,
    },
    DUMMY_WIN_TEMPLATE,
};

// ------------------------------------------------------------------
// Hand evaluation

static u8 EvaluateHand(const u8 *hand)
{
    u8 rankCount[GC_RANKS] = {0};
    u32 i;
    u8 pairs = 0, trips = 0, quads = 0, highPair = FALSE;
    u8 low = GC_RANKS, high = 0, distinct = 0;
    bool8 flush = TRUE, straight = FALSE;

    for (i = 0; i < HAND_SIZE; i++)
    {
        rankCount[GC_CARD_RANK(hand[i])]++;
        if (GC_CARD_SUIT(hand[i]) != GC_CARD_SUIT(hand[0]))
            flush = FALSE;
    }
    for (i = 0; i < GC_RANKS; i++)
    {
        if (rankCount[i] == 0)
            continue;
        distinct++;
        if (i < low)
            low = i;
        if (i > high)
            high = i;
        if (rankCount[i] == 2)
        {
            pairs++;
            if (i >= GC_RANK_JACK)
                highPair = TRUE;
        }
        else if (rankCount[i] == 3)
        {
            trips++;
        }
        else if (rankCount[i] == 4)
        {
            quads++;
        }
    }
    if (distinct == HAND_SIZE)
    {
        if (high - low == HAND_SIZE - 1)
            straight = TRUE;
        // A-2-3-4-5: the ace plays low (ranks 0-3 and the ace)
        else if (rankCount[GC_RANK_ACE] && rankCount[0] && rankCount[1] && rankCount[2] && rankCount[3])
            straight = TRUE;
    }

    if (straight && flush)
        return (low == GC_RANK_ACE - 4) ? POKER_HAND_ROYAL_FLUSH : POKER_HAND_STRAIGHT_FLUSH;
    if (quads)
        return POKER_HAND_FOUR_KIND;
    if (trips && pairs)
        return POKER_HAND_FULL_HOUSE;
    if (flush)
        return POKER_HAND_FLUSH;
    if (straight)
        return POKER_HAND_STRAIGHT;
    if (trips)
        return POKER_HAND_THREE_KIND;
    if (pairs == 2)
        return POKER_HAND_TWO_PAIR;
    if (highPair)
        return POKER_HAND_JACKS_OR_BETTER;
    return POKER_HAND_NOTHING;
}

// ------------------------------------------------------------------
// Drawing

static void PrintCentered(u8 windowId, u8 fontId, const u8 *str, u8 centerX, u8 y, const u8 *color)
{
    s32 width = GetStringWidth(fontId, str, 0);
    AddTextPrinterParameterized3(windowId, fontId, centerX - width / 2, y, color, TEXT_SKIP_DRAW, str);
}

static void DrawPaytable(void)
{
    u32 i;
    u8 x, y;

    FillWindowPixelBuffer(WIN_PAYTABLE, PIXEL_FILL(0));
    // 3 columns x 3 rows, best hands on the left
    for (i = 0; i < POKER_HAND_COUNT - 1; i++)
    {
        u8 hand = POKER_HAND_ROYAL_FLUSH - i;
        x = 4 + (i / 3) * 80;
        y = 2 + (i % 3) * 15;
        AddTextPrinterParameterized3(WIN_PAYTABLE, FONT_SMALL, x, y, sColor_Gold, TEXT_SKIP_DRAW, sPaytableNames[hand]);
        ConvertIntToDecimalStringN(gStringVar1, sHandPayout[hand], STR_CONV_MODE_LEFT_ALIGN, 3);
        AddTextPrinterParameterized3(WIN_PAYTABLE, FONT_SMALL, x + 72 - GetStringWidth(FONT_SMALL, gStringVar1, 0), y,
                                     (sPoker->state == STATE_RESULT && sPoker->lastHand == hand) ? sColor_Red : sColor_Normal,
                                     TEXT_SKIP_DRAW, gStringVar1);
    }
    CopyWindowToVram(WIN_PAYTABLE, COPYWIN_FULL);
}

static void DrawHoldRow(void)
{
    u32 i;

    FillWindowPixelBuffer(WIN_HOLD, PIXEL_FILL(0));
    for (i = 0; i < HAND_SIZE; i++)
    {
        if (sPoker->held[i])
            PrintCentered(WIN_HOLD, FONT_SMALL, sText_Hold, CARD_X(i), 0, sColor_Gold);
        if (sPoker->state == STATE_HOLD && sPoker->cursor == i)
            PrintCentered(WIN_HOLD, FONT_SMALL, sText_Cursor, CARD_X(i), 6, sColor_Normal);
    }
    CopyWindowToVram(WIN_HOLD, COPYWIN_FULL);
}

static void DrawInfo(void)
{
    const u8 *help;
    const u8 *line;

    ConvertIntToDecimalStringN(gStringVar1, sPoker->stake, STR_CONV_MODE_LEFT_ALIGN, 3);
    ConvertIntToDecimalStringN(gStringVar2, GetCoins(), STR_CONV_MODE_LEFT_ALIGN, 4);
    switch (sPoker->state)
    {
    case STATE_RESULT:
        if (sPoker->lastPayout != 0)
        {
            ConvertIntToDecimalStringN(gStringVar1, sPoker->lastPayout, STR_CONV_MODE_LEFT_ALIGN, 5);
            StringCopy(gStringVar3, sHandNames[sPoker->lastHand]);
            line = sText_Won;
        }
        else
        {
            line = sText_Lost;
        }
        help = (GetCoins() >= GC_CARD_STAKE_MIN) ? sText_HelpResult : sText_HelpBroke;
        break;
    case STATE_HOLD:
        line = sText_BetLine;
        help = sText_HelpHold;
        break;
    default:
        line = sText_BetLine;
        help = (GetCoins() >= GC_CARD_STAKE_MIN) ? sText_HelpBet : sText_HelpBroke;
        break;
    }
    StringExpandPlaceholders(gStringVar4, line);
    FillWindowPixelBuffer(WIN_INFO, PIXEL_FILL(0));
    AddTextPrinterParameterized3(WIN_INFO, FONT_SMALL, 4, 0, sColor_Normal, TEXT_SKIP_DRAW, gStringVar4);
    AddTextPrinterParameterized3(WIN_INFO, FONT_SMALL, 4, 12, sColor_Normal, TEXT_SKIP_DRAW, help);
    CopyWindowToVram(WIN_INFO, COPYWIN_FULL);
}

static void ShowCard(u8 slot, u8 card)
{
    GameCornerCards_LoadGfx(card, TAG_CARD_TILES + slot);
}

static void RedrawAll(void)
{
    DrawPaytable();
    DrawHoldRow();
    DrawInfo();
}

// ------------------------------------------------------------------
// Game flow

static u16 ClampStake(u16 stake)
{
    u16 coins = GetCoins();

    if (stake > GC_CARD_STAKE_MAX)
        stake = GC_CARD_STAKE_MAX;
    // Highest affordable step
    while (stake > GC_CARD_STAKE_MIN && stake > coins)
        stake -= GC_CARD_STAKE_STEP;
    if (stake < GC_CARD_STAKE_MIN)
        stake = GC_CARD_STAKE_MIN;
    return stake;
}

static void Deal(void)
{
    u32 i;

    RemoveCoins(sPoker->stake);
    GameCornerCards_Shuffle(sPoker->deck);
    for (i = 0; i < HAND_SIZE; i++)
    {
        sPoker->hand[i] = sPoker->deck[i];
        sPoker->held[i] = FALSE;
        ShowCard(i, sPoker->hand[i]);
    }
    sPoker->deckPos = HAND_SIZE;
    sPoker->cursor = 0;
    sPoker->state = STATE_HOLD;
    PlaySE(SE_CARD);
    RedrawAll();
}

static void Draw(void)
{
    u32 i, payout;

    for (i = 0; i < HAND_SIZE; i++)
    {
        if (!sPoker->held[i])
        {
            sPoker->hand[i] = sPoker->deck[sPoker->deckPos++];
            ShowCard(i, sPoker->hand[i]);
        }
    }
    sPoker->lastHand = EvaluateHand(sPoker->hand);
    payout = sHandPayout[sPoker->lastHand] * sPoker->stake;
    if (payout > MAX_COINS - GetCoins())
        payout = MAX_COINS - GetCoins();
    sPoker->lastPayout = payout;
    if (payout != 0)
    {
        AddCoins(payout);
        PlayFanfare(sPoker->lastHand >= POKER_HAND_FULL_HOUSE ? MUS_SLOTS_JACKPOT : MUS_SLOTS_WIN);
    }
    else
    {
        PlaySE(SE_FAILURE);
    }
    sPoker->state = STATE_RESULT;
    RedrawAll();
}

static void BackToBetting(void)
{
    u32 i;

    for (i = 0; i < HAND_SIZE; i++)
    {
        sPoker->held[i] = FALSE;
        ShowCard(i, GC_CARD_FACE_DOWN);
    }
    sPoker->stake = ClampStake(sPoker->stake);
    sPoker->state = STATE_BET;
    RedrawAll();
}

static void StartExit(void)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    sPoker->state = STATE_FADE_OUT;
}

static void Task_VideoPoker(u8 taskId)
{
    switch (sPoker->state)
    {
    case STATE_FADE_IN:
        if (!gPaletteFade.active)
            sPoker->state = STATE_BET;
        break;
    case STATE_BET:
        if (JOY_NEW(B_BUTTON))
        {
            PlaySE(SE_SELECT);
            StartExit();
        }
        else if (GetCoins() < GC_CARD_STAKE_MIN)
        {
            // Can't afford a hand: only B works
        }
        else if (JOY_NEW(A_BUTTON))
        {
            Deal();
        }
        else if (JOY_REPEAT(DPAD_UP) && sPoker->stake + GC_CARD_STAKE_STEP <= GC_CARD_STAKE_MAX
              && sPoker->stake + GC_CARD_STAKE_STEP <= GetCoins())
        {
            PlaySE(SE_SELECT);
            sPoker->stake += GC_CARD_STAKE_STEP;
            DrawInfo();
        }
        else if (JOY_REPEAT(DPAD_DOWN) && sPoker->stake - GC_CARD_STAKE_STEP >= GC_CARD_STAKE_MIN)
        {
            PlaySE(SE_SELECT);
            sPoker->stake -= GC_CARD_STAKE_STEP;
            DrawInfo();
        }
        break;
    case STATE_HOLD:
        if (JOY_NEW(START_BUTTON))
        {
            Draw();
        }
        else if (JOY_NEW(A_BUTTON))
        {
            PlaySE(SE_SELECT);
            sPoker->held[sPoker->cursor] ^= TRUE;
            DrawHoldRow();
        }
        else if (JOY_REPEAT(DPAD_LEFT))
        {
            PlaySE(SE_SELECT);
            sPoker->cursor = (sPoker->cursor + HAND_SIZE - 1) % HAND_SIZE;
            DrawHoldRow();
        }
        else if (JOY_REPEAT(DPAD_RIGHT))
        {
            PlaySE(SE_SELECT);
            sPoker->cursor = (sPoker->cursor + 1) % HAND_SIZE;
            DrawHoldRow();
        }
        break;
    case STATE_RESULT:
        if (!IsFanfareTaskInactive())
            break;
        if (JOY_NEW(B_BUTTON))
        {
            PlaySE(SE_SELECT);
            StartExit();
        }
        else if (JOY_NEW(A_BUTTON) && GetCoins() >= GC_CARD_STAKE_MIN)
        {
            PlaySE(SE_SELECT);
            BackToBetting();
        }
        break;
    case STATE_FADE_OUT:
        if (!gPaletteFade.active)
        {
            DestroyTask(taskId);
            sPoker->state = STATE_EXIT;
            SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
            Free(GetBgTilemapBuffer(0));
            FreeAllWindowBuffers();
            FreeAllSpritePalettes();
            FREE_AND_SET_NULL(sPoker);
        }
        break;
    }
}

// ------------------------------------------------------------------
// Setup

static void CB2_VideoPoker(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void VBlankCB_VideoPoker(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void InitPokerScreen(void)
{
    u32 i;

    SetVBlankCallback(NULL);
    ScanlineEffect_Stop();
    ResetAllBgsCoordinates();
    ResetVramOamAndBgCntRegs();
    ResetBgsAndClearDma3BusyFlags(0);
    ResetTempTileDataBuffers();
    ResetPaletteFade();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetTasks();

    InitBgsFromTemplates(0, sPokerBgTemplates, ARRAY_COUNT(sPokerBgTemplates));
    SetBgTilemapBuffer(0, AllocZeroed(BG_SCREEN_SIZE));
    InitWindows(sPokerWinTemplates);
    DeactivateAllTextPrinters();
    LoadPalette(sPokerBgPal, BG_PLTT_ID(0), sizeof(sPokerBgPal));
    LoadSpritePalette(&(const struct SpritePalette){gGameCornerCards_Pal, TAG_CARD_PAL});

    for (i = 0; i < WIN_INFO + 1; i++)
        PutWindowTilemap(i);

    for (i = 0; i < HAND_SIZE; i++)
    {
        ShowCard(i, GC_CARD_FACE_DOWN);
        sPoker->cardSpriteIds[i] = GameCornerCards_CreateSprite(TAG_CARD_TILES + i, TAG_CARD_PAL, CARD_X(i), CARD_Y, 0);
    }
    sPoker->stake = ClampStake(GC_CARD_STAKE_MIN);
    sPoker->state = STATE_FADE_IN;
    RedrawAll();

    CopyBgTilemapBufferToVram(0);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON | DISPCNT_BG0_ON);
    ShowBg(0);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(VBlankCB_VideoPoker);
    SetMainCallback2(CB2_VideoPoker);
    CreateTask(Task_VideoPoker, 0);
}

static void Task_FadeToPoker(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        InitPokerScreen();
    }
}

void StartVideoPoker(void)
{
    sPoker = AllocZeroed(sizeof(*sPoker));
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    CreateTask(Task_FadeToPoker, 0);
}
