#include "global.h"
#include "game_corner_tien_len.h"
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
#include "random.h"
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

// Fantasy Retold: Tien Len (southern rules), the Game Corner's third card game
// (design.md §2, Game Corner Minigames). The player against 3 AI, 13 cards each.
// 4th pays 1st a stake and 3rd pays 2nd half; the chopped pays the chopper and the
// last player pays for stuck 2s / bombs, all from the pocket (an empty pocket
// waives the rest).

// ---- TL ENGINE BEGIN (no GBA calls below this line until TL ENGINE END; host-tested)

// A Tien Len value is rank * 4 + suit: rank 0 = '3' ... 11 = 'A', 12 = '2';
// suit 0 = spades < 1 = clubs < 2 = diamonds < 3 = hearts. 3 of spades = 0.
#define TL_SEATS        4
#define TL_HAND_MAX     13
#define TL_MAX_MOVES    400
#define TL_RANK(v)      ((v) / 4)
#define TL_SUIT(v)      ((v) % 4)
#define TL_RANK_TWO     12
#define TL_RANK_ACE     11
#define TL_THREE_SPADES 0

enum
{
    TL_NONE,
    TL_SINGLE,
    TL_PAIR,
    TL_TRIPLE,
    TL_QUAD,
    TL_STRAIGHT,
    TL_DOUBLE_SEQ, // 3+ consecutive pairs
    TL_TYPE_COUNT,
};

enum
{
    TL_BEAT_NO,
    TL_BEAT_YES,
    TL_BEAT_CHOP,
};

enum
{
    TL_INSTANT_NONE,
    TL_INSTANT_FOUR_TWOS,
    TL_INSTANT_DRAGON,
    TL_INSTANT_SIX_PAIRS,
    TL_INSTANT_ONE_SUIT,
};

struct TLCombo
{
    u8 type;
    u8 count;
    u8 top; // highest value; combos of the same type and size compare by it
    u8 cards[TL_HAND_MAX];
};

static void TL_Sort(u8 *v, u8 n)
{
    u32 i, j;

    for (i = 1; i < n; i++)
    {
        u8 x = v[i];
        for (j = i; j > 0 && v[j - 1] > x; j--)
            v[j] = v[j - 1];
        v[j] = x;
    }
}

static bool8 TL_Classify(const u8 *cardsIn, u8 n, struct TLCombo *out)
{
    u8 c[TL_HAND_MAX];
    u32 i;
    u8 first, last;

    if (n == 0 || n > TL_HAND_MAX)
        return FALSE;
    for (i = 0; i < n; i++)
        c[i] = cardsIn[i];
    TL_Sort(c, n);
    out->type = TL_NONE;
    out->count = n;
    out->top = c[n - 1];
    for (i = 0; i < n; i++)
        out->cards[i] = c[i];

    first = TL_RANK(c[0]);
    last = TL_RANK(c[n - 1]);
    if (first == last)
    {
        static const u8 sameRank[] = {TL_NONE, TL_SINGLE, TL_PAIR, TL_TRIPLE, TL_QUAD};
        out->type = sameRank[n];
        return TRUE;
    }
    if (n < 3 || last == TL_RANK_TWO) // runs never hold a 2
        return FALSE;

    for (i = 1; i < n; i++)
    {
        if (TL_RANK(c[i]) != TL_RANK(c[i - 1]) + 1)
            break;
    }
    if (i == n)
    {
        out->type = TL_STRAIGHT;
        return TRUE;
    }

    if (n >= 6 && n % 2 == 0)
    {
        for (i = 0; i < n; i += 2)
        {
            if (TL_RANK(c[i]) != TL_RANK(c[i + 1]))
                return FALSE;
            if (i != 0 && TL_RANK(c[i]) != TL_RANK(c[i - 2]) + 1)
                return FALSE;
        }
        out->type = TL_DOUBLE_SEQ;
        return TRUE;
    }
    return FALSE;
}

static bool8 TL_IsTwos(const struct TLCombo *c)
{
    return c->type >= TL_SINGLE && c->type <= TL_TRIPLE && TL_RANK(c->top) == TL_RANK_TWO;
}

static bool8 TL_IsBomb(const struct TLCombo *c)
{
    return c->type == TL_QUAD || c->type == TL_DOUBLE_SEQ;
}

static bool8 TL_ComboHas(const struct TLCombo *c, u8 value)
{
    u32 i;

    for (i = 0; i < c->count; i++)
    {
        if (c->cards[i] == value)
            return TRUE;
    }
    return FALSE;
}

// Can `play` go on top of `cur`? TL_BEAT_CHOP when it chops 2s or a bomb.
static u8 TL_Beats(const struct TLCombo *play, const struct TLCombo *cur)
{
    u8 playPairs = (play->type == TL_DOUBLE_SEQ) ? play->count / 2 : 0;
    u8 curPairs = (cur->type == TL_DOUBLE_SEQ) ? cur->count / 2 : 0;

    if (cur->type == TL_NONE)
        return TL_BEAT_YES;
    if (play->type == cur->type && play->count == cur->count && play->top > cur->top)
        return TL_IsBomb(cur) ? TL_BEAT_CHOP : TL_BEAT_YES;

    switch (cur->type)
    {
    case TL_SINGLE:
        if (TL_IsTwos(cur) && (play->type == TL_QUAD || playPairs >= 3))
            return TL_BEAT_CHOP;
        break;
    case TL_PAIR:
        if (TL_IsTwos(cur) && (play->type == TL_QUAD || playPairs >= 4))
            return TL_BEAT_CHOP;
        break;
    case TL_TRIPLE:
        if (TL_IsTwos(cur) && playPairs >= 5)
            return TL_BEAT_CHOP;
        break;
    case TL_QUAD:
        if (playPairs >= 4)
            return TL_BEAT_CHOP;
        break;
    case TL_DOUBLE_SEQ:
        if ((play->type == TL_QUAD && curPairs == 3) || playPairs > curPairs)
            return TL_BEAT_CHOP;
        break;
    }
    return TL_BEAT_NO;
}

// What a chop of `cur` is worth, in half-stakes: a black 2 is 1, a red 2 is 2, a bomb 2.
static u8 TL_ChopValue(const struct TLCombo *cur)
{
    u32 i;
    u8 value = 0;

    if (!TL_IsTwos(cur))
        return 2;
    for (i = 0; i < cur->count; i++)
        value += (TL_SUIT(cur->cards[i]) >= 2) ? 2 : 1;
    return value;
}

static u8 TL_InstantWin(const u8 *hand, u8 n)
{
    u8 cnt[TL_RANK_TWO + 1] = {0};
    u32 i;

    u8 run = 0;
    bool8 oneSuit = TRUE;

    for (i = 0; i < n; i++)
    {
        cnt[TL_RANK(hand[i])]++;
        if (TL_SUIT(hand[i]) != TL_SUIT(hand[0]))
            oneSuit = FALSE;
    }
    if (oneSuit && n == TL_HAND_MAX)
        return TL_INSTANT_ONE_SUIT;
    if (cnt[TL_RANK_TWO] == 4)
        return TL_INSTANT_FOUR_TWOS;
    // 6 consecutive pairs (no 2s, like any pair run)
    for (i = 0; i < TL_RANK_TWO; i++)
    {
        run = (cnt[i] >= 2) ? run + 1 : 0;
        if (run == 6)
            return TL_INSTANT_SIX_PAIRS;
    }
    for (i = 0; i < TL_RANK_TWO; i++)
    {
        if (cnt[i] == 0)
            return TL_INSTANT_NONE;
    }
    return TL_INSTANT_DRAGON; // 3 through A
}

// Stuck cards (thoi): what the last player still holds is worth, in half-stakes,
// 1 per black 2, 2 per red 2, 2 per four of a kind and 2 per pair run (3+ pairs, no 2s).
static u8 TL_StuckValue(const u8 *hand, u8 n)
{
    u8 cnt[TL_RANK_TWO + 1] = {0};
    u8 value = 0, run = 0;
    u32 i;

    for (i = 0; i < n; i++)
    {
        cnt[TL_RANK(hand[i])]++;
        if (TL_RANK(hand[i]) == TL_RANK_TWO)
            value += (TL_SUIT(hand[i]) >= 2) ? 2 : 1;
    }
    for (i = 0; i < TL_RANK_TWO; i++)
    {
        if (cnt[i] == 4)
            value += 2;
        run = (cnt[i] >= 2) ? run + 1 : 0;
        if (run == 3) // counted once per run, however long
            value += 2;
    }
    return value;
}

// Whose turn follows `seat`. Finished seats are skipped. When everyone still
// in has passed since `owner` played, the trick is over (*trickOver): its owner
// leads, or the next seat still in if the owner has gone out.
static u8 TL_NextSeat(u8 seat, u8 owner, const bool8 *passed, const bool8 *finished, bool8 *trickOver)
{
    u8 next = seat;
    u32 i;

    *trickOver = FALSE;
    for (i = 0; i < TL_SEATS; i++)
    {
        next = (next + 1) % TL_SEATS;
        if (next == owner)
            break;
        if (!finished[next] && !passed[next])
            return next;
    }
    *trickOver = TRUE;
    next = owner;
    for (i = 0; i < TL_SEATS && finished[next]; i++)
        next = (next + 1) % TL_SEATS;
    return next;
}

static void TL_RemoveCards(u8 *hand, u8 *n, const struct TLCombo *c)
{
    u32 i, j = 0;

    for (i = 0; i < *n; i++)
    {
        if (!TL_ComboHas(c, hand[i]))
            hand[j++] = hand[i];
    }
    *n = j;
}

static void TL_AddMove(struct TLCombo *moves, u16 *num, const u8 *cards, u8 k)
{
    if (*num < TL_MAX_MOVES && TL_Classify(cards, k, &moves[*num]))
        (*num)++;
}

// Every combo the hand can make (runs and pair runs use the lowest suits below the top).
static u16 TL_GenMoves(const u8 *hand, u8 n, struct TLCombo *moves)
{
    u8 byRank[TL_RANK_TWO + 1][4];
    u8 cnt[TL_RANK_TWO + 1] = {0};
    u8 buf[TL_HAND_MAX];
    u16 num = 0;
    u32 i, j, k, r, s, e;

    for (i = 0; i < n; i++)
    {
        r = TL_RANK(hand[i]);
        byRank[r][cnt[r]++] = hand[i];
    }
    for (r = 0; r <= TL_RANK_TWO; r++)
        TL_Sort(byRank[r], cnt[r]);

    for (i = 0; i < n; i++)
        TL_AddMove(moves, &num, &hand[i], 1);

    for (r = 0; r <= TL_RANK_TWO; r++)
    {
        for (i = 0; i < cnt[r]; i++)
        {
            for (j = i + 1; j < cnt[r]; j++)
            {
                buf[0] = byRank[r][i];
                buf[1] = byRank[r][j];
                TL_AddMove(moves, &num, buf, 2);
                for (k = j + 1; k < cnt[r]; k++)
                {
                    buf[2] = byRank[r][k];
                    TL_AddMove(moves, &num, buf, 3);
                }
            }
        }
        if (cnt[r] == 4)
            TL_AddMove(moves, &num, byRank[r], 4);
    }

    // Runs: ranks s..e, no 2s
    for (s = 0; s < TL_RANK_TWO; s++)
    {
        if (cnt[s] == 0)
            continue;
        for (e = s + 1; e < TL_RANK_TWO && cnt[e] != 0; e++)
        {
            if (e - s + 1 < 3)
                continue;
            for (r = s; r < e; r++)
                buf[r - s] = byRank[r][0];
            for (j = 0; j < cnt[e]; j++)
            {
                buf[e - s] = byRank[e][j];
                TL_AddMove(moves, &num, buf, e - s + 1);
            }
        }
    }

    // Pair runs: 3+ consecutive ranks with 2+ cards each, no 2s
    for (s = 0; s < TL_RANK_TWO; s++)
    {
        if (cnt[s] < 2)
            continue;
        for (e = s + 1; e < TL_RANK_TWO && cnt[e] >= 2; e++)
        {
            u8 pairs = e - s + 1;
            if (pairs < 3 || pairs * 2 > TL_HAND_MAX)
                continue;
            for (r = s; r < e; r++)
            {
                buf[(r - s) * 2] = byRank[r][0];
                buf[(r - s) * 2 + 1] = byRank[r][1];
            }
            for (j = 1; j < cnt[e]; j++)
            {
                buf[(e - s) * 2] = byRank[e][0];
                buf[(e - s) * 2 + 1] = byRank[e][j];
                TL_AddMove(moves, &num, buf, pairs * 2);
            }
        }
    }
    return num;
}

// Picks the AI's play into *out; FALSE = pass. `moves` is scratch space (TL_MAX_MOVES).
static bool8 TL_AiChoose(const u8 *hand, u8 n, const struct TLCombo *cur, bool8 mustThreeSpades,
                         u8 fewestOpponentCards, u8 coinFlip, struct TLCombo *moves, struct TLCombo *out)
{
    u8 cnt[TL_RANK_TWO + 1] = {0};
    u16 num, i;
    s32 best = -1, bestScore = 0;
    bool8 leading = (cur->type == TL_NONE);
    bool8 endgame = (fewestOpponentCards <= 4 || n <= 4);

    for (i = 0; i < n; i++)
        cnt[TL_RANK(hand[i])]++;
    num = TL_GenMoves(hand, n, moves);

    // Going out wins the hand
    for (i = 0; i < num; i++)
    {
        if (moves[i].count == n && TL_Beats(&moves[i], cur) != TL_BEAT_NO
         && (!mustThreeSpades || TL_ComboHas(&moves[i], TL_THREE_SPADES)))
        {
            *out = moves[i];
            return TRUE;
        }
    }

    for (i = 0; i < num; i++)
    {
        const struct TLCombo *m = &moves[i];
        bool8 hasTwo = (TL_RANK(m->top) == TL_RANK_TWO);
        u8 beat = TL_Beats(m, cur);
        s32 score;

        if (beat == TL_BEAT_NO)
            continue;
        if (mustThreeSpades && !TL_ComboHas(m, TL_THREE_SPADES))
            continue;

        if (leading)
        {
            // Shed the lowest card in the biggest combo that holds it; keep 2s and bombs
            if (hasTwo || TL_IsBomb(m))
                score = 1000 - m->top;
            else if (TL_ComboHas(m, hand[0]) || mustThreeSpades)
                score = 5000 + m->count * 100 - m->top;
            else
                score = 2000 + m->count * 100 - m->top;
        }
        else if (beat == TL_BEAT_CHOP)
        {
            score = 9000 - m->count; // always chop, with the smallest bomb
        }
        else if (hasTwo)
        {
            // A 2 only late, over an ace, or now and then
            if (!endgame && TL_RANK(cur->top) < TL_RANK_ACE && coinFlip != 0)
                continue;
            score = 1000 - m->top;
        }
        else
        {
            score = 5000 - m->top;
            // Don't break a pair / triple for a smaller play
            if ((m->type == TL_SINGLE || m->type == TL_PAIR || m->type == TL_TRIPLE) && cnt[TL_RANK(m->top)] > m->count)
                score -= endgame ? 0 : 60;
        }
        if (best < 0 || score > bestScore)
        {
            best = i;
            bestScore = score;
        }
    }
    if (best < 0)
        return FALSE;
    *out = moves[best];
    return TRUE;
}

// ---- TL ENGINE END

#define TL_PLAYER 0

enum
{
    STATE_FADE_IN,
    STATE_BET,
    STATE_PLAYER_TURN,
    STATE_AI_WAIT,
    STATE_PAUSE, // shows the last play before the game moves on
    STATE_RESULT,
    STATE_FADE_OUT,
};

enum
{
    WIN_OPPONENTS,
    WIN_STATUS,
};

#define TAG_CARD_PAL     0x5B00
#define TAG_HAND_TILES   0x5B00 // + slot
#define TAG_TABLE_TILES  0x5B20 // + slot
#define NOT_SHOWN        0xFE

#define TABLE_Y          46
#define HAND_Y           136
#define CARD_SPACING     16
#define RAISE_SELECTED   10
#define RAISE_CURSOR     4
#define AI_DELAY         30 // AI "thinking" before it plays
#define PLAY_PAUSE       75 // a play stays on screen this long before the next turn
#define PASS_PAUSE       40

struct TienLen
{
    u8 state;
    u8 seat; // whose turn
    u8 curOwner;
    u8 nextLeader;
    u8 winner;
    u8 cursor;
    u8 delay;
    bool8 firstHand;
    bool8 mustThreeSpades;
    bool8 passed[TL_SEATS];
    bool8 finished[TL_SEATS];
    u8 place[TL_SEATS]; // 1st-4th once out, 0 while still in
    u8 numOut;
    bool8 handOver; // set by the last play; the result shows after the pause
    u8 handCount[TL_SEATS];
    u8 hand[TL_SEATS][TL_HAND_MAX];
    u16 stake;
    u16 selected; // bit per hand slot
    u8 bonusHalves;
    u8 penaltyHalves; // chopped
    u8 stuckHalves;   // thoi: left holding 2s / bombs when last
    s32 lastNet;
    struct TLCombo cur;
    u8 handSpriteIds[TL_HAND_MAX];
    u8 tableSpriteIds[TL_HAND_MAX];
    u8 handShown[TL_HAND_MAX];
    u8 tableShown[TL_HAND_MAX];
    u8 deck[GC_DECK_SIZE];
    struct TLCombo moves[TL_MAX_MOVES];
};

static EWRAM_DATA struct TienLen *sTL = NULL;

static const u16 sTienLenBgPal[16] =
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

static const u8 *const sSeatNames[TL_SEATS] =
{
    COMPOUND_STRING("YOU"),
    COMPOUND_STRING("MINH"),
    COMPOUND_STRING("LINH"),
    COMPOUND_STRING("BAO"),
};

static const u8 *const sComboNames[TL_TYPE_COUNT] =
{
    [TL_NONE]       = COMPOUND_STRING("nothing"),
    [TL_SINGLE]     = COMPOUND_STRING("a card"),
    [TL_PAIR]       = COMPOUND_STRING("a pair"),
    [TL_TRIPLE]     = COMPOUND_STRING("a triple"),
    [TL_QUAD]       = COMPOUND_STRING("four of a kind"),
    [TL_STRAIGHT]   = COMPOUND_STRING("a run"),
    [TL_DOUBLE_SEQ] = COMPOUND_STRING("a pair run"),
};

static const u8 sText_Played[] = _("{STR_VAR_1} played {STR_VAR_2}.");
static const u8 sText_Chopped[] = _("{STR_VAR_1} CHOPPED with {STR_VAR_2}!");
static const u8 sText_Passed[] = _("{STR_VAR_1} passed.");
static const u8 sText_YourLead[] = _("Your lead.");
static const u8 sText_YourLeadThreeSpades[] = _("Your lead. Include the 3 of SPADES.");
static const u8 sText_YourTurn[] = _("Your turn.");
static const u8 sText_NotAHand[] = _("That's not a valid hand.");
static const u8 sText_DoesntBeat[] = _("That doesn't beat the table.");
static const u8 sText_NeedThreeSpades[] = _("The first lead must hold the 3 of SPADES.");
static const u8 sText_MustLead[] = _("You lead, so you can't pass.");
static const u8 sText_BetLine[] = _("BET {STR_VAR_1}   COINS {STR_VAR_2}");
static const u8 sText_IsOut[] = _("{STR_VAR_1} went out {STR_VAR_2}!");
static const u8 sText_Result[] = _("{STR_VAR_1} won. You: {STR_VAR_2}. CHOPS +{STR_VAR_3}");
static const u8 sText_ResultPenalty[] = _(" -");
static const u8 sText_ResultStuck[] = _(" STUCK -");
static const u8 sText_SplitInstant[] = _("{STR_VAR_1}: instant wins! Pot split {STR_VAR_2} ways.");
static const u8 sText_NameJoin[] = _("/");
static const u8 sText_FourTwos[] = _("{STR_VAR_1}: four 2s, instant win!");
static const u8 sText_Dragon[] = _("{STR_VAR_1}: 3 to ACE, instant win!");
static const u8 sText_SixPairs[] = _("{STR_VAR_1}: 6 pairs in a row, instant win!");
static const u8 sText_OneSuit[] = _("{STR_VAR_1}: 13 of one suit, instant win!");
static const u8 *const sPlaceNames[TL_SEATS + 1] =
{
    COMPOUND_STRING("-"),
    COMPOUND_STRING("1st"),
    COMPOUND_STRING("2nd"),
    COMPOUND_STRING("3rd"),
    COMPOUND_STRING("4th"),
};
static const u8 sText_PaidLine[] = _("NET {STR_VAR_1}  {A_BUTTON}AGAIN {B_BUTTON}EXIT");
static const u8 sText_PaidLineBroke[] = _("NET {STR_VAR_1}  {B_BUTTON}EXIT");
static const u8 sText_Plus[] = _("+");
static const u8 sText_Minus[] = _("-");
static const u8 sText_HelpBet[] = _("{DPAD_UPDOWN}BET {A_BUTTON}DEAL {B_BUTTON}EXIT");
static const u8 sText_HelpBroke[] = _("Not enough COINS.  {B_BUTTON}EXIT");
static const u8 sText_HelpTurn[] = _("{A_BUTTON}PICK {START_BUTTON}PLAY {B_BUTTON}PASS");
static const u8 sText_HelpWait[] = _("…");
static const u8 sText_Cards[] = _("{STR_VAR_1} {STR_VAR_2}");
static const u8 sText_Pass[] = _("PASS");

static const struct BgTemplate sTLBgTemplates[] =
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

static const struct WindowTemplate sTLWinTemplates[] =
{
    [WIN_OPPONENTS] = {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 0,
        .width = 30,
        .height = 2,
        .paletteNum = 0,
        .baseBlock = 1,
    },
    [WIN_STATUS] = {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 10,
        .width = 30,
        .height = 3,
        .paletteNum = 0,
        .baseBlock = 1 + 30 * 2,
    },
    DUMMY_WIN_TEMPLATE,
};

static EWRAM_DATA const u8 *sStatus = NULL;

// Tien Len value -> shared card id (suit * 13 + rank, rank 0 = '2', 12 = A;
// suits Hearts, Diamonds, Clubs, Spades)
static u8 ValueToCard(u8 value)
{
    u8 rank = (TL_RANK(value) + 1) % GC_RANKS;
    u8 suit = 3 - TL_SUIT(value);
    return suit * GC_RANKS + rank;
}

static u8 CardToValue(u8 card)
{
    u8 rank = (GC_CARD_RANK(card) + GC_RANKS - 1) % GC_RANKS;
    u8 suit = 3 - GC_CARD_SUIT(card);
    return rank * 4 + suit;
}

// ------------------------------------------------------------------
// Drawing

static void DrawOpponents(void)
{
    static const u8 sSeatX[TL_SEATS] = {0, 176, 92, 4}; // right, top (center), left
    u32 seat;

    FillWindowPixelBuffer(WIN_OPPONENTS, PIXEL_FILL(0));
    for (seat = 1; seat < TL_SEATS; seat++)
    {
        const u8 *color = (sTL->state == STATE_AI_WAIT && sTL->seat == seat) ? sColor_Gold : sColor_Normal;
        StringCopy(gStringVar1, sSeatNames[seat]);
        if (sTL->finished[seat])
            StringCopy(gStringVar2, sPlaceNames[sTL->place[seat]]);
        else
            ConvertIntToDecimalStringN(gStringVar2, sTL->handCount[seat], STR_CONV_MODE_LEFT_ALIGN, 2);
        StringExpandPlaceholders(gStringVar4, sText_Cards);
        AddTextPrinterParameterized3(WIN_OPPONENTS, FONT_SMALL, sSeatX[seat], 2, color, TEXT_SKIP_DRAW, gStringVar4);
        if (sTL->passed[seat] && sTL->cur.type != TL_NONE)
            AddTextPrinterParameterized3(WIN_OPPONENTS, FONT_SMALL, sSeatX[seat] + 40, 2, sColor_Red, TEXT_SKIP_DRAW, sText_Pass);
    }
    CopyWindowToVram(WIN_OPPONENTS, COPYWIN_FULL);
}

static void DrawStatus(void)
{
    const u8 *help;

    FillWindowPixelBuffer(WIN_STATUS, PIXEL_FILL(0));
    switch (sTL->state)
    {
    case STATE_BET:
        ConvertIntToDecimalStringN(gStringVar1, sTL->stake, STR_CONV_MODE_LEFT_ALIGN, 3);
        ConvertIntToDecimalStringN(gStringVar2, GetCoins(), STR_CONV_MODE_LEFT_ALIGN, 4);
        StringExpandPlaceholders(gStringVar4, sText_BetLine);
        help = (GetCoins() >= GC_CARD_STAKE_MIN) ? sText_HelpBet : sText_HelpBroke;
        break;
    case STATE_RESULT:
        StringCopy(gStringVar4, sStatus);
        StringCopy(gStringVar1, sTL->lastNet < 0 ? sText_Minus : sText_Plus);
        ConvertIntToDecimalStringN(gStringVar2, sTL->lastNet < 0 ? -sTL->lastNet : sTL->lastNet, STR_CONV_MODE_LEFT_ALIGN, 5);
        StringAppend(gStringVar1, gStringVar2);
        StringExpandPlaceholders(gStringVar3, (GetCoins() >= GC_CARD_STAKE_MIN) ? sText_PaidLine : sText_PaidLineBroke);
        help = gStringVar3;
        break;
    case STATE_PLAYER_TURN:
        StringCopy(gStringVar4, sStatus);
        help = sText_HelpTurn;
        break;
    default:
        StringCopy(gStringVar4, sStatus);
        help = sText_HelpWait;
        break;
    }
    AddTextPrinterParameterized3(WIN_STATUS, FONT_SMALL, 4, 0, sColor_Normal, TEXT_SKIP_DRAW, gStringVar4);
    AddTextPrinterParameterized3(WIN_STATUS, FONT_SMALL, 4, 12, sColor_Gold, TEXT_SKIP_DRAW, help);
    CopyWindowToVram(WIN_STATUS, COPYWIN_FULL);
}

static void PlaceRow(const u8 *spriteIds, u8 *shown, u16 tagBase, const u8 *values, u8 n, s16 y, bool8 isHand)
{
    u32 i;
    s16 left = (DISPLAY_WIDTH - ((n > 0 ? n - 1 : 0) * CARD_SPACING + 32)) / 2 + 16;

    for (i = 0; i < TL_HAND_MAX; i++)
    {
        struct Sprite *sprite = &gSprites[spriteIds[i]];
        if (i >= n)
        {
            sprite->invisible = TRUE;
            continue;
        }
        if (shown[i] != values[i])
        {
            GameCornerCards_LoadGfx(ValueToCard(values[i]), tagBase + i);
            shown[i] = values[i];
        }
        sprite->invisible = FALSE;
        sprite->x = left + i * CARD_SPACING;
        sprite->y = y;
        if (isHand)
        {
            if (sTL->selected & (1 << i))
                sprite->y -= RAISE_SELECTED;
            if (sTL->state == STATE_PLAYER_TURN && sTL->cursor == i)
                sprite->y -= RAISE_CURSOR;
        }
    }
}

static void DrawCards(void)
{
    PlaceRow(sTL->handSpriteIds, sTL->handShown, TAG_HAND_TILES, sTL->hand[TL_PLAYER], sTL->handCount[TL_PLAYER], HAND_Y, TRUE);
    PlaceRow(sTL->tableSpriteIds, sTL->tableShown, TAG_TABLE_TILES, sTL->cur.cards, (sTL->cur.type == TL_NONE) ? 0 : sTL->cur.count, TABLE_Y, FALSE);
}

static void RedrawAll(void)
{
    DrawOpponents();
    DrawStatus();
    DrawCards();
}

static void SetStatus(const u8 *text)
{
    sStatus = text;
}

// Buffers a status line into gStringVar4's sibling buffer so it survives redraws
static EWRAM_DATA u8 sStatusBuffer[100] = {0};

static void SetStatusExpanded(const u8 *template)
{
    StringExpandPlaceholders(sStatusBuffer, template);
    sStatus = sStatusBuffer;
}

// ------------------------------------------------------------------
// Game flow

static void BeginTurn(void);

static u16 ClampStake(u16 stake)
{
    u16 coins = GetCoins();

    if (stake > GC_CARD_STAKE_MAX)
        stake = GC_CARD_STAKE_MAX;
    while (stake > GC_CARD_STAKE_MIN && stake > coins)
        stake -= GC_CARD_STAKE_STEP;
    if (stake < GC_CARD_STAKE_MIN)
        stake = GC_CARD_STAKE_MIN;
    return stake;
}

// The hand ends once the player is out (or last), or on instant wins.
// `placeReturn` is what the placing hands back (the stake is already out of the
// pocket). Chops and stuck cards then pay from / to the pocket; a pocket that
// can't cover a debt is emptied and the rest is waived.
static void EndHand(u8 leader, bool8 playerWon, s32 placeReturn)
{
    s32 delta = placeReturn;
    u32 coins = GetCoins();

    sTL->winner = leader;
    delta += ((s32)sTL->bonusHalves - (s32)sTL->penaltyHalves - (s32)sTL->stuckHalves) * sTL->stake / 2;
    if (delta >= 0)
    {
        if (delta > MAX_COINS - coins)
            delta = MAX_COINS - coins;
        AddCoins(delta);
    }
    else
    {
        if (-delta > (s32)coins)
            delta = -(s32)coins;
        RemoveCoins(-delta);
    }
    sTL->lastNet = delta - sTL->stake; // the whole hand, stake included
    if (playerWon)
        PlayFanfare(MUS_SLOTS_JACKPOT);
    else
        PlaySE(SE_FAILURE);

    sTL->nextLeader = leader;
    sTL->firstHand = FALSE;
    sTL->state = STATE_RESULT;
    RedrawAll();
}

static void EndHandFinished(void)
{
    u8 winner = TL_PLAYER;
    u32 seat;

    for (seat = 0; seat < TL_SEATS; seat++)
    {
        if (sTL->place[seat] == 1)
            winner = seat;
    }
    // The player is out, or last when the three AI are out: then whatever
    // 2s and bombs they still hold are stuck (thoi)
    if (!sTL->finished[TL_PLAYER])
    {
        sTL->place[TL_PLAYER] = TL_SEATS;
        sTL->stuckHalves = TL_StuckValue(sTL->hand[TL_PLAYER], sTL->handCount[TL_PLAYER]);
    }
    StringCopy(gStringVar1, sSeatNames[winner]);
    StringCopy(gStringVar2, sPlaceNames[sTL->place[TL_PLAYER]]);
    ConvertIntToDecimalStringN(gStringVar3, sTL->bonusHalves * sTL->stake / 2, STR_CONV_MODE_LEFT_ALIGN, 5);
    StringAppend(gStringVar3, sText_ResultPenalty);
    ConvertIntToDecimalStringN(gStringVar4, sTL->penaltyHalves * sTL->stake / 2, STR_CONV_MODE_LEFT_ALIGN, 5);
    StringAppend(gStringVar3, gStringVar4);
    if (sTL->stuckHalves != 0)
    {
        StringAppend(gStringVar3, sText_ResultStuck);
        ConvertIntToDecimalStringN(gStringVar4, sTL->stuckHalves * sTL->stake / 2, STR_CONV_MODE_LEFT_ALIGN, 5);
        StringAppend(gStringVar3, gStringVar4);
    }
    SetStatusExpanded(sText_Result);
    // 4th pays 1st a stake, 3rd pays 2nd half a stake
    {
        static const s8 sPlaceHalves[TL_SEATS + 1] =
        {
            [1] =  GC_TIEN_LEN_FIRST_PAY_HALVES,
            [2] =  GC_TIEN_LEN_SECOND_PAY_HALVES,
            [3] = -GC_TIEN_LEN_SECOND_PAY_HALVES,
            [4] = -GC_TIEN_LEN_FIRST_PAY_HALVES,
        };
        u8 place = sTL->place[TL_PLAYER];
        EndHand(winner, place <= 2, (s32)sTL->stake + sPlaceHalves[place] * (s32)sTL->stake / 2);
    }
}

// After the pause that shows the last play: end the hand, or pass the turn on
static void ContinueAfterPause(void)
{
    bool8 trickOver;

    if (sTL->handOver)
    {
        EndHandFinished();
        return;
    }
    sTL->seat = TL_NextSeat(sTL->seat, sTL->curOwner, sTL->passed, sTL->finished, &trickOver);
    if (trickOver)
    {
        // Everyone still in passed: the owner leads (or the next seat, if the owner is out)
        sTL->cur.type = TL_NONE;
        sTL->cur.count = 0;
        memset(sTL->passed, 0, sizeof(sTL->passed));
    }
    BeginTurn();
}

static void StartPause(u8 frames)
{
    sTL->state = STATE_PAUSE;
    sTL->delay = frames;
    RedrawAll();
}

static void DoPlay(u8 seat, const struct TLCombo *combo)
{
    u8 beat = TL_Beats(combo, &sTL->cur);

    StringCopy(gStringVar1, sSeatNames[seat]);
    StringCopy(gStringVar2, sComboNames[combo->type]);
    if (beat == TL_BEAT_CHOP)
    {
        u8 value = TL_ChopValue(&sTL->cur);
        if (seat == TL_PLAYER)
            sTL->bonusHalves += value;
        if (sTL->curOwner == TL_PLAYER)
            sTL->penaltyHalves += value;
        SetStatusExpanded(sText_Chopped);
        PlaySE(SE_BANG);
    }
    else
    {
        SetStatusExpanded(sText_Played);
        PlaySE(SE_CARD);
    }
    TL_RemoveCards(sTL->hand[seat], &sTL->handCount[seat], combo);
    sTL->cur = *combo;
    sTL->curOwner = seat;
    sTL->mustThreeSpades = FALSE;
    if (seat == TL_PLAYER)
        sTL->selected = 0;
    if (sTL->handCount[seat] == 0)
    {
        sTL->finished[seat] = TRUE;
        sTL->place[seat] = ++sTL->numOut;
        StringCopy(gStringVar1, sSeatNames[seat]);
        StringCopy(gStringVar2, sPlaceNames[sTL->place[seat]]);
        SetStatusExpanded(sText_IsOut);
        // Play on until the player is out, or is the last one holding cards
        if (seat == TL_PLAYER || sTL->numOut == TL_SEATS - 1)
            sTL->handOver = TRUE;
    }
    StartPause(PLAY_PAUSE);
}

static void DoPass(u8 seat)
{
    StringCopy(gStringVar1, sSeatNames[seat]);
    SetStatusExpanded(sText_Passed);
    sTL->passed[seat] = TRUE;
    StartPause(PASS_PAUSE);
}

static void BeginTurn(void)
{
    if (sTL->seat == TL_PLAYER)
    {
        sTL->state = STATE_PLAYER_TURN;
        if (sTL->cursor >= sTL->handCount[TL_PLAYER])
            sTL->cursor = sTL->handCount[TL_PLAYER] - 1;
        // Keep the last opponent's play on screen; add whose turn it is
        if (sTL->cur.type == TL_NONE)
            SetStatus(sTL->mustThreeSpades ? sText_YourLeadThreeSpades : sText_YourLead);
    }
    else
    {
        sTL->state = STATE_AI_WAIT;
        sTL->delay = AI_DELAY;
    }
    RedrawAll();
}

static void StartHand(void)
{
    u32 seat, i;

    RemoveCoins(sTL->stake);
    GameCornerCards_Shuffle(sTL->deck);
    for (seat = 0; seat < TL_SEATS; seat++)
    {
        for (i = 0; i < TL_HAND_MAX; i++)
            sTL->hand[seat][i] = CardToValue(sTL->deck[seat * TL_HAND_MAX + i]);
        sTL->handCount[seat] = TL_HAND_MAX;
        TL_Sort(sTL->hand[seat], TL_HAND_MAX);
        sTL->passed[seat] = FALSE;
        sTL->finished[seat] = FALSE;
        sTL->place[seat] = 0;
    }
    sTL->numOut = 0;
    sTL->handOver = FALSE;
    sTL->cur.type = TL_NONE;
    sTL->cur.count = 0;
    sTL->curOwner = TL_PLAYER;
    sTL->selected = 0;
    sTL->cursor = 0;
    sTL->bonusHalves = 0;
    sTL->penaltyHalves = 0;
    sTL->stuckHalves = 0;
    PlaySE(SE_CARD);

    // Instant wins end the hand at once. Every seat is checked; ties split the pot.
    {
        static const u8 *const sInstantTexts[] =
        {
            [TL_INSTANT_FOUR_TWOS] = sText_FourTwos,
            [TL_INSTANT_DRAGON]    = sText_Dragon,
            [TL_INSTANT_SIX_PAIRS] = sText_SixPairs,
            [TL_INSTANT_ONE_SUIT]  = sText_OneSuit,
        };
        u8 ways = 0, first = TL_SEATS, kind = TL_INSTANT_NONE;
        bool8 playerWon = FALSE;

        gStringVar1[0] = EOS;
        for (seat = 0; seat < TL_SEATS; seat++)
        {
            u8 instant = TL_InstantWin(sTL->hand[seat], TL_HAND_MAX);
            if (instant == TL_INSTANT_NONE)
                continue;
            if (ways++ != 0)
                StringAppend(gStringVar1, sText_NameJoin);
            StringAppend(gStringVar1, sSeatNames[seat]);
            if (first == TL_SEATS)
            {
                first = seat;
                kind = instant;
            }
            if (seat == TL_PLAYER)
                playerWon = TRUE;
        }
        if (ways != 0)
        {
            s32 winPayout = 0;

            // Everyone without an instant win finishes last and pays a stake; the
            // instant winners split that (a sole one wins 3 stakes) and get theirs back
            if (playerWon)
                winPayout = (s32)sTL->stake + (s32)sTL->stake * GC_TIEN_LEN_FIRST_PAY_HALVES / 2 * (TL_SEATS - ways) / ways;
            else
                sTL->stuckHalves = TL_StuckValue(sTL->hand[TL_PLAYER], TL_HAND_MAX);
            if (ways == 1)
            {
                SetStatusExpanded(sInstantTexts[kind]);
            }
            else
            {
                ConvertIntToDecimalStringN(gStringVar2, ways, STR_CONV_MODE_LEFT_ALIGN, 1);
                SetStatusExpanded(sText_SplitInstant);
            }
            EndHand(first, playerWon, winPayout);
            return;
        }
    }

    if (sTL->firstHand)
    {
        // Whoever holds the 3 of spades leads it
        for (seat = 0; seat < TL_SEATS; seat++)
        {
            if (sTL->hand[seat][0] == TL_THREE_SPADES)
                sTL->seat = seat;
        }
        sTL->mustThreeSpades = TRUE;
    }
    else
    {
        sTL->seat = sTL->nextLeader;
        sTL->mustThreeSpades = FALSE;
    }
    sTL->curOwner = sTL->seat;
    SetStatus(sText_HelpWait);
    BeginTurn();
}

static void RunAiTurn(void)
{
    struct TLCombo play;
    u8 seat = sTL->seat;
    u8 fewest = TL_HAND_MAX;
    u32 i;

    for (i = 0; i < TL_SEATS; i++)
    {
        if (i != seat && !sTL->finished[i] && sTL->handCount[i] < fewest)
            fewest = sTL->handCount[i];
    }
    if (TL_AiChoose(sTL->hand[seat], sTL->handCount[seat], &sTL->cur, sTL->mustThreeSpades,
                    fewest, Random() % 3, sTL->moves, &play))
        DoPlay(seat, &play);
    else
        DoPass(seat);
}

static void TryPlayerPlay(void)
{
    struct TLCombo play;
    u8 cards[TL_HAND_MAX];
    u8 n = 0;
    u32 i;

    for (i = 0; i < sTL->handCount[TL_PLAYER]; i++)
    {
        if (sTL->selected & (1 << i))
            cards[n++] = sTL->hand[TL_PLAYER][i];
    }
    if (!TL_Classify(cards, n, &play) || play.type == TL_NONE)
        SetStatus(sText_NotAHand);
    else if (sTL->mustThreeSpades && !TL_ComboHas(&play, TL_THREE_SPADES))
        SetStatus(sText_NeedThreeSpades);
    else if (TL_Beats(&play, &sTL->cur) == TL_BEAT_NO)
        SetStatus(sText_DoesntBeat);
    else
    {
        DoPlay(TL_PLAYER, &play);
        return;
    }
    PlaySE(SE_FAILURE);
    DrawStatus();
}

static void StartExit(void)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    sTL->state = STATE_FADE_OUT;
}

static void Task_TienLen(u8 taskId)
{
    switch (sTL->state)
    {
    case STATE_FADE_IN:
        if (!gPaletteFade.active)
        {
            sTL->state = STATE_BET;
            DrawStatus();
        }
        break;
    case STATE_BET:
        if (JOY_NEW(B_BUTTON))
        {
            PlaySE(SE_SELECT);
            StartExit();
        }
        else if (GetCoins() < GC_CARD_STAKE_MIN)
        {
        }
        else if (JOY_NEW(A_BUTTON))
        {
            StartHand();
        }
        else if (JOY_REPEAT(DPAD_UP) && sTL->stake + GC_CARD_STAKE_STEP <= GC_CARD_STAKE_MAX
              && sTL->stake + GC_CARD_STAKE_STEP <= GetCoins())
        {
            PlaySE(SE_SELECT);
            sTL->stake += GC_CARD_STAKE_STEP;
            DrawStatus();
        }
        else if (JOY_REPEAT(DPAD_DOWN) && sTL->stake - GC_CARD_STAKE_STEP >= GC_CARD_STAKE_MIN)
        {
            PlaySE(SE_SELECT);
            sTL->stake -= GC_CARD_STAKE_STEP;
            DrawStatus();
        }
        break;
    case STATE_PLAYER_TURN:
        if (JOY_NEW(START_BUTTON))
        {
            TryPlayerPlay();
        }
        else if (JOY_NEW(A_BUTTON))
        {
            PlaySE(SE_SELECT);
            sTL->selected ^= (1 << sTL->cursor);
            DrawCards();
        }
        else if (JOY_NEW(B_BUTTON))
        {
            if (sTL->cur.type == TL_NONE)
            {
                PlaySE(SE_FAILURE);
                SetStatus(sText_MustLead);
                DrawStatus();
            }
            else
            {
                sTL->selected = 0;
                DoPass(TL_PLAYER);
            }
        }
        else if (JOY_NEW(SELECT_BUTTON))
        {
            sTL->selected = 0;
            DrawCards();
        }
        else if (JOY_REPEAT(DPAD_LEFT) && sTL->cursor > 0)
        {
            PlaySE(SE_SELECT);
            sTL->cursor--;
            DrawCards();
        }
        else if (JOY_REPEAT(DPAD_RIGHT) && sTL->cursor + 1 < sTL->handCount[TL_PLAYER])
        {
            PlaySE(SE_SELECT);
            sTL->cursor++;
            DrawCards();
        }
        break;
    case STATE_AI_WAIT:
        if (--sTL->delay == 0)
            RunAiTurn();
        break;
    case STATE_PAUSE:
        // A skips the rest of the pause
        if (--sTL->delay == 0 || JOY_NEW(A_BUTTON))
            ContinueAfterPause();
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
            sTL->stake = ClampStake(sTL->stake);
            sTL->handCount[TL_PLAYER] = 0;
            sTL->cur.type = TL_NONE;
            memset(sTL->passed, 0, sizeof(sTL->passed));
            memset(sTL->finished, 0, sizeof(sTL->finished));
            sTL->state = STATE_BET;
            RedrawAll();
        }
        break;
    case STATE_FADE_OUT:
        if (!gPaletteFade.active)
        {
            DestroyTask(taskId);
            SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
            Free(GetBgTilemapBuffer(0));
            FreeAllWindowBuffers();
            FreeAllSpritePalettes();
            FREE_AND_SET_NULL(sTL);
        }
        break;
    }
}

// ------------------------------------------------------------------
// Setup

static void CB2_TienLen(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

static void VBlankCB_TienLen(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void InitTienLenScreen(void)
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

    InitBgsFromTemplates(0, sTLBgTemplates, ARRAY_COUNT(sTLBgTemplates));
    SetBgTilemapBuffer(0, AllocZeroed(BG_SCREEN_SIZE));
    InitWindows(sTLWinTemplates);
    DeactivateAllTextPrinters();
    LoadPalette(sTienLenBgPal, BG_PLTT_ID(0), sizeof(sTienLenBgPal));
    LoadSpritePalette(&(const struct SpritePalette){gGameCornerCards_Pal, TAG_CARD_PAL});
    PutWindowTilemap(WIN_OPPONENTS);
    PutWindowTilemap(WIN_STATUS);

    // Cards further right sit on top, so each card's corner index stays visible
    for (i = 0; i < TL_HAND_MAX; i++)
    {
        GameCornerCards_LoadGfx(GC_CARD_FACE_DOWN, TAG_HAND_TILES + i);
        GameCornerCards_LoadGfx(GC_CARD_FACE_DOWN, TAG_TABLE_TILES + i);
        sTL->handSpriteIds[i] = GameCornerCards_CreateSprite(TAG_HAND_TILES + i, TAG_CARD_PAL, 0, HAND_Y, 40 - i);
        sTL->tableSpriteIds[i] = GameCornerCards_CreateSprite(TAG_TABLE_TILES + i, TAG_CARD_PAL, 0, TABLE_Y, 40 - i);
        gSprites[sTL->handSpriteIds[i]].invisible = TRUE;
        gSprites[sTL->tableSpriteIds[i]].invisible = TRUE;
        sTL->handShown[i] = NOT_SHOWN;
        sTL->tableShown[i] = NOT_SHOWN;
    }
    sTL->firstHand = TRUE;
    sTL->stake = ClampStake(GC_CARD_STAKE_MIN);
    sTL->state = STATE_FADE_IN;
    sStatus = sText_HelpWait;
    RedrawAll();

    CopyBgTilemapBufferToVram(0);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON | DISPCNT_BG0_ON);
    ShowBg(0);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(VBlankCB_TienLen);
    SetMainCallback2(CB2_TienLen);
    CreateTask(Task_TienLen, 0);
}

static void Task_FadeToTienLen(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(taskId);
        InitTienLenScreen();
    }
}

void StartTienLen(void)
{
    sTL = AllocZeroed(sizeof(*sTL));
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    CreateTask(Task_FadeToTienLen, 0);
}
