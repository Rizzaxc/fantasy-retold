#include "global.h"
#include "malloc.h"
#include "battle.h"
#include "pokemon.h"
#include "battle_controllers.h"
#include "battle_interface.h"
#include "battle_z_move.h"
#include "battle_util.h"
#include "battle_main.h"
#include "graphics.h"
#include "sprite.h"
#include "window.h"
#include "string_util.h"
#include "text.h"
#include "sound.h"
#include "decompress.h"
#include "task.h"
#include "util.h"
#include "gpu_regs.h"
#include "battle_message.h"
#include "pokedex.h"
#include "palette.h"
#include "international_string_util.h"
#include "safari_zone.h"
#include "battle_anim.h"
#include "data.h"
#include "pokemon_summary_screen.h"
#include "strings.h"
#include "battle_debug.h"
#include "item.h"
#include "item_icon.h"
#include "item_use.h"
#include "test_runner.h"
#include "constants/battle_anim.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/items.h"
#include "caps.h"
#include "status_rail.h"

// Fantasy Retold healthbox (design.md, "Healthbox layout"). Each box is a dark corner-cut plate drawn at
// the left of the two healthbox sprites (a 128px-wide canvas); generate_healthbox_art.py draws the static
// plate, bar frames, Recuperate tick and EXP track, and everything else is drawn here in canvas pixels.
// The HB_* layout values below must match that script.

// Healthbox palette slots (ball_status_bar.png)
#define HB_COLOR_PLATE   1  // plate fill; also clears text
#define HB_COLOR_EMPTY   2  // empty gauge / status cell background
#define HB_COLOR_INK     3  // outlines, text shadow
#define HB_COLOR_SLATE   4  // recoverable HP loss, unlit stage pips
#define HB_COLOR_WHITE   5  // text
#define HB_COLOR_RED     7  // current HP and the HP bar frame, female symbol, exhausted Stamina frame
#define HB_COLOR_GREEN   8  // Stamina
#define HB_COLOR_PURPLE  9  // Trauma
#define HB_COLOR_CYAN    10 // EXP
#define HB_COLOR_ORANGE  11
#define HB_COLOR_BLUE    12 // male symbol
#define HB_COLOR_YELLOW  13 // Stamina at or below the Recuperate threshold
#define HB_COLOR_SKY     14
#define HB_COLOR_PINK    15

#define HEALTHBOX_BG_INDEX HB_COLOR_PLATE

struct HealthboxLayout
{
    u8 plateW;
    u8 plateH;
    u8 barX;    // HP / Stamina bar interior left
    u8 barW;    // HP / Stamina bar interior width
    u8 hpY;     // HP bar interior top (4 rows)
    u8 staY;    // Stamina bar interior top (2 rows)
    bool8 hpNumbers;
    bool8 exp;
};

enum
{
    HB_LAYOUT_PLAYER,         // doubles player
    HB_LAYOUT_PLAYER_SINGLES, // + EXP track
    HB_LAYOUT_OPPONENT,
};

static const struct HealthboxLayout sHealthboxLayouts[] =
{
    [HB_LAYOUT_PLAYER]         = {.plateW = 104, .plateH = 32, .barX = 5, .barW = 94, .hpY = 13, .staY = 20, .hpNumbers = TRUE},
    [HB_LAYOUT_PLAYER_SINGLES] = {.plateW = 104, .plateH = 32, .barX = 5, .barW = 94, .hpY = 13, .staY = 20, .hpNumbers = TRUE, .exp = TRUE},
    [HB_LAYOUT_OPPONENT]       = {.plateW = 100, .plateH = 25, .barX = 5, .barW = 90, .hpY = 13, .staY = 20},
};

#define HB_TEXT_Y        0  // printer y of the name / level row (small font caps land on rows 4-10)
#define HB_HP_TEXT_Y     20 // printer y of the HP numbers: digits on rows 24-30, a 1px gap under the Stamina frame;
                            // their ink shadow (row 31) blends into the plate's ink bottom edge
#define HB_EXP_X         6
#define HB_EXP_Y         28
#define HB_EXP_W         50
#define HB_BALL_ICON_W   7  // caught-ball icon before the opponent's name (5px + gap)

// Status column: the old healthbar sprite, re-purposed as a 64x32 box beside the plate that holds one
// cell per active status line. It grows away from the plate (left for the player, right for the opponent).
#define HB_STATUS_CELL_W 12
#define HB_STATUS_CELL_H 9
#define HB_STATUS_COLUMN_GAP 2

STATIC_ASSERT(!B_HP_PERCENTAGE_DISPLAY, FantasyRetoldHealthboxHasNoHpPercentage) // the plates have no room for it

enum
{   // Corresponds to gHealthboxElementsGfxTable (and the tables after it) in graphics.c
    // These are indexes into the tables, which are filled with 8x8 square pixel data.
    HEALTHBOX_GFX_0, //hp bar [black section]
    HEALTHBOX_GFX_1, //hp bar "H"
    HEALTHBOX_GFX_2, //hp bar "P"
    HEALTHBOX_GFX_HP_BAR_GREEN, //hp bar [0 pixels]
    HEALTHBOX_GFX_4,  //hp bar [1 pixels]
    HEALTHBOX_GFX_5,  //hp bar [2 pixels]
    HEALTHBOX_GFX_6,  //hp bar [3 pixels]
    HEALTHBOX_GFX_7,  //hp bar [4 pixels]
    HEALTHBOX_GFX_8,  //hp bar [5 pixels]
    HEALTHBOX_GFX_9,  //hp bar [6 pixels]
    HEALTHBOX_GFX_10, //hp bar [7 pixels]
    HEALTHBOX_GFX_11, //hp bar [8 pixels]
    HEALTHBOX_GFX_12, //exp bar [0 pixels]
    HEALTHBOX_GFX_13, //exp bar [1 pixels]
    HEALTHBOX_GFX_14, //exp bar [2 pixels]
    HEALTHBOX_GFX_15, //exp bar [3 pixels]
    HEALTHBOX_GFX_16, //exp bar [4 pixels]
    HEALTHBOX_GFX_17, //exp bar [5 pixels]
    HEALTHBOX_GFX_18, //exp bar [6 pixels]
    HEALTHBOX_GFX_19, //exp bar [7 pixels]
    HEALTHBOX_GFX_20, //exp bar [8 pixels]
    HEALTHBOX_GFX_STATUS_PSN_BATTLER0,  //status psn "(P"
    HEALTHBOX_GFX_22,                   //status psn "SN"
    HEALTHBOX_GFX_23,                   //status psn "|)"
    HEALTHBOX_GFX_STATUS_PRZ_BATTLER0,  //status prz
    HEALTHBOX_GFX_25,
    HEALTHBOX_GFX_26,
    HEALTHBOX_GFX_STATUS_SLP_BATTLER0,  //status slp
    HEALTHBOX_GFX_28,
    HEALTHBOX_GFX_29,
    HEALTHBOX_GFX_STATUS_FRZ_BATTLER0,  //status frz
    HEALTHBOX_GFX_31,
    HEALTHBOX_GFX_32,
    HEALTHBOX_GFX_STATUS_BRN_BATTLER0,  //status brn
    HEALTHBOX_GFX_34,
    HEALTHBOX_GFX_35,
    HEALTHBOX_GFX_STATUS_FRB_BATTLER0,  //status frb
    HEALTHBOX_GFX_116,
    HEALTHBOX_GFX_117,
    HEALTHBOX_GFX_36, //misc [Black section]
    HEALTHBOX_GFX_37, //misc [Black section]
    HEALTHBOX_GFX_38, //misc [Black section]
    HEALTHBOX_GFX_39, //misc [Blank Health Window?]
    HEALTHBOX_GFX_40, //misc [Blank Health Window?]
    HEALTHBOX_GFX_41, //misc [Blank Health Window?]
    HEALTHBOX_GFX_42, //misc [Blank Health Window?]
    HEALTHBOX_GFX_43, //misc [Top of Health Window?]
    HEALTHBOX_GFX_44, //misc [Top of Health Window?]
    HEALTHBOX_GFX_45, //misc [Top of Health Window?]
    HEALTHBOX_GFX_46, //misc [Blank Health Window?]
    HEALTHBOX_GFX_HP_BAR_YELLOW, //hp bar yellow [0 pixels]
    HEALTHBOX_GFX_48, //hp bar yellow [1 pixels]
    HEALTHBOX_GFX_49, //hp bar yellow [2 pixels]
    HEALTHBOX_GFX_50, //hp bar yellow [3 pixels]
    HEALTHBOX_GFX_51, //hp bar yellow [4 pixels]
    HEALTHBOX_GFX_52, //hp bar yellow [5 pixels]
    HEALTHBOX_GFX_53, //hp bar yellow [6 pixels]
    HEALTHBOX_GFX_54, //hp bar yellow [7 pixels]
    HEALTHBOX_GFX_55, //hp bar yellow [8 pixels]
    HEALTHBOX_GFX_HP_BAR_RED,  //hp bar red [0 pixels]
    HEALTHBOX_GFX_57, //hp bar red [1 pixels]
    HEALTHBOX_GFX_58, //hp bar red [2 pixels]
    HEALTHBOX_GFX_59, //hp bar red [3 pixels]
    HEALTHBOX_GFX_60, //hp bar red [4 pixels]
    HEALTHBOX_GFX_61, //hp bar red [5 pixels]
    HEALTHBOX_GFX_62, //hp bar red [6 pixels]
    HEALTHBOX_GFX_63, //hp bar red [7 pixels]
    HEALTHBOX_GFX_64, //hp bar red [8 pixels]
    HEALTHBOX_GFX_65, //hp bar frame end
    HEALTHBOX_GFX_STATUS_BALL, // Full
    HEALTHBOX_GFX_STATUS_BALL_EMPTY,
    HEALTHBOX_GFX_STATUS_BALL_FAINTED,
    HEALTHBOX_GFX_STATUS_BALL_STATUSED,
    HEALTHBOX_GFX_STATUS_BALL_CAUGHT,
    HEALTHBOX_GFX_STATUS_PSN_BATTLER1, //status2 "PSN"
    HEALTHBOX_GFX_72,
    HEALTHBOX_GFX_73,
    HEALTHBOX_GFX_STATUS_PRZ_BATTLER1, //status2 "PRZ"
    HEALTHBOX_GFX_75,
    HEALTHBOX_GFX_76,
    HEALTHBOX_GFX_STATUS_SLP_BATTLER1, //status2 "SLP"
    HEALTHBOX_GFX_78,
    HEALTHBOX_GFX_79,
    HEALTHBOX_GFX_STATUS_FRZ_BATTLER1, //status2 "FRZ"
    HEALTHBOX_GFX_81,
    HEALTHBOX_GFX_82,
    HEALTHBOX_GFX_STATUS_BRN_BATTLER1, //status2 "BRN"
    HEALTHBOX_GFX_84,
    HEALTHBOX_GFX_85,
    HEALTHBOX_GFX_STATUS_FRB_BATTLER1, //status2 "FRB"
    HEALTHBOX_GFX_118,
    HEALTHBOX_GFX_119,
    HEALTHBOX_GFX_STATUS_PSN_BATTLER2, //status3 "PSN"
    HEALTHBOX_GFX_87,
    HEALTHBOX_GFX_88,
    HEALTHBOX_GFX_STATUS_PRZ_BATTLER2, //status3 "PRZ"
    HEALTHBOX_GFX_90,
    HEALTHBOX_GFX_91,
    HEALTHBOX_GFX_STATUS_SLP_BATTLER2, //status3 "SLP"
    HEALTHBOX_GFX_93,
    HEALTHBOX_GFX_94,
    HEALTHBOX_GFX_STATUS_FRZ_BATTLER2, //status3 "FRZ"
    HEALTHBOX_GFX_96,
    HEALTHBOX_GFX_97,
    HEALTHBOX_GFX_STATUS_BRN_BATTLER2, //status3 "BRN"
    HEALTHBOX_GFX_99,
    HEALTHBOX_GFX_100,
    HEALTHBOX_GFX_STATUS_FRB_BATTLER2, //status3 "FRB"
    HEALTHBOX_GFX_120,
    HEALTHBOX_GFX_121,
    HEALTHBOX_GFX_STATUS_PSN_BATTLER3, //status4 "PSN"
    HEALTHBOX_GFX_102,
    HEALTHBOX_GFX_103,
    HEALTHBOX_GFX_STATUS_PRZ_BATTLER3, //status4 "PRZ"
    HEALTHBOX_GFX_105,
    HEALTHBOX_GFX_106,
    HEALTHBOX_GFX_STATUS_SLP_BATTLER3, //status4 "SLP"
    HEALTHBOX_GFX_108,
    HEALTHBOX_GFX_109,
    HEALTHBOX_GFX_STATUS_FRZ_BATTLER3, //status4 "FRZ"
    HEALTHBOX_GFX_111,
    HEALTHBOX_GFX_112,
    HEALTHBOX_GFX_STATUS_BRN_BATTLER3, //status4 "BRN"
    HEALTHBOX_GFX_114,
    HEALTHBOX_GFX_115,
    HEALTHBOX_GFX_STATUS_FRB_BATTLER3, //status4 "FRB"
    HEALTHBOX_GFX_122,
    HEALTHBOX_GFX_123,
    HEALTHBOX_GFX_PLAYER_FRAME_END,
    HEALTHBOX_GFX_PLAYER_FRAME_END_BAR,
    HEALTHBOX_GFX_OPPONENT_FRAME_END,
    HEALTHBOX_GFX_OPPONENT_FRAME_END_BAR,
};

static const u8 *GetHealthboxElementGfxPtr(u8);

static void UpdateStatusIconInHealthbox(u8);
static bool32 ShouldShowCaughtBallIcon(enum BattlerId battler);


static void Task_HidePartyStatusSummary_BattleStart_1(u8);
static void Task_HidePartyStatusSummary_BattleStart_2(u8);
static void Task_HidePartyStatusSummary_DuringBattle(u8);

static void SpriteCB_HealthBoxOther(struct Sprite *);
static void SpriteCB_HealthBar(struct Sprite *);
static void DrawStaminaGaugeOnHealthbox(u32 healthboxSpriteId, enum BattlerId battler);
static void DrawHealthboxHpBar(u32 healthboxSpriteId, enum BattlerId battler, s32 barMaxHP, u32 hpPixels);
static void SpriteCB_StatusSummaryBar_Enter(struct Sprite *);
static void SpriteCB_StatusSummaryBar_Exit(struct Sprite *);
static void SpriteCB_StatusSummaryBalls_Enter(struct Sprite *);
static void SpriteCB_StatusSummaryBalls_Exit(struct Sprite *);
static void SpriteCB_StatusSummaryBalls_OnSwitchout(struct Sprite *);

static s32 CalcNewBarValue(s32, s32, s32, s32 *, u8, u16);
static u8 GetScaledExpFraction(s32, s32, s32, u8);
static void MoveBattleBarGraphically(enum BattlerId, u8);

static void SpriteCb_AbilityPopUp(struct Sprite *);
static void Task_FreeAbilityPopUpGfx(u8);

static void SpriteCB_LastUsedBall(struct Sprite *);
static void SpriteCB_LastUsedBallWin(struct Sprite *);
static void SpriteCB_MoveInfoWin(struct Sprite *sprite);

static const struct OamData sOamData_64x32 =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x32),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(64x32),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};

static const struct SpriteTemplate sHealthboxPlayerSpriteTemplates[2] =
{
    {
        .tileTag = TAG_HEALTHBOX_PLAYER1_TILE,
        .paletteTag = TAG_HEALTHBOX_PAL,
        .oam = &sOamData_64x32,
    },
    {
        .tileTag = TAG_HEALTHBOX_PLAYER2_TILE,
        .paletteTag = TAG_HEALTHBOX_PAL,
        .oam = &sOamData_64x32,
    }
};

static const struct SpriteTemplate sHealthboxOpponentSpriteTemplates[2] =
{
    {
        .tileTag = TAG_HEALTHBOX_OPPONENT1_TILE,
        .paletteTag = TAG_HEALTHBOX_PAL,
        .oam = &sOamData_64x32,
    },
    {
        .tileTag = TAG_HEALTHBOX_OPPONENT2_TILE,
        .paletteTag = TAG_HEALTHBOX_PAL,
        .oam = &sOamData_64x32,
    }
};

static const struct SpriteTemplate sHealthboxSafariSpriteTemplate =
{
    .tileTag = TAG_HEALTHBOX_SAFARI_TILE,
    .paletteTag = TAG_HEALTHBOX_PAL,
    .oam = &sOamData_64x32,
};

// Fantasy Retold: the healthbar sprite is the status column (the HP bar is drawn into the healthbox itself).
static const struct SpriteTemplate sHealthbarSpriteTemplates[MAX_BATTLERS_COUNT] =
{
    {
        .tileTag = TAG_HEALTHBAR_PLAYER1_TILE,
        .paletteTag = TAG_HEALTHBOX_PAL,
        .oam = &sOamData_64x32,
        .callback = SpriteCB_HealthBar
    },
    {
        .tileTag = TAG_HEALTHBAR_OPPONENT1_TILE,
        .paletteTag = TAG_HEALTHBOX_PAL,
        .oam = &sOamData_64x32,
        .callback = SpriteCB_HealthBar
    },
    {
        .tileTag = TAG_HEALTHBAR_PLAYER2_TILE,
        .paletteTag = TAG_HEALTHBOX_PAL,
        .oam = &sOamData_64x32,
        .callback = SpriteCB_HealthBar
    },
    {
        .tileTag = TAG_HEALTHBAR_OPPONENT2_TILE,
        .paletteTag = TAG_HEALTHBOX_PAL,
        .oam = &sOamData_64x32,
        .callback = SpriteCB_HealthBar
    }
};
/*                      v-- Origin
[0     ][1     ][2     ][3     ]   128x8
*/
static const struct Subsprite sStatusSummaryBar_Subsprites_Enter[] =
{
    {
        .x = 32 * -3,
        .y = 0,
        .shape = SPRITE_SHAPE(32x8),
        .size = SPRITE_SIZE(32x8),
        .tileOffset = 0,
        .priority = 1
    },
    {
        .x = 32 * -2,
        .y = 0,
        .shape = SPRITE_SHAPE(32x8),
        .size = SPRITE_SIZE(32x8),
        .tileOffset = 4,
        .priority = 1
    },
    {
        .x = 32 * -1,
        .y = 0,
        .shape = SPRITE_SHAPE(32x8),
        .size = SPRITE_SIZE(32x8),
        .tileOffset = 8,
        .priority = 1
    },
    {
        .x = 0,
        .y = 0,
        .shape = SPRITE_SHAPE(32x8),
        .size = SPRITE_SIZE(32x8),
        .tileOffset = 12,
        .priority = 1
    }
};

/*                      v-- Origin
[0     ][1     ][2     ][3     ][4     ][5     ]   192x8
                 ^-- uses same tiles --^
*/
static const struct Subsprite sStatusSummaryBar_Subsprites_Exit[] =
{
    {
        .x = 32 * -3,
        .y = 0,
        .shape = SPRITE_SHAPE(32x8),
        .size = SPRITE_SIZE(32x8),
        .tileOffset = 0,
        .priority = 1
    },
    {
        .x = 32 * -2,
        .y = 0,
        .shape = SPRITE_SHAPE(32x8),
        .size = SPRITE_SIZE(32x8),
        .tileOffset = 4,
        .priority = 1
    },
    {
        .x = 32 * -1,
        .y = 0,
        .shape = SPRITE_SHAPE(32x8),
        .size = SPRITE_SIZE(32x8),
        .tileOffset = 8,
        .priority = 1
    },
    {
        .x = 32 * 0,
        .y = 0,
        .shape = SPRITE_SHAPE(32x8),
        .size = SPRITE_SIZE(32x8),
        .tileOffset = 8,
        .priority = 1
    },
    {
        .x = 32 * 1,
        .y = 0,
        .shape = SPRITE_SHAPE(32x8),
        .size = SPRITE_SIZE(32x8),
        .tileOffset = 8,
        .priority = 1
    },
    {
        .x = 32 * 2,
        .y = 0,
        .shape = SPRITE_SHAPE(32x8),
        .size = SPRITE_SIZE(32x8),
        .tileOffset = 12,
        .priority = 1
    }
};

static const struct SubspriteTable sStatusSummaryBar_SubspriteTable_Enter[] =
{
    {ARRAY_COUNT(sStatusSummaryBar_Subsprites_Enter), sStatusSummaryBar_Subsprites_Enter}
};

static const struct SubspriteTable sStatusSummaryBar_SubspriteTable_Exit[] =
{
    {ARRAY_COUNT(sStatusSummaryBar_Subsprites_Exit), sStatusSummaryBar_Subsprites_Exit}
};

static const struct CompressedSpriteSheet sStatusSummaryBarSpriteSheet =
{
    gBattleInterface_BallStatusBarGfx, 0x200, TAG_STATUS_SUMMARY_BAR_TILE
};

static const struct SpritePalette sStatusSummaryBarSpritePal =
{
    gBattleInterface_BallStatusBarPal, TAG_STATUS_SUMMARY_BAR_PAL
};

static const struct SpritePalette sStatusSummaryBallsSpritePal =
{
    gBattleInterface_BallDisplayPal, TAG_STATUS_SUMMARY_BALLS_PAL
};

static const struct SpriteSheet sStatusSummaryBallsSpriteSheet =
{
    &gHealthboxElementsGfxTable[HEALTHBOX_GFX_STATUS_BALL], 0x80, TAG_STATUS_SUMMARY_BALLS_TILE
};

static const struct OamData sOamData_StatusSummaryBalls =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(8x8),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(8x8),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};

static const struct SpriteTemplate sStatusSummaryBarSpriteTemplates[2] =
{
    { // Player
        .tileTag = TAG_STATUS_SUMMARY_BAR_TILE,
        .paletteTag = TAG_STATUS_SUMMARY_BAR_PAL,
        .oam = &sOamData_64x32,
        .callback = SpriteCB_StatusSummaryBar_Enter
    },
    { // Opponent
        .tileTag = TAG_STATUS_SUMMARY_BAR_TILE,
        .paletteTag = TAG_STATUS_SUMMARY_BAR_PAL,
        .oam = &sOamData_64x32,
        .callback = SpriteCB_StatusSummaryBar_Enter
    }
};

static const struct SpriteTemplate sStatusSummaryBallsSpriteTemplates[2] =
{
    {
        .tileTag = TAG_STATUS_SUMMARY_BALLS_TILE,
        .paletteTag = TAG_STATUS_SUMMARY_BALLS_PAL,
        .oam = &sOamData_StatusSummaryBalls,
        .callback = SpriteCB_StatusSummaryBalls_Enter
    },
    {
        .tileTag = TAG_STATUS_SUMMARY_BALLS_TILE,
        .paletteTag = TAG_STATUS_SUMMARY_BALLS_PAL,
        .oam = &sOamData_StatusSummaryBalls,
        .callback = SpriteCB_StatusSummaryBalls_Enter
    }
};

static const u8 sEmptyWhiteText_GrayHighlight[] = __("{COLOR WHITE}{BACKGROUND DARK_GRAY}{ACCENT DARK_GRAY}              ");
static const u8 sEmptyWhiteText_TransparentHighlight[] = __("{COLOR WHITE}{BACKGROUND TRANSPARENT}{ACCENT TRANSPARENT}              ");

// Fantasy Retold: the status lines of combat.md §6, in the order their cells fill the status column.
enum HealthboxStatusLine
{
    HB_STATUS_SOAK,
    HB_STATUS_CHILL,     // Chilled -> Frozen | Frostbitten
    HB_STATUS_BURN,
    HB_STATUS_POISON,    // Poison -> Toxic
    HB_STATUS_ELECTRIC,  // Dazed -> Shocked -> Paralyzed
    HB_STATUS_SLEEP,     // Drowsy -> Asleep -> Nightmare
    HB_STATUS_CONFUSION, // Confused -> Stupefied -> Frightened
    HB_STATUS_COUNT,
};

struct HealthboxStatusLineInfo
{
    u8 color;
    u8 maxStage; // pips drawn in the cell
    u8 glyph[7]; // 7x7 icon, one row per byte, bit 6 = leftmost pixel
};

static const struct HealthboxStatusLineInfo sHealthboxStatusLines[HB_STATUS_COUNT] =
{
    [HB_STATUS_SOAK]      = {HB_COLOR_SKY,    1, {0x08, 0x1C, 0x1C, 0x36, 0x2E, 0x3E, 0x1C}}, // drop
    [HB_STATUS_CHILL]     = {HB_COLOR_CYAN,   2, {0x08, 0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x08}}, // snowflake
    [HB_STATUS_BURN]      = {HB_COLOR_ORANGE, 3, {0x08, 0x18, 0x1C, 0x3E, 0x36, 0x22, 0x1C}}, // flame
    [HB_STATUS_POISON]    = {HB_COLOR_PURPLE, 2, {0x3E, 0x7F, 0x49, 0x7F, 0x36, 0x1C, 0x14}}, // skull
    [HB_STATUS_ELECTRIC]  = {HB_COLOR_YELLOW, 3, {0x06, 0x0C, 0x1E, 0x3C, 0x0C, 0x18, 0x30}}, // bolt
    [HB_STATUS_SLEEP]     = {HB_COLOR_BLUE,   3, {0x7F, 0x06, 0x0C, 0x18, 0x30, 0x7F, 0x00}}, // Z
    [HB_STATUS_CONFUSION] = {HB_COLOR_PINK,   3, {0x3E, 0x41, 0x5D, 0x55, 0x51, 0x5E, 0x40}}, // swirl
};

static const struct WindowTemplate sHealthboxWindowTemplate = {
    .bg = 0,
    .tilemapLeft = 0,
    .tilemapTop = 0,
    .width = 8,
    .height = 2,
    .paletteNum = 0,
    .baseBlock = 0
};

static const union TextColor sHealthBoxTextColor =
{
    .background = 0,
    .foreground = HB_COLOR_WHITE, // Fantasy Retold: light text on the dark plate
    .shadow = HB_COLOR_INK,
    .accent = 0
};

// Because the healthbox is too large to fit into one sprite, it is divided into two sprites.
// healthboxLeft  or healthboxMain  is the left part that is used as the 'main' sprite.
// healthboxRight or healthboxOther is the right part of the healthbox.
// There's also the third sprite under name of healthbarSprite that refers to the healthbar visible on the healtbox.

// data fields for healthboxMain
// oam.affineParam holds healthboxRight spriteId
#define hMain_HealthBarSpriteId     data[5]
#define hMain_Battler               data[6]
#define hMain_Data7                 data[7]

// data fields for healthboxRight
#define hOther_HealthBoxSpriteId    data[5]

// data fields for healthbar (the status column)
#define hBar_HealthBoxSpriteId      data[5]
#define hBar_StatusSignature        data[7] // stages last drawn, see GetHealthboxStatusSignature

// This function is here to cover a specific case - one player's mon in a 2 vs 1 double battle. In this scenario - display singles layout.
// The same goes for a 2 vs 1 where opponent has only one Pokémon.
enum BattleCoordTypes GetBattlerCoordsIndex(enum BattlerId battler)
{
    if (GetBattlerPosition(battler) == B_POSITION_PLAYER_LEFT && gPartiesCount[B_TRAINER_PLAYER] == 1 && !(gBattleTypeFlags & BATTLE_TYPE_MULTI))
        return BATTLE_COORDS_SINGLES;
    else if (GetBattlerPosition(battler) == B_POSITION_OPPONENT_LEFT && gPartiesCount[B_TRAINER_OPPONENT_A] == 1 && !(gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS))
        return BATTLE_COORDS_SINGLES;
    else if (IsDoubleBattle())
        return BATTLE_COORDS_DOUBLES;
    else
        return BATTLE_COORDS_SINGLES;
}

u8 CreateBattlerHealthboxSprites(enum BattlerId battler)
{
    u8 healthboxLeftSpriteId, healthboxRightSpriteId;
    u8 healthbarSpriteId;
    struct Sprite *healthBarSpritePtr;

    switch (GetBattlerCoordsIndex(battler))
    {
    default:
    case BATTLE_COORDS_SINGLES:
    {
        if (IsOnPlayerSide(battler))
        {
            healthboxLeftSpriteId = CreateSprite(&sHealthboxPlayerSpriteTemplates[0], DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);
            healthboxRightSpriteId = CreateSpriteAtEnd(&sHealthboxPlayerSpriteTemplates[0], DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);

            gSprites[healthboxLeftSpriteId].oam.shape = ST_OAM_SQUARE;
            gSprites[healthboxRightSpriteId].oam.shape = ST_OAM_SQUARE;
            gSprites[healthboxRightSpriteId].oam.tileNum += 64;
        }
        else
        {
            healthboxLeftSpriteId = CreateSprite(&sHealthboxOpponentSpriteTemplates[0], DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);
            healthboxRightSpriteId = CreateSpriteAtEnd(&sHealthboxOpponentSpriteTemplates[0], DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);

            gSprites[healthboxRightSpriteId].oam.tileNum += 32;
        }
        gSprites[healthboxLeftSpriteId].oam.affineParam = healthboxRightSpriteId;

        gSprites[healthboxRightSpriteId].hOther_HealthBoxSpriteId = healthboxLeftSpriteId;
        gSprites[healthboxRightSpriteId].callback = SpriteCB_HealthBoxOther;
        break;
    }
    case BATTLE_COORDS_DOUBLES:
    {
        if (IsOnPlayerSide(battler))
        {
            healthboxLeftSpriteId = CreateSprite(&sHealthboxPlayerSpriteTemplates[GetBattlerPosition(battler) / 2], DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);
            healthboxRightSpriteId = CreateSpriteAtEnd(&sHealthboxPlayerSpriteTemplates[GetBattlerPosition(battler) / 2], DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);
        }
        else
        {
            healthboxLeftSpriteId = CreateSprite(&sHealthboxOpponentSpriteTemplates[GetBattlerPosition(battler) / 2], DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);
            healthboxRightSpriteId = CreateSpriteAtEnd(&sHealthboxOpponentSpriteTemplates[GetBattlerPosition(battler) / 2], DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);
        }
        gSprites[healthboxLeftSpriteId].oam.affineParam = healthboxRightSpriteId;

        gSprites[healthboxRightSpriteId].hOther_HealthBoxSpriteId = healthboxLeftSpriteId;
        gSprites[healthboxRightSpriteId].oam.tileNum += 32;
        gSprites[healthboxRightSpriteId].callback = SpriteCB_HealthBoxOther;
        break;
    }
    }

    healthbarSpriteId = CreateSpriteAtEnd(&sHealthbarSpriteTemplates[gBattlerPositions[battler]], 140, 60, 0);
    healthBarSpritePtr = &gSprites[healthbarSpriteId];
    healthBarSpritePtr->oam.priority = 1;
    CpuFill32(0, (void *)(OBJ_VRAM0 + healthBarSpritePtr->oam.tileNum * TILE_SIZE_4BPP), 32 * TILE_SIZE_4BPP);

    gSprites[healthboxLeftSpriteId].hMain_HealthBarSpriteId = healthbarSpriteId;
    gSprites[healthboxLeftSpriteId].hMain_Battler = battler;
    gSprites[healthboxLeftSpriteId].invisible = TRUE;

    gSprites[healthboxRightSpriteId].invisible = TRUE;

    healthBarSpritePtr->hBar_HealthBoxSpriteId = healthboxLeftSpriteId;
    healthBarSpritePtr->hBar_StatusSignature = 0; // empty column, matching the cleared tiles
    healthBarSpritePtr->invisible = TRUE;

    CreateIndicatorSprite(battler);

    gBattleStruct->ballSpriteIds[0] = MAX_SPRITES;
    gBattleStruct->ballSpriteIds[1] = MAX_SPRITES;
    gBattleStruct->moveInfoSpriteId = MAX_SPRITES;

    return healthboxLeftSpriteId;
}

u8 CreateSafariPlayerHealthboxSprites(void)
{
    u8 healthboxLeftSpriteId, healthboxRightSpriteId;

    healthboxLeftSpriteId = CreateSprite(&sHealthboxSafariSpriteTemplate, DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);
    healthboxRightSpriteId = CreateSpriteAtEnd(&sHealthboxSafariSpriteTemplate, DISPLAY_WIDTH, DISPLAY_HEIGHT, 1);

    gSprites[healthboxLeftSpriteId].oam.shape = ST_OAM_SQUARE;
    gSprites[healthboxRightSpriteId].oam.shape = ST_OAM_SQUARE;

    gSprites[healthboxRightSpriteId].oam.tileNum += 64;

    gSprites[healthboxLeftSpriteId].oam.affineParam = healthboxRightSpriteId;
    gSprites[healthboxRightSpriteId].hOther_HealthBoxSpriteId = healthboxLeftSpriteId;

    gSprites[healthboxRightSpriteId].callback = SpriteCB_HealthBoxOther;

    return healthboxLeftSpriteId;
}

static const u8 *GetHealthboxElementGfxPtr(u8 elementId)
{
    return gHealthboxElementsGfxTable[elementId];
}

static const struct HealthboxLayout *GetHealthboxLayout(enum BattlerId battler)
{
    if (!IsOnPlayerSide(battler))
        return &sHealthboxLayouts[HB_LAYOUT_OPPONENT];
    if (GetBattlerCoordsIndex(battler) == BATTLE_COORDS_SINGLES)
        return &sHealthboxLayouts[HB_LAYOUT_PLAYER_SINGLES];
    return &sHealthboxLayouts[HB_LAYOUT_PLAYER];
}

// The plate is drawn from the top-left of the healthbox canvas. The player singles box is switched to
// 64x64 after creation but keeps the 64x32 template's centre offset, so this holds for every box.
static s32 GetHealthboxPlateTop(struct Sprite *healthbox)
{
    return healthbox->y + healthbox->centerToCornerVecY;
}

// Fills `stages` with the current stage (0 = none) of every status line and returns them packed 2 bits
// per line, so the status column can tell when it needs a redraw. Every line reads its status rail
// (combat.md §6). This runs every frame, so it only reads: a status that vanilla code wrote straight to
// status1 / confusionTimer and that no rail has picked up yet shows at the stage that bit stands for.
static u32 GetHealthboxStatusStages(enum BattlerId battler, u8 stages[HB_STATUS_COUNT])
{
    u32 status1 = gBattleMons[battler].status1;
    u32 i, signature = 0;

    stages[HB_STATUS_SOAK] = GetStatusRailLevel(battler, RAIL_SOAK);
    stages[HB_STATUS_CHILL] = GetStatusRailLevel(battler, RAIL_CHILL);
    stages[HB_STATUS_BURN] = GetStatusRailLevel(battler, RAIL_BURN);
    stages[HB_STATUS_POISON] = GetStatusRailLevel(battler, RAIL_POISON);
    stages[HB_STATUS_ELECTRIC] = GetStatusRailLevel(battler, RAIL_ELECTRIC);
    stages[HB_STATUS_SLEEP] = GetStatusRailLevel(battler, RAIL_SLEEP);
    stages[HB_STATUS_CONFUSION] = GetStatusRailLevel(battler, RAIL_CONFUSION);

    if (stages[HB_STATUS_CHILL] < 2 && (status1 & STATUS1_ICY_ANY))
        stages[HB_STATUS_CHILL] = 2;
    if (stages[HB_STATUS_BURN] == 0 && (status1 & STATUS1_BURN))
        stages[HB_STATUS_BURN] = 1;
    if (stages[HB_STATUS_POISON] == 0 && (status1 & STATUS1_PSN_ANY))
        stages[HB_STATUS_POISON] = (status1 & STATUS1_TOXIC_POISON) ? 2 : 1;
    if (stages[HB_STATUS_ELECTRIC] < 3 && (status1 & STATUS1_PARALYSIS))
        stages[HB_STATUS_ELECTRIC] = 3;
    if (stages[HB_STATUS_SLEEP] < 2 && (status1 & STATUS1_SLEEP))
        stages[HB_STATUS_SLEEP] = 2;
    if (stages[HB_STATUS_CONFUSION] == 0 && gBattleMons[battler].volatiles.confusionTimer > 0)
        stages[HB_STATUS_CONFUSION] = 1;

    for (i = 0; i < HB_STATUS_COUNT; i++)
        signature |= min(stages[i], 3) << (2 * i);
    return signature;
}

// Inclusive rectangle in a 64x32 sprite's tiles (8 tiles wide, 1D mapping).
static void FillStatusColumnRect(u32 *tiles, u32 x0, u32 y0, u32 x1, u32 y1, u32 color)
{
    u32 x, y;

    for (y = y0; y <= y1; y++)
    {
        for (x = x0; x <= x1; x++)
        {
            u32 *word = &tiles[(((y / 8) * 8) + (x / 8)) * 8 + (y % 8)];
            u32 shift = (x % 8) * 4;
            *word = (*word & ~(0xF << shift)) | (color << shift);
        }
    }
}

// A box as tall as the plate holding one cell per active status line: a 7x7 icon plus one pip per stage
// the line can reach, lit bottom-up. Cells fill top-down, two per column; extra columns grow away from
// the plate (left for the player, right for the opponent). No status = no box.
static void DrawHealthboxStatusColumn(struct Sprite *sprite, enum BattlerId battler)
{
    const struct HealthboxLayout *layout = GetHealthboxLayout(battler);
    u32 *tiles = (u32 *)(OBJ_VRAM0 + sprite->oam.tileNum * TILE_SIZE_4BPP);
    u8 stages[HB_STATUS_COUNT];
    u32 i, r, count = 0;
    u32 h = layout->plateH;
    u32 rows = 2, cols, w, x0, top;

    sprite->hBar_StatusSignature = GetHealthboxStatusStages(battler, stages);
    CpuFill32(0, tiles, 32 * TILE_SIZE_4BPP);

    for (i = 0; i < HB_STATUS_COUNT; i++)
    {
        if (stages[i] != 0)
            count++;
    }
    if (count == 0)
        return;

    cols = (count + rows - 1) / rows;
    w = cols * (HB_STATUS_CELL_W + 1) + 3;
    x0 = IsOnPlayerSide(battler) ? 64 - w : 0;
    top = (h - rows * (HB_STATUS_CELL_H + 1) + 1) / 2;

    // Same corner-cut silhouette as the plate.
    for (r = 0; r < h; r++)
    {
        u32 cut = max(0, max(2 - (s32)r, (s32)r - (s32)(h - 3)));
        FillStatusColumnRect(tiles, x0 + cut, r, x0 + w - 1 - cut, r, HB_COLOR_INK);
        if (r != 0 && r != h - 1)
            FillStatusColumnRect(tiles, x0 + max(1, cut), r, x0 + w - 2 - (cut > 0 ? cut - 1 : 0), r, HB_COLOR_PLATE);
    }
    FillStatusColumnRect(tiles, x0 + 2, 1, x0 + w - 3, 1, HB_COLOR_SLATE);

    for (i = 0, count = 0; i < HB_STATUS_COUNT; i++)
    {
        const struct HealthboxStatusLineInfo *line = &sHealthboxStatusLines[i];
        u32 cx, cy, gx, gy, pip;

        if (stages[i] == 0)
            continue;

        cx = x0 + 2 + (count / rows) * (HB_STATUS_CELL_W + 1);
        cy = top + (count % rows) * (HB_STATUS_CELL_H + 1);
        count++;

        FillStatusColumnRect(tiles, cx, cy, cx + HB_STATUS_CELL_W - 1, cy + HB_STATUS_CELL_H - 1, HB_COLOR_EMPTY);
        for (gy = 0; gy < 7; gy++)
        {
            for (gx = 0; gx < 7; gx++)
            {
                if (line->glyph[gy] & (0x40 >> gx))
                    FillStatusColumnRect(tiles, cx + 1 + gx, cy + 1 + gy, cx + 1 + gx, cy + 1 + gy, line->color);
            }
        }
        for (pip = 0; pip < line->maxStage; pip++)
        {
            u32 py = cy + HB_STATUS_CELL_H - 3 - pip * 3;
            FillStatusColumnRect(tiles, cx + 9, py, cx + 10, py + 1, pip < stages[i] ? line->color : HB_COLOR_SLATE);
        }
    }
}

// Fantasy Retold: the Stamina value the bar currently shows. It slides toward the live CurrentSta at the HP
// bar's speed (SpriteCB_HealthBar steps it every frame) and snaps on a full healthbox redraw (send-out).
static u16 sStaminaShown[MAX_BATTLERS_COUNT];

static u32 GetLiveStaminaForBar(enum BattlerId battler)
{
    return min(gBattleStruct->battlerState[battler].currentStamina, GetBaseStamina(battler));
}

// Fantasy Retold: moves the shown Stamina one frame's step toward the live value, at the HP bar's pace
// (MoveBattleBar: a full bar in B_HEALTHBAR_PIXELS / 2 = 24 frames with B_FAST_HP_DRAIN, else 1 point a frame).
#define STAMINA_BAR_FULL_DRAIN_FRAMES 24

static void StepStaminaGauge(u32 healthboxSpriteId, enum BattlerId battler)
{
    u32 target = GetLiveStaminaForBar(battler);
    u32 shown = sStaminaShown[battler];
    u32 step;

    if (shown == target)
        return;

    step = B_FAST_HP_DRAIN == FALSE ? 1 : max(GetBaseStamina(battler) / STAMINA_BAR_FULL_DRAIN_FRAMES, 1);
    if (shown < target)
        shown = min(shown + step, target);
    else
        shown = (shown > target + step) ? shown - step : target;
    sStaminaShown[battler] = shown;
    DrawStaminaGaugeOnHealthbox(healthboxSpriteId, battler);
}

// Fantasy Retold: the Trauma value the HP bar's purple scar currently shows, slid toward the live Trauma at the
// HP bar's pace like sStaminaShown, and snapped on a full healthbox redraw. The step redraws the HP bar with the
// last HP fill it drew (sHpBarPixels / sHpBarMax), so it works whether or not the HP bar is moving too.
static u16 sTraumaShown[MAX_BATTLERS_COUNT];
static u16 sHpBarMax[MAX_BATTLERS_COUNT];
static u8 sHpBarPixels[MAX_BATTLERS_COUNT];

static u32 GetLiveTraumaForBar(enum BattlerId battler)
{
    return GetBattlerPartyState(battler)->trauma;
}

static void StepTraumaGauge(u32 healthboxSpriteId, enum BattlerId battler)
{
    u32 target = GetLiveTraumaForBar(battler);
    u32 shown = sTraumaShown[battler];
    u32 step;

    if (shown == target)
        return;
    if (sHpBarMax[battler] == 0) // no HP bar drawn yet
    {
        sTraumaShown[battler] = target;
        return;
    }

    step = B_FAST_HP_DRAIN == FALSE ? 1 : max(sHpBarMax[battler] / STAMINA_BAR_FULL_DRAIN_FRAMES, 1);
    if (shown < target)
        shown = min(shown + step, target);
    else
        shown = (shown > target + step) ? shown - step : target;
    sTraumaShown[battler] = shown;
    DrawHealthboxHpBar(healthboxSpriteId, battler, sHpBarMax[battler], sHpBarPixels[battler]);
}

// The healthbar sprite is the status column: keeps it beside the plate and redraws it whenever any
// status stage changes (polled, since confusion and rails change without a healthbox update).
static void SpriteCB_HealthBar(struct Sprite *sprite)
{
    struct Sprite *healthbox = &gSprites[sprite->hBar_HealthBoxSpriteId];
    enum BattlerId battler = healthbox->hMain_Battler;
    s32 plateLeft = healthbox->x + healthbox->centerToCornerVecX;
    u8 stages[HB_STATUS_COUNT];

    if (IsOnPlayerSide(battler))
        sprite->x = plateLeft - HB_STATUS_COLUMN_GAP - 64 - sprite->centerToCornerVecX;
    else
        sprite->x = plateLeft + GetHealthboxLayout(battler)->plateW + HB_STATUS_COLUMN_GAP - sprite->centerToCornerVecX;
    sprite->y = GetHealthboxPlateTop(healthbox) - sprite->centerToCornerVecY;

    sprite->x2 = healthbox->x2;
    sprite->y2 = healthbox->y2;

    if (gBattleStruct != NULL && GetHealthboxStatusStages(battler, stages) != (u16)sprite->hBar_StatusSignature)
        DrawHealthboxStatusColumn(sprite, battler);

    if (gBattleStruct != NULL)
    {
        StepStaminaGauge(sprite->hBar_HealthBoxSpriteId, battler);
        StepTraumaGauge(sprite->hBar_HealthBoxSpriteId, battler);
    }
}

static void SpriteCB_HealthBoxOther(struct Sprite *sprite)
{
    u8 healthboxMainSpriteId = sprite->hOther_HealthBoxSpriteId;

    sprite->x = gSprites[healthboxMainSpriteId].x + 64;
    sprite->y = gSprites[healthboxMainSpriteId].y;

    sprite->x2 = gSprites[healthboxMainSpriteId].x2;
    sprite->y2 = gSprites[healthboxMainSpriteId].y2;
}

void SetBattleBarStruct(enum BattlerId battler, u8 healthboxSpriteId, s32 maxVal, s32 oldVal, s32 receivedValue)
{
    gBattleSpritesDataPtr->battleBars[battler].healthboxSpriteId = healthboxSpriteId;
    gBattleSpritesDataPtr->battleBars[battler].maxValue = maxVal;
    gBattleSpritesDataPtr->battleBars[battler].oldValue = oldVal;
    gBattleSpritesDataPtr->battleBars[battler].receivedValue = receivedValue;
    gBattleSpritesDataPtr->battleBars[battler].currValue = -32768;
}

void SetHealthboxSpriteInvisible(u8 healthboxSpriteId)
{
    gSprites[healthboxSpriteId].invisible = TRUE;
    gSprites[gSprites[healthboxSpriteId].hMain_HealthBarSpriteId].invisible = TRUE;
    gSprites[gSprites[healthboxSpriteId].oam.affineParam].invisible = TRUE;
    UpdateIndicatorVisibilityAndType(healthboxSpriteId, TRUE);
}

void SetHealthboxSpriteVisible(u8 healthboxSpriteId)
{
    gSprites[healthboxSpriteId].invisible = FALSE;
    gSprites[gSprites[healthboxSpriteId].hMain_HealthBarSpriteId].invisible = FALSE;
    gSprites[gSprites[healthboxSpriteId].oam.affineParam].invisible = FALSE;
    UpdateIndicatorVisibilityAndType(healthboxSpriteId, FALSE);
}

static void UpdateSpritePos(u8 spriteId, s16 x, s16 y)
{
    gSprites[spriteId].x = x;
    gSprites[spriteId].y = y;
}

void DummyBattleInterfaceFunc(u8 healthboxSpriteId, bool8 isDoubleBattleBattlerOnly)
{

}

static void TryToggleHealboxVisibility(u32 priority, u32 healthboxLeftSpriteId, u32 healthboxRightSpriteId, u32 healthbarSpriteId)
{
    bool32 invisible = FALSE;

    if (priority == 0)  // start of anim -> make invisible
        invisible = TRUE;
    else if (priority == 1) // end of anim -> make visible
        invisible = FALSE;

    gSprites[healthboxLeftSpriteId].invisible = invisible;
    gSprites[healthboxRightSpriteId].invisible = invisible;
    gSprites[healthbarSpriteId].invisible = invisible;

    UpdateIndicatorVisibilityAndType(healthboxLeftSpriteId, invisible);
}

void UpdateOamPriorityInAllHealthboxes(u8 priority, bool32 hideHPBoxes)
{
    for (enum BattlerId i = 0; i < gBattlersCount; i++)
    {
        u8 healthboxLeftSpriteId = gHealthboxSpriteIds[i];
        u8 healthboxRightSpriteId = gSprites[gHealthboxSpriteIds[i]].oam.affineParam;
        u8 healthbarSpriteId = gSprites[gHealthboxSpriteIds[i]].hMain_HealthBarSpriteId;

        gSprites[healthboxLeftSpriteId].oam.priority = priority;
        gSprites[healthboxRightSpriteId].oam.priority = priority;
        gSprites[healthbarSpriteId].oam.priority = priority;

        UpdateIndicatorOamPriority(healthboxLeftSpriteId, priority);

        if (B_HIDE_HEALTHBOX_IN_ANIMS == TRUE && hideHPBoxes && IsBattlerAlive(i))
            TryToggleHealboxVisibility(priority, healthboxLeftSpriteId, healthboxRightSpriteId, healthbarSpriteId);
    }
}

static const s16 sBattlerHealthboxCoords[BATTLE_COORDS_COUNT][MAX_BATTLERS_COUNT][2] =
{
    // Fantasy Retold: every box's plate starts at (x - 32, y - 16). Player plates end near the right
    // edge with their status column growing left; opponent columns grow right, toward the centre.
    [BATTLE_COORDS_SINGLES] =
    {
        [B_POSITION_PLAYER_LEFT]   = { 164, 88 },
        [B_POSITION_OPPONENT_LEFT] = { 36,  28 },
    },
    [BATTLE_COORDS_DOUBLES] =
    {
        [B_POSITION_PLAYER_LEFT]    = { 156, 61 },
        [B_POSITION_PLAYER_RIGHT]   = { 164, 95 },
        [B_POSITION_OPPONENT_LEFT]  = { 44,  20 },
        [B_POSITION_OPPONENT_RIGHT] = { 36,  46 },
    },
};

void GetBattlerHealthboxCoords(enum BattlerId battler, s16 *x, s16 *y)
{
    enum BattlerPosition position = GetBattlerPosition(battler);
    enum BattleCoordTypes index = GetBattlerCoordsIndex(battler);

    *x = sBattlerHealthboxCoords[index][position][0];
    *y = sBattlerHealthboxCoords[index][position][1];
}

void InitBattlerHealthboxCoords(enum BattlerId battler)
{
    s16 x, y;

    GetBattlerHealthboxCoords(battler, &x, &y);
    UpdateSpritePos(gHealthboxSpriteIds[battler], x, y);
}

// Fantasy Retold: the two healthbox sprites form one 128px-wide canvas. These link the halves for the
// sprite fill / text helpers and pick the right half when x starts there (the helpers only follow the
// link rightwards, from the sprite they start in).
static void FillHealthboxRect(u32 healthboxSpriteId, u32 x, u32 y, u32 width, u32 height, u32 color)
{
    u32 rightSpriteId = gSprites[healthboxSpriteId].oam.affineParam;
    s16 savedLeft = gSprites[healthboxSpriteId].data[1];
    s16 savedRight = gSprites[rightSpriteId].data[1];

    if (width == 0 || height == 0)
        return;

    gSprites[healthboxSpriteId].data[1] = rightSpriteId;
    gSprites[rightSpriteId].data[1] = SPRITE_NONE;
    if (x >= 64)
        FillSpriteRectColor(rightSpriteId, x - 64, y, width, height, color);
    else
        FillSpriteRectColor(healthboxSpriteId, x, y, width, height, color);
    gSprites[healthboxSpriteId].data[1] = savedLeft;
    gSprites[rightSpriteId].data[1] = savedRight;
}

static void PrintOnHealthbox(u32 healthboxSpriteId, u32 fontId, u32 x, u32 y, const u8 *str)
{
    u32 rightSpriteId = gSprites[healthboxSpriteId].oam.affineParam;
    s16 savedLeft = gSprites[healthboxSpriteId].data[1];
    s16 savedRight = gSprites[rightSpriteId].data[1];

    gSprites[healthboxSpriteId].data[1] = rightSpriteId;
    gSprites[rightSpriteId].data[1] = SPRITE_NONE;
    if (x >= 64)
        AddSpriteTextPrinterParameterized6(rightSpriteId, fontId, x - 64, y, 0, 0, sHealthBoxTextColor, 0, str);
    else
        AddSpriteTextPrinterParameterized6(healthboxSpriteId, fontId, x, y, 0, 0, sHealthBoxTextColor, 0, str);
    gSprites[healthboxSpriteId].data[1] = savedLeft;
    gSprites[rightSpriteId].data[1] = savedRight;
}

// Right edge (exclusive) of right-aligned text on the plate.
static u32 GetHealthboxTextRight(const struct HealthboxLayout *layout)
{
    return layout->plateW - 5;
}

#define HB_LEVEL_TEXT_W 26 // widest "Lv100" plus shadow

static void UpdateLvlInHealthbox(u8 healthboxSpriteId, u8 lvl)
{
    u8 text[16];
    enum BattlerId battler = gSprites[healthboxSpriteId].hMain_Battler;
    u32 right = GetHealthboxTextRight(GetHealthboxLayout(battler));

    // Don't print Lv char if mon has a gimmick with an indicator active.
    if (GetIndicatorPalTag(battler) != TAG_NONE)
    {
        ConvertIntToDecimalStringN(text, lvl, STR_CONV_MODE_LEFT_ALIGN, 3);
        UpdateIndicatorLevelData(healthboxSpriteId, lvl);
        UpdateIndicatorVisibilityAndType(healthboxSpriteId, FALSE);
    }
    else
    {
        text[0] = CHAR_EXTRA_SYMBOL;
        text[1] = CHAR_LV_2;

        ConvertIntToDecimalStringN(text + 2, lvl, STR_CONV_MODE_LEFT_ALIGN, 3);
        UpdateIndicatorVisibilityAndType(healthboxSpriteId, TRUE);
    }

    FillHealthboxRect(healthboxSpriteId, right - HB_LEVEL_TEXT_W, 2, HB_LEVEL_TEXT_W + 1, 10, HEALTHBOX_BG_INDEX);
    PrintOnHealthbox(healthboxSpriteId, FONT_SMALL, right - GetStringWidth(FONT_SMALL, text, 0), HB_TEXT_Y, text);
}

#define HP_FONT FONT_SMALL
#define HP_MAX_DIGITS 4

// Player side only: `current / effective max` right-aligned on the bottom row, beside the EXP track.
static void PrintHpOnHealthbox(u32 healthboxSpriteId, s16 currHp, s16 maxHp)
{
    u8 text[2 * HP_MAX_DIGITS + 2], *txtPtr;
    enum BattlerId battler = gSprites[healthboxSpriteId].hMain_Battler;
    u32 trauma = GetBattlerPartyState(battler)->trauma;
    u32 right = GetHealthboxTextRight(GetHealthboxLayout(battler));
    u32 clearX = HB_EXP_X + HB_EXP_W;

    // Fantasy Retold: Trauma is unhealable, so the max shown is the effective max (maxHP - Trauma).
    // The HP bar still spans the original max and shows the lost part as the purple scar.
    maxHp = ((s32)trauma >= maxHp) ? 0 : maxHp - trauma;

    txtPtr = ConvertIntToDecimalStringN(text, currHp, STR_CONV_MODE_LEFT_ALIGN, HP_MAX_DIGITS);
    *txtPtr++ = CHAR_SLASH;
    ConvertIntToDecimalStringN(txtPtr, maxHp, STR_CONV_MODE_LEFT_ALIGN, HP_MAX_DIGITS);

    FillHealthboxRect(healthboxSpriteId, clearX, HB_HP_TEXT_Y + 3, right + 1 - clearX, 8, HEALTHBOX_BG_INDEX);
    PrintOnHealthbox(healthboxSpriteId, HP_FONT, right - GetStringWidth(HP_FONT, text, 0), HB_HP_TEXT_Y, text);
}

// Fantasy Retold: the HP bar spans the ORIGINAL max HP and reads, left to right: purple (Trauma,
// permanently lost) -> red (current HP) -> slate (recoverable loss). Current HP is capped at
// maxHP - trauma, so the red fill always starts where the scar ends. The scar draws the animated Trauma
// (sTraumaShown), not the live one.
static void DrawHealthboxHpBar(u32 healthboxSpriteId, enum BattlerId battler, s32 barMaxHP, u32 hpPixels)
{
    const struct HealthboxLayout *layout = GetHealthboxLayout(battler);
    u32 trauma = sTraumaShown[battler];
    u32 traumaPixels = 0, redEnd;

    sHpBarMax[battler] = max(barMaxHP, 0);
    sHpBarPixels[battler] = hpPixels;

    if (trauma != 0 && barMaxHP > 0)
    {
        traumaPixels = (trauma * layout->barW) / barMaxHP;
        if (traumaPixels == 0)
            traumaPixels = 1; // a single hit of Trauma always registers
        if (traumaPixels > layout->barW)
            traumaPixels = layout->barW;
    }
    redEnd = min(traumaPixels + hpPixels, layout->barW);

    // The frame matches the fill (red), as the Stamina bar's does, so both bars read at their true size.
    FillHealthboxRect(healthboxSpriteId, layout->barX - 1, layout->hpY - 1, layout->barW + 2, 1, HB_COLOR_RED);
    FillHealthboxRect(healthboxSpriteId, layout->barX - 1, layout->hpY + 4, layout->barW + 2, 1, HB_COLOR_RED);
    FillHealthboxRect(healthboxSpriteId, layout->barX - 1, layout->hpY, 1, 4, HB_COLOR_RED);
    FillHealthboxRect(healthboxSpriteId, layout->barX + layout->barW, layout->hpY, 1, 4, HB_COLOR_RED);

    FillHealthboxRect(healthboxSpriteId, layout->barX, layout->hpY, traumaPixels, 4, HB_COLOR_PURPLE);
    FillHealthboxRect(healthboxSpriteId, layout->barX + traumaPixels, layout->hpY, redEnd - traumaPixels, 4, HB_COLOR_RED);
    FillHealthboxRect(healthboxSpriteId, layout->barX + redEnd, layout->hpY, layout->barW - redEnd, 4, HB_COLOR_SLATE);
}

// Fantasy Retold: the Stamina bar under the HP bar, same length. Green normally, yellow once Recuperate is
// available (a white tick through the frame marks that threshold), empty with a red frame at 0
// Stamina. Any non-zero Stamina shows at least 1px, since exhaustion keys off the exact live value
// (CurrentSta == 0), not the drawn bar. Draws the animated value (sStaminaShown), not the live one.
static void DrawStaminaGaugeOnHealthbox(u32 healthboxSpriteId, enum BattlerId battler)
{
    const struct HealthboxLayout *layout = GetHealthboxLayout(battler);
    u32 x = layout->barX, y = layout->staY, w = layout->barW;
    u32 curSta = sStaminaShown[battler];
    u32 baseSta = GetBaseStamina(battler);
    u32 filled, frameColor, fillColor;

    if (gBattleTypeFlags & BATTLE_TYPE_SAFARI)
        return;

    if (curSta > baseSta)
        curSta = baseSta;
    filled = (baseSta != 0) ? (curSta * w) / baseSta : 0;
    if (curSta != 0 && filled == 0)
        filled = 1;

    fillColor = (curSta * 100 <= baseSta * RECUPERATE_STAMINA_PERCENT) ? HB_COLOR_YELLOW : HB_COLOR_GREEN;
    frameColor = (curSta == 0) ? HB_COLOR_RED : fillColor; // the frame matches the fill

    FillHealthboxRect(healthboxSpriteId, x - 1, y - 1, w + 2, 1, frameColor);
    FillHealthboxRect(healthboxSpriteId, x - 1, y + 2, w + 2, 1, frameColor);
    FillHealthboxRect(healthboxSpriteId, x - 1, y, 1, 2, frameColor);
    FillHealthboxRect(healthboxSpriteId, x + w, y, 1, 2, frameColor);
    // Recuperate threshold: a white tick through the frame.
    FillHealthboxRect(healthboxSpriteId, x + (RECUPERATE_STAMINA_PERCENT * w) / 100, y - 1, 1, 1, HB_COLOR_WHITE);
    FillHealthboxRect(healthboxSpriteId, x + (RECUPERATE_STAMINA_PERCENT * w) / 100, y + 2, 1, 1, HB_COLOR_WHITE);

    FillHealthboxRect(healthboxSpriteId, x, y, filled, 2, fillColor);
    FillHealthboxRect(healthboxSpriteId, x + filled, y, w - filled, 2, HB_COLOR_EMPTY);
}

static void DrawHealthboxExpBar(u32 healthboxSpriteId, u32 pixels)
{
    FillHealthboxRect(healthboxSpriteId, HB_EXP_X, HB_EXP_Y, pixels, 1, HB_COLOR_CYAN);
    FillHealthboxRect(healthboxSpriteId, HB_EXP_X + pixels, HB_EXP_Y, HB_EXP_W - pixels, 1, HB_COLOR_EMPTY);
}

// Note: this is only possible to trigger via debug, it was an unused GF function.
UNUSED static void UpdateOpponentHpTextDoubles(u32 healthboxSpriteId, u32 barSpriteId, s16 value, u8 maxOrCurrent)
{
    u8 text[32], *txtPtr;
    u32 i, var;
    enum BattlerId battler = gSprites[healthboxSpriteId].hMain_Battler;

    if (gBattleSpritesDataPtr->battlerData[battler].hpNumbersNoBars) // don't print text if only bars are visible
    {
        memcpy(text, sEmptyWhiteText_TransparentHighlight, sizeof(sEmptyWhiteText_TransparentHighlight));
        if (maxOrCurrent == HP_CURRENT)
            var = 0;
        else
            var = 4;

        txtPtr = ConvertIntToDecimalStringN(text + 9, value, STR_CONV_MODE_RIGHT_ALIGN, 3);
        if (!maxOrCurrent)
            StringCopy(txtPtr, gText_Slash);
        RenderTextHandleBold(gMonSpritesGfxPtr->barFontGfx, FONT_BOLD, text);

        for (i = var; i < var + 3; i++)
        {
            if (i < 3)
            {
                CpuCopy32(&gMonSpritesGfxPtr->barFontGfx[((i - var) * 64) + 32],
                      (void *)((OBJ_VRAM0) + 32 * (1 + gSprites[barSpriteId].oam.tileNum + i)),
                      0x20);
            }
            else
            {
                CpuCopy32(&gMonSpritesGfxPtr->barFontGfx[((i - var) * 64) + 32],
                      (void *)((OBJ_VRAM0 + 0x20) + 32 * (i + gSprites[barSpriteId].oam.tileNum)),
                      0x20);
            }
        }

        if (maxOrCurrent == HP_CURRENT)
        {
            CpuCopy32(&gMonSpritesGfxPtr->barFontGfx[224],
                      (void *)((OBJ_VRAM0) + ((gSprites[barSpriteId].oam.tileNum + 4) * TILE_SIZE_4BPP)),
                      0x20);
            CpuFill32(0, (void *)((OBJ_VRAM0) + (gSprites[barSpriteId].oam.tileNum * TILE_SIZE_4BPP)), 0x20);
        }
    }
}

// Same with this one.
UNUSED static void UpdateOpponentHpTextSingles(u32 healthboxSpriteId, s16 value, u32 maxOrCurrent)
{
    u8 text[32];
    u32 var, i;
    enum BattlerId battler = gSprites[healthboxSpriteId].hMain_Battler;

    memcpy(text, sEmptyWhiteText_GrayHighlight, sizeof(sEmptyWhiteText_GrayHighlight));
    if (gBattleSpritesDataPtr->battlerData[battler].hpNumbersNoBars) // don't print text if only bars are visible
    {
        if (maxOrCurrent == HP_CURRENT)
            var = 21;
        else
            var = 49;

        ConvertIntToDecimalStringN(text + 6, value, STR_CONV_MODE_LEADING_ZEROS, 3);
        RenderTextHandleBold(gMonSpritesGfxPtr->barFontGfx, FONT_BOLD, text);

        for (i = 0; i < 3; i++)
        {
            CpuCopy32(&gMonSpritesGfxPtr->barFontGfx[i * 64 + 32],
                      (void *)((OBJ_VRAM0) + TILE_SIZE_4BPP * (gSprites[healthboxSpriteId].oam.tileNum + var + i)),
                      0x20);
        }
    }
}

// Fantasy Retold: only the player's plates carry HP numbers; they are always shown next to the bars.
void UpdateHpTextInHealthbox(u32 healthboxSpriteId, u32 maxOrCurrent, s16 currHp, s16 maxHp)
{
    if (GetHealthboxLayout(gSprites[healthboxSpriteId].hMain_Battler)->hpNumbers)
        PrintHpOnHealthbox(healthboxSpriteId, currHp, maxHp);
}

// Prints mon's nature, catch and flee rate. Probably used to test pokeblock-related features.
UNUSED static void PrintSafariMonInfo(u8 healthboxSpriteId, struct Pokemon *mon)
{
    u8 text[23];
    s32 j, spriteTileNum;
    u8 *barFontGfx;
    u8 i, var, nature, healthBarSpriteId;

    memcpy(text, sEmptyWhiteText_GrayHighlight, sizeof(sEmptyWhiteText_GrayHighlight));
    barFontGfx = &gMonSpritesGfxPtr->barFontGfx[0x520 + (GetBattlerPosition(gSprites[healthboxSpriteId].hMain_Battler) * 384)];
    var = 5;
    nature = GetNature(mon);
    StringCopy(&text[9], gNaturesInfo[nature].name);
    RenderTextHandleBold(barFontGfx, FONT_BOLD, text);

    for (j = 9, i = 0; i < var; i++, j++)
    {
        u8 elementId;

        if ((text[j] >= 55 && text[j] <= 74) || (text[j] >= 135 && text[j] <= 154))
            elementId = 44;
        else if ((text[j] >= 75 && text[j] <= 79) || (text[j] >= 155 && text[j] <= 159))
            elementId = 45;
        else
            elementId = 43;

        CpuCopy32(GetHealthboxElementGfxPtr(elementId), barFontGfx + (i * 64), 0x20);
    }

    for (j = 1; j < var + 1; j++)
    {
        spriteTileNum = (gSprites[healthboxSpriteId].oam.tileNum + (j % 8) + (j / 8 * 64)) * TILE_SIZE_4BPP;
        CpuCopy32(barFontGfx, (void *)(OBJ_VRAM0) + (spriteTileNum), 0x20);
        barFontGfx += 0x20;

        spriteTileNum = (8 + gSprites[healthboxSpriteId].oam.tileNum + (j % 8) + (j / 8 * 64)) * TILE_SIZE_4BPP;
        CpuCopy32(barFontGfx, (void *)(OBJ_VRAM0) + (spriteTileNum), 0x20);
        barFontGfx += 0x20;
    }

    healthBarSpriteId = gSprites[healthboxSpriteId].hMain_HealthBarSpriteId;
    ConvertIntToDecimalStringN(&text[9], gBattleStruct->safariCatchFactor, STR_CONV_MODE_RIGHT_ALIGN, 2);
    ConvertIntToDecimalStringN(&text[12], gBattleStruct->safariEscapeFactor, STR_CONV_MODE_RIGHT_ALIGN, 2);
    text[5] = TEXT_COLOR_TRANSPARENT;
    text[8] = TEXT_COLOR_TRANSPARENT;
    text[11] = CHAR_SLASH;
    RenderTextHandleBold(gMonSpritesGfxPtr->barFontGfx, FONT_BOLD, text);

    j = healthBarSpriteId; // Needed to match for some reason.
    for (j = 0; j < 5; j++)
    {
        if (j <= 1)
        {
            CpuCopy32(&gMonSpritesGfxPtr->barFontGfx[0x40 * j + 0x20],
                      (void *)(OBJ_VRAM0) + (gSprites[healthBarSpriteId].oam.tileNum + 2 + j) * TILE_SIZE_4BPP,
                      32);
        }
        else
        {
            CpuCopy32(&gMonSpritesGfxPtr->barFontGfx[0x40 * j + 0x20],
                      (void *)(OBJ_VRAM0 + 0xC0) + (j + gSprites[healthBarSpriteId].oam.tileNum) * TILE_SIZE_4BPP,
                      32);
        }
    }
}

// Fantasy Retold: the plates always show the bars, and the player's also show the HP numbers, so there is
// nothing to swap. The opponent's HP stays hidden by design (no numbers or percentage).
void SwapHpBarsWithHpText(void)
{
}

#undef tBattler
#undef tType
#undef tPosX
#undef tLevelXDelta

#define tBattler                data[0]
#define tSummaryBarSpriteId     data[1]
#define tBallIconSpriteId(n)    data[3 + n]
#define tIsBattleStart          data[10]
#define tBlend                  data[15]

u8 CreatePartyStatusSummarySprites(enum BattlerId battler, struct HpAndStatus *partyInfo, bool8 skipPlayer, bool8 isBattleStart)
{
    bool8 isOpponent;
    s16 bar_X, bar_Y, bar_pos2_X, bar_data0;
    s32 i, j, var;
    u8 summaryBarSpriteId;
    u8 ballIconSpritesIds[PARTY_SIZE];
    u8 taskId;

    if (!skipPlayer)
    {
        if (IsOnPlayerSide(battler))
        {
            isOpponent = FALSE;
            bar_X = 136;
            if (BattleSideHasTwoTrainers(B_SIDE_PLAYER))
                bar_Y = GetBattlerPosition(battler) == B_POSITION_PLAYER_LEFT ? 90 : 106;
            else
                bar_Y = 96;
            bar_pos2_X = 100;
            bar_data0 = -5;
        }
        else
        {
            isOpponent = TRUE;

            if (GetBattlerCoordsIndex(battler) == BATTLE_COORDS_SINGLES)
            {
                bar_X = 104, bar_Y = 24;
                if (GetBattlerPosition(battler) == B_POSITION_OPPONENT_LEFT)
                    bar_Y = 40;
            }
            else
            {
                bar_X = 104;
                if (BattleSideHasTwoTrainers(B_SIDE_OPPONENT) && GetBattlerPosition(battler) == B_POSITION_OPPONENT_LEFT)
                    bar_Y = 16;
                else
                    bar_Y = 32;
            }

            bar_pos2_X = -100;
            bar_data0 = 5;
        }
    }
    else
    {
        isOpponent = TRUE;
        bar_X = 104, bar_Y = 40;
        if (GetBattlerCoordsIndex(battler) == BATTLE_COORDS_DOUBLES && GetBattlerPosition(battler) == B_POSITION_OPPONENT_LEFT)
            bar_Y = 24;
        bar_pos2_X = -100;
        bar_data0 = 5;
    }

    LoadCompressedSpriteSheetUsingHeap(&sStatusSummaryBarSpriteSheet);
    LoadSpriteSheet(&sStatusSummaryBallsSpriteSheet);
    LoadSpritePalette(&sStatusSummaryBarSpritePal);
    LoadSpritePalette(&sStatusSummaryBallsSpritePal);

    summaryBarSpriteId = CreateSprite(&sStatusSummaryBarSpriteTemplates[isOpponent], bar_X, bar_Y, 10);
    SetSubspriteTables(&gSprites[summaryBarSpriteId], sStatusSummaryBar_SubspriteTable_Enter);
    gSprites[summaryBarSpriteId].x2 = bar_pos2_X;
    gSprites[summaryBarSpriteId].data[0] = bar_data0;

    if (isOpponent)
    {
        gSprites[summaryBarSpriteId].x -= 96;
        gSprites[summaryBarSpriteId].oam.matrixNum = ST_OAM_HFLIP;
    }
    else
    {
        gSprites[summaryBarSpriteId].x += 96;
    }

    for (i = 0; i < PARTY_SIZE; i++)
    {
        ballIconSpritesIds[i] = CreateSpriteAtEnd(&sStatusSummaryBallsSpriteTemplates[isOpponent], bar_X, bar_Y - 4, 9);

        if (!isBattleStart)
            gSprites[ballIconSpritesIds[i]].callback = SpriteCB_StatusSummaryBalls_OnSwitchout;

        if (!isOpponent)
        {
            gSprites[ballIconSpritesIds[i]].x2 = 0;
            gSprites[ballIconSpritesIds[i]].y2 = 0;
        }

        gSprites[ballIconSpritesIds[i]].data[0] = summaryBarSpriteId;

        if (!isOpponent)
        {
            gSprites[ballIconSpritesIds[i]].x += 10 * i + 24;
            gSprites[ballIconSpritesIds[i]].data[1] = i * 7 + 10;
            gSprites[ballIconSpritesIds[i]].x2 = 120;
        }
        else
        {
            gSprites[ballIconSpritesIds[i]].x -= 10 * (5 - i) + 24;
            gSprites[ballIconSpritesIds[i]].data[1] = (6 - i) * 7 + 10;
            gSprites[ballIconSpritesIds[i]].x2 = -120;
        }

        gSprites[ballIconSpritesIds[i]].data[2] = isOpponent;
    }

    if (IsOnPlayerSide(battler))
    {
        if (gBattleTypeFlags & BATTLE_TYPE_MULTI)
        {
            for (i = 0; i < PARTY_SIZE; i++)
            {
                if (partyInfo[i].hp == HP_EMPTY_SLOT)
                {
                    // empty slot or an egg
                    gSprites[ballIconSpritesIds[i]].oam.tileNum += 1;
                    gSprites[ballIconSpritesIds[i]].data[7] = 1;
                }
                else if (partyInfo[i].hp == 0)
                {
                    // fainted mon
                    gSprites[ballIconSpritesIds[i]].oam.tileNum += 3;
                }
                else if (partyInfo[i].status != 0)
                {
                    // mon with major status
                    gSprites[ballIconSpritesIds[i]].oam.tileNum += 2;
                }
            }
        }
        else
        {
            for (i = 0, var = PARTY_SIZE - 1, j = 0; j < PARTY_SIZE; j++)
            {
                if (partyInfo[j].hp == HP_EMPTY_SLOT)
                {
                     // empty slot or an egg
                    gSprites[ballIconSpritesIds[var]].oam.tileNum += 1;
                    gSprites[ballIconSpritesIds[var]].data[7] = 1;
                    var--;
                    continue;
                }
                else if (partyInfo[j].hp == 0)
                {
                    // fainted mon
                    gSprites[ballIconSpritesIds[i]].oam.tileNum += 3;
                }
                else if (gBattleTypeFlags & BATTLE_TYPE_ARENA && gBattleStruct->arenaLostPlayerMons & (1u << j))
                {
                    // fainted arena mon
                    gSprites[ballIconSpritesIds[i]].oam.tileNum += 3;
                }
                else if (partyInfo[j].status != 0)
                {
                    // mon with primary status
                    gSprites[ballIconSpritesIds[i]].oam.tileNum += 2;
                }
                i++;
            }
        }
    }
    else
    {
        if (gBattleTypeFlags & (BATTLE_TYPE_MULTI | BATTLE_TYPE_TWO_OPPONENTS))
        {
            for (var = PARTY_SIZE - 1, i = 0; i < PARTY_SIZE; i++)
            {
                if (partyInfo[i].hp == HP_EMPTY_SLOT)
                {
                    // empty slot or an egg
                    gSprites[ballIconSpritesIds[var]].oam.tileNum += 1;
                    gSprites[ballIconSpritesIds[var]].data[7] = 1;
                }
                else if (partyInfo[i].hp == 0)
                {
                    // fainted mon
                    gSprites[ballIconSpritesIds[var]].oam.tileNum += 3;
                }
                else if (partyInfo[i].status != 0)
                {
                    // mon with primary status
                    gSprites[ballIconSpritesIds[var]].oam.tileNum += 2;
                }
                var--;
            }
        }
        else
        {
            for (var = 0, i = 0, j = 0; j < PARTY_SIZE; j++)
            {
                if (partyInfo[j].hp == HP_EMPTY_SLOT)
                {
                    // empty slot or an egg
                    gSprites[ballIconSpritesIds[i]].oam.tileNum += 1;
                    gSprites[ballIconSpritesIds[i]].data[7] = 1;
                    i++;
                    continue;
                }
                else if (partyInfo[j].hp == 0)
                {
                     // fainted mon
                    gSprites[ballIconSpritesIds[PARTY_SIZE - 1 - var]].oam.tileNum += 3;
                }
                else if (gBattleTypeFlags & BATTLE_TYPE_ARENA && gBattleStruct->arenaLostOpponentMons & (1u << j))
                {
                     // fainted arena mon
                    gSprites[ballIconSpritesIds[PARTY_SIZE - 1 - var]].oam.tileNum += 3;
                }
                else if (partyInfo[j].status != 0)
                {
                     // mon with primary status
                    gSprites[ballIconSpritesIds[PARTY_SIZE - 1 - var]].oam.tileNum += 2;
                }
                var++;
            }
        }
    }

    taskId = CreateTask(TaskDummy, 5);
    gTasks[taskId].tBattler = battler;
    gTasks[taskId].tSummaryBarSpriteId = summaryBarSpriteId;

    for (i = 0; i < PARTY_SIZE; i++)
        gTasks[taskId].tBallIconSpriteId(i) = ballIconSpritesIds[i];

    gTasks[taskId].tIsBattleStart = isBattleStart;

    if (isBattleStart)
    {
        gBattleSpritesDataPtr->animationData->field_9_x1C++;
    }

    PlaySE12WithPanning(SE_BALL_TRAY_ENTER, 0);
    return taskId;
}

// Slide the party summary tray back offscreen
void Task_HidePartyStatusSummary(u8 taskId)
{
    u8 ballIconSpriteIds[PARTY_SIZE];
    bool8 isBattleStart;
    u8 summaryBarSpriteId;
    enum BattlerId battler;
    s32 i;

    isBattleStart = gTasks[taskId].tIsBattleStart;
    summaryBarSpriteId = gTasks[taskId].tSummaryBarSpriteId;
    battler = gTasks[taskId].tBattler;

    for (i = 0; i < PARTY_SIZE; i++)
        ballIconSpriteIds[i] = gTasks[taskId].tBallIconSpriteId(i);

    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT2_ALL | BLDCNT_EFFECT_BLEND);
    SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(16, 0));

    gTasks[taskId].tBlend = 16;

    for (i = 0; i < PARTY_SIZE; i++)
        gSprites[ballIconSpriteIds[i]].oam.objMode = ST_OAM_OBJ_BLEND;

    gSprites[summaryBarSpriteId].oam.objMode = ST_OAM_OBJ_BLEND;

    if (isBattleStart)
    {
        for (i = 0; i < PARTY_SIZE; i++)
        {
            if (!IsOnPlayerSide(battler))
            {
                gSprites[ballIconSpriteIds[PARTY_SIZE - 1 - i]].data[1] = 7 * i;
                gSprites[ballIconSpriteIds[PARTY_SIZE - 1 - i]].data[3] = 0;
                gSprites[ballIconSpriteIds[PARTY_SIZE - 1 - i]].data[4] = 0;
                gSprites[ballIconSpriteIds[PARTY_SIZE - 1 - i]].callback = SpriteCB_StatusSummaryBalls_Exit;
            }
            else
            {
                gSprites[ballIconSpriteIds[i]].data[1] = 7 * i;
                gSprites[ballIconSpriteIds[i]].data[3] = 0;
                gSprites[ballIconSpriteIds[i]].data[4] = 0;
                gSprites[ballIconSpriteIds[i]].callback = SpriteCB_StatusSummaryBalls_Exit;
            }
        }
        gSprites[summaryBarSpriteId].data[0] /= 2;
        gSprites[summaryBarSpriteId].data[1] = 0;
        gSprites[summaryBarSpriteId].callback = SpriteCB_StatusSummaryBar_Exit;
        SetSubspriteTables(&gSprites[summaryBarSpriteId], sStatusSummaryBar_SubspriteTable_Exit);
        gTasks[taskId].func = Task_HidePartyStatusSummary_BattleStart_1;
    }
    else
    {
        gTasks[taskId].func = Task_HidePartyStatusSummary_DuringBattle;
    }
}

static void Task_HidePartyStatusSummary_BattleStart_1(u8 taskId)
{
    if ((gTasks[taskId].data[11]++ % 2) == 0)
    {
        if (--gTasks[taskId].tBlend < 0)
            return;

        SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(gTasks[taskId].tBlend, 16 - gTasks[taskId].tBlend));
    }
    if (gTasks[taskId].tBlend == 0)
        gTasks[taskId].func = Task_HidePartyStatusSummary_BattleStart_2;
}

static void Task_HidePartyStatusSummary_BattleStart_2(u8 taskId)
{
    u8 ballIconSpriteIds[PARTY_SIZE];
    s32 i;

    enum BattlerId battler = gTasks[taskId].tBattler;
    if (--gTasks[taskId].tBlend == -1)
    {
        u8 summaryBarSpriteId = gTasks[taskId].tSummaryBarSpriteId;

        for (i = 0; i < PARTY_SIZE; i++)
            ballIconSpriteIds[i] = gTasks[taskId].tBallIconSpriteId(i);

        gBattleSpritesDataPtr->animationData->field_9_x1C--;
        if (gBattleSpritesDataPtr->animationData->field_9_x1C == 0)
        {
            DestroySpriteAndFreeResources(&gSprites[summaryBarSpriteId]);
            DestroySpriteAndFreeResources(&gSprites[ballIconSpriteIds[0]]);
        }
        else
        {
            FreeSpriteOamMatrix(&gSprites[summaryBarSpriteId]);
            DestroySprite(&gSprites[summaryBarSpriteId]);
            FreeSpriteOamMatrix(&gSprites[ballIconSpriteIds[0]]);
            DestroySprite(&gSprites[ballIconSpriteIds[0]]);
        }

        for (i = 1; i < PARTY_SIZE; i++)
            DestroySprite(&gSprites[ballIconSpriteIds[i]]);
    }
    else if (gTasks[taskId].tBlend == -3)
    {
        gBattleSpritesDataPtr->healthBoxesData[battler].partyStatusSummaryShown = 0;
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        DestroyTask(taskId);
    }
}

static void Task_HidePartyStatusSummary_DuringBattle(u8 taskId)
{
    u8 ballIconSpriteIds[PARTY_SIZE];
    s32 i;
    enum BattlerId battler = gTasks[taskId].tBattler;

    if (--gTasks[taskId].tBlend >= 0)
    {
        SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(gTasks[taskId].tBlend, 16 - gTasks[taskId].tBlend));
    }
    else if (gTasks[taskId].tBlend == -1)
    {
        u8 summaryBarSpriteId = gTasks[taskId].tSummaryBarSpriteId;

        for (i = 0; i < PARTY_SIZE; i++)
            ballIconSpriteIds[i] = gTasks[taskId].tBallIconSpriteId(i);

        DestroySpriteAndFreeResources(&gSprites[summaryBarSpriteId]);
        DestroySpriteAndFreeResources(&gSprites[ballIconSpriteIds[0]]);

        for (i = 1; i < PARTY_SIZE; i++)
            DestroySprite(&gSprites[ballIconSpriteIds[i]]);
    }
    else if (gTasks[taskId].tBlend == -3)
    {
        gBattleSpritesDataPtr->healthBoxesData[battler].partyStatusSummaryShown = 0;
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        DestroyTask(taskId);
    }
}

#undef tBattler
#undef tSummaryBarSpriteId
#undef tBallIconSpriteId
#undef tIsBattleStart
#undef tBlend

static void SpriteCB_StatusSummaryBar_Enter(struct Sprite *sprite)
{
    if (sprite->x2 != 0)
        sprite->x2 += sprite->data[0];
}

static void SpriteCB_StatusSummaryBar_Exit(struct Sprite *sprite)
{
    sprite->data[1] += 32;
    if (sprite->data[0] > 0)
        sprite->x2 += sprite->data[1] >> 4;
    else
        sprite->x2 -= sprite->data[1] >> 4;
    sprite->data[1] &= 0xF;
}

static void SpriteCB_StatusSummaryBalls_Enter(struct Sprite *sprite)
{
    u8 var1;
    u16 var2;
    s8 pan;

    if (sprite->data[1] > 0)
    {
        sprite->data[1]--;
        return;
    }

    var1 = sprite->data[2];
    var2 = sprite->data[3];
    var2 += 56;
    sprite->data[3] = var2 & 0xFFF0;

    if (var1 != 0)
    {
        sprite->x2 += var2 >> 4;
        if (sprite->x2 > 0)
            sprite->x2 = 0;
    }
    else
    {
        sprite->x2 -= var2 >> 4;
        if (sprite->x2 < 0)
            sprite->x2 = 0;
    }

    if (sprite->x2 == 0)
    {
        pan = SOUND_PAN_TARGET;
        if (var1 != 0)
            pan = SOUND_PAN_ATTACKER;

        if (sprite->data[7] != 0)
            PlaySE2WithPanning(SE_BALL_TRAY_EXIT, pan);
        else
            PlaySE1WithPanning(SE_BALL_TRAY_BALL, pan);

        sprite->callback = SpriteCallbackDummy;
    }
}

static void SpriteCB_StatusSummaryBalls_Exit(struct Sprite *sprite)
{
    u8 var1;
    u16 var2;

    if (sprite->data[1] > 0)
    {
        sprite->data[1]--;
        return;
    }
    var1 = sprite->data[2];
    var2 = sprite->data[3];
    var2 += 56;
    sprite->data[3] = var2 & 0xFFF0;
    if (var1 != 0)
        sprite->x2 += var2 >> 4;
    else
        sprite->x2 -= var2 >> 4;
    if (sprite->x2 + sprite->x > 248
     || sprite->x2 + sprite->x < -8)
    {
        sprite->invisible = TRUE;
        sprite->callback = SpriteCallbackDummy;
    }
}

static void SpriteCB_StatusSummaryBalls_OnSwitchout(struct Sprite *sprite)
{
    u8 barSpriteId = sprite->data[0];

    sprite->x2 = gSprites[barSpriteId].x2;
    sprite->y2 = gSprites[barSpriteId].y2;
}

void UpdateNickInHealthbox(u8 healthboxSpriteId, struct Pokemon *mon)
{
    u8 nickname[POKEMON_NAME_LENGTH + 1];
    u8 *ptr;
    enum Species species;
    u8 gender;
    struct Pokemon *illusionMon = GetIllusionMonPtr(gSprites[healthboxSpriteId].hMain_Battler);
    if (illusionMon != NULL)
        mon = illusionMon;

    GetMonData(mon, MON_DATA_NICKNAME, nickname);
    StringGet_Nickname(nickname);
    ptr = StringCopy(gDisplayedStringBattle, nickname);

    gender = GetMonGender(mon);
    species = GetMonData(mon, MON_DATA_SPECIES);

    if ((species == SPECIES_NIDORAN_F || species == SPECIES_NIDORAN_M) && StringCompare(nickname, GetSpeciesName(species)) == 0)
        gender = 100;

    if (GetBattlerSide(gSprites[healthboxSpriteId].hMain_Battler) == B_SIDE_OPPONENT && IsGhostBattleWithoutScope())
        gender = 100;

    // Fantasy Retold: the symbol takes the plate's blue / red instead of the shared dynamic text colours.
    if (gender == MON_MALE || gender == MON_FEMALE)
    {
        *ptr++ = EXT_CTRL_CODE_BEGIN;
        *ptr++ = EXT_CTRL_CODE_COLOR;
        *ptr++ = (gender == MON_MALE) ? HB_COLOR_BLUE : HB_COLOR_RED;
        *ptr++ = (gender == MON_MALE) ? CHAR_MALE : CHAR_FEMALE;
    }
    *ptr = EOS;

    enum BattlerId battler = gSprites[healthboxSpriteId].hMain_Battler;
    u32 nameX = 5 + (ShouldShowCaughtBallIcon(battler) ? HB_BALL_ICON_W : 0);
    u32 nameRight = GetHealthboxTextRight(GetHealthboxLayout(battler)) - HB_LEVEL_TEXT_W - 2;
    u32 fontId = GetFontIdToFit(gDisplayedStringBattle, FONT_SMALL, 0, nameRight - nameX);

    FillHealthboxRect(healthboxSpriteId, 4, 2, nameRight - 4, 10, HEALTHBOX_BG_INDEX);
    PrintOnHealthbox(healthboxSpriteId, fontId, nameX, HB_TEXT_Y, gDisplayedStringBattle);
    TryAddPokeballIconToHealthbox(healthboxSpriteId, TRUE);
}

// Fantasy Retold: a 5x5 ball before the name of a wild mon whose species is already caught.
static bool32 ShouldShowCaughtBallIcon(enum BattlerId battler)
{
    if (gBattleTypeFlags & (BATTLE_TYPE_CATCH_TUTORIAL | BATTLE_TYPE_TRAINER))
        return FALSE;
    if (IsOnPlayerSide(battler))
        return FALSE;
    if (IsGhostBattleWithoutScope())
        return FALSE;
    return GetSetPokedexFlag(SpeciesToNationalPokedexNum(GetMonData(GetBattlerMon(battler), MON_DATA_SPECIES)), FLAG_GET_CAUGHT);
}

static const u8 sCaughtBallIcon[5][5] =
{
    {0,              HB_COLOR_RED,   HB_COLOR_RED,   HB_COLOR_RED,   0},
    {HB_COLOR_RED,   HB_COLOR_RED,   HB_COLOR_RED,   HB_COLOR_RED,   HB_COLOR_RED},
    {HB_COLOR_SLATE, HB_COLOR_SLATE, HB_COLOR_WHITE, HB_COLOR_SLATE, HB_COLOR_SLATE},
    {HB_COLOR_WHITE, HB_COLOR_WHITE, HB_COLOR_WHITE, HB_COLOR_WHITE, HB_COLOR_WHITE},
    {0,              HB_COLOR_WHITE, HB_COLOR_WHITE, HB_COLOR_WHITE, 0},
};

void TryAddPokeballIconToHealthbox(u8 healthboxSpriteId, bool8 noStatus)
{
    u32 x, y;

    if (!ShouldShowCaughtBallIcon(gSprites[healthboxSpriteId].hMain_Battler))
        return;

    for (y = 0; y < 5; y++)
    {
        for (x = 0; x < 5; x++)
        {
            if (sCaughtBallIcon[y][x] != 0)
                FillHealthboxRect(healthboxSpriteId, 5 + x, 5 + y, 1, 1, sCaughtBallIcon[y][x]);
        }
    }
}

// Fantasy Retold: statuses live in the status column (the healthbar sprite), not on the plate.
static void UpdateStatusIconInHealthbox(u8 healthboxSpriteId)
{
    DrawHealthboxStatusColumn(&gSprites[gSprites[healthboxSpriteId].hMain_HealthBarSpriteId], gSprites[healthboxSpriteId].hMain_Battler);
}

static void UpdateSafariBallsTextOnHealthbox(u8 healthboxSpriteId)
{
    PrintOnHealthbox(healthboxSpriteId, FONT_SMALL, 5, HB_TEXT_Y, gText_SafariBalls);
}

static void UpdateLeftNoOfBallsTextOnHealthbox(u8 healthboxSpriteId)
{
    u8 text[16];
    u8 *txtPtr;

    txtPtr = StringCopy(text, gText_SafariBallLeft);
    ConvertIntToDecimalStringN(txtPtr, gNumSafariBalls, STR_CONV_MODE_LEFT_ALIGN, 2);

    FillHealthboxRect(healthboxSpriteId, 4, HB_HP_TEXT_Y + 3, 60, 8, HEALTHBOX_BG_INDEX);
    PrintOnHealthbox(healthboxSpriteId, FONT_SMALL, 5, HB_HP_TEXT_Y, text);
}

void UpdateHealthboxAttribute(u8 healthboxSpriteId, struct Pokemon *mon, u8 elementId)
{
    enum BattlerId battler = gSprites[healthboxSpriteId].hMain_Battler;
    s32 maxHp = GetMonData(mon, MON_DATA_MAX_HP);
    s32 currHp = GetMonData(mon, MON_DATA_HP);

    if (IsOnPlayerSide(battler))
    {
        u8 isDoubles = GetBattlerCoordsIndex(battler) == BATTLE_COORDS_DOUBLES;

        if (elementId == HEALTHBOX_LEVEL || elementId == HEALTHBOX_ALL)
            UpdateLvlInHealthbox(healthboxSpriteId, GetMonData(mon, MON_DATA_LEVEL));

        if (elementId == HEALTHBOX_ALL)
            UpdateHpTextInHealthbox(healthboxSpriteId, HP_BOTH, currHp, maxHp);
        else if (elementId == HEALTHBOX_MAX_HP)
            UpdateHpTextInHealthbox(healthboxSpriteId, HP_MAX, currHp, maxHp);
        else if (elementId == HEALTHBOX_CURRENT_HP)
            UpdateHpTextInHealthbox(healthboxSpriteId, HP_CURRENT, currHp, maxHp);

        if (elementId == HEALTHBOX_HEALTH_BAR || elementId == HEALTHBOX_ALL)
        {
            LoadBattleBarGfx(0);
            SetBattleBarStruct(battler, healthboxSpriteId, maxHp, currHp, 0);
            if (elementId == HEALTHBOX_ALL)
                sTraumaShown[battler] = GetLiveTraumaForBar(battler);
            MoveBattleBar(battler, healthboxSpriteId, HEALTH_BAR, 0);
            // Fantasy Retold: refresh the Stamina gauge whenever the HP bar redraws; a full redraw (send-out)
            // shows the live value at once instead of sliding to it.
            if (elementId == HEALTHBOX_ALL)
                sStaminaShown[battler] = GetLiveStaminaForBar(battler);
            DrawStaminaGaugeOnHealthbox(healthboxSpriteId, battler);
        }

        if (!isDoubles && (elementId == HEALTHBOX_EXP_BAR || elementId == HEALTHBOX_ALL))
        {
            enum Species species;
            u32 exp, currLevelExp;
            s32 currExpBarValue, maxExpBarValue;
            u8 level;

            LoadBattleBarGfx(3);
            species = GetMonData(mon, MON_DATA_SPECIES);
            level = GetMonData(mon, MON_DATA_LEVEL);
            exp = GetMonData(mon, MON_DATA_EXP);
            currLevelExp = gExperienceTables[gSpeciesInfo[species].growthRate][level];
            currExpBarValue = exp - currLevelExp;
            maxExpBarValue = gExperienceTables[gSpeciesInfo[species].growthRate][level + 1] - currLevelExp;
            SetBattleBarStruct(battler, healthboxSpriteId, maxExpBarValue, currExpBarValue, isDoubles);
            MoveBattleBar(battler, healthboxSpriteId, EXP_BAR, 0);
        }
        if (elementId == HEALTHBOX_NICK || elementId == HEALTHBOX_ALL)
            UpdateNickInHealthbox(healthboxSpriteId, mon);
        if (elementId == HEALTHBOX_STATUS_ICON || elementId == HEALTHBOX_ALL)
            UpdateStatusIconInHealthbox(healthboxSpriteId);
        if (elementId == HEALTHBOX_SAFARI_ALL_TEXT)
            UpdateSafariBallsTextOnHealthbox(healthboxSpriteId);
        if (elementId == HEALTHBOX_SAFARI_ALL_TEXT || elementId == HEALTHBOX_SAFARI_BALLS_TEXT)
            UpdateLeftNoOfBallsTextOnHealthbox(healthboxSpriteId);
    }
    else
    {
        if (elementId == HEALTHBOX_LEVEL || elementId == HEALTHBOX_ALL)
            UpdateLvlInHealthbox(healthboxSpriteId, GetMonData(mon, MON_DATA_LEVEL));

        if (elementId == HEALTHBOX_ALL)
            UpdateHpTextInHealthbox(healthboxSpriteId, HP_BOTH, currHp, maxHp);
        else if (elementId == HEALTHBOX_MAX_HP)
            UpdateHpTextInHealthbox(healthboxSpriteId, HP_MAX, currHp, maxHp);
        else if (elementId == HEALTHBOX_CURRENT_HP)
            UpdateHpTextInHealthbox(healthboxSpriteId, HP_CURRENT, currHp, maxHp);

        if (elementId == HEALTHBOX_HEALTH_BAR || elementId == HEALTHBOX_ALL)
        {
            LoadBattleBarGfx(0);
            SetBattleBarStruct(battler, healthboxSpriteId, maxHp, currHp, 0);
            if (elementId == HEALTHBOX_ALL)
                sTraumaShown[battler] = GetLiveTraumaForBar(battler);
            MoveBattleBar(battler, healthboxSpriteId, HEALTH_BAR, 0);
            // Fantasy Retold: refresh the Stamina gauge whenever the HP bar redraws; a full redraw (send-out)
            // shows the live value at once instead of sliding to it.
            if (elementId == HEALTHBOX_ALL)
                sStaminaShown[battler] = GetLiveStaminaForBar(battler);
            DrawStaminaGaugeOnHealthbox(healthboxSpriteId, battler);
        }
        if (elementId == HEALTHBOX_NICK || elementId == HEALTHBOX_ALL)
            UpdateNickInHealthbox(healthboxSpriteId, mon);
        if (elementId == HEALTHBOX_STATUS_ICON || elementId == HEALTHBOX_ALL)
            UpdateStatusIconInHealthbox(healthboxSpriteId);
    }
}

// Fantasy Retold: redraw just the Stamina gauge for one battler. Called from battle logic right after
// Stamina changes (action cost, attrition); the bar then slides to the new value (SpriteCB_HealthBar).
void UpdateStaminaGaugeOnHealthbox(enum BattlerId battler)
{
    u8 spriteId = gHealthboxSpriteIds[battler];
    if (spriteId >= MAX_SPRITES) // healthbox not created yet (or already torn down)
        return;
    DrawStaminaGaugeOnHealthbox(spriteId, battler);
}

#define B_EXPBAR_PIXELS 64
#define B_HEALTHBAR_PIXELS 48

s32 MoveBattleBar(enum BattlerId battler, u8 healthboxSpriteId, u8 whichBar, u8 unused)
{
    s32 currentBarValue;

    if (whichBar == HEALTH_BAR) // health bar
    {
        u16 hpFraction = B_FAST_HP_DRAIN == FALSE ? 1 : max(gBattleSpritesDataPtr->battleBars[battler].maxValue / (B_HEALTHBAR_PIXELS / 2), 1);
        currentBarValue = CalcNewBarValue(gBattleSpritesDataPtr->battleBars[battler].maxValue,
                    gBattleSpritesDataPtr->battleBars[battler].oldValue,
                    gBattleSpritesDataPtr->battleBars[battler].receivedValue,
                    &gBattleSpritesDataPtr->battleBars[battler].currValue,
                    B_HEALTHBAR_PIXELS / 8, hpFraction);
    }
    else // exp bar
    {
        u16 expFraction = GetScaledExpFraction(gBattleSpritesDataPtr->battleBars[battler].oldValue,
                    gBattleSpritesDataPtr->battleBars[battler].receivedValue,
                    gBattleSpritesDataPtr->battleBars[battler].maxValue, 8);
        if (expFraction == 0)
            expFraction = 1;
        expFraction = abs(gBattleSpritesDataPtr->battleBars[battler].receivedValue / expFraction);

        currentBarValue = CalcNewBarValue(gBattleSpritesDataPtr->battleBars[battler].maxValue,
                    gBattleSpritesDataPtr->battleBars[battler].oldValue,
                    gBattleSpritesDataPtr->battleBars[battler].receivedValue,
                    &gBattleSpritesDataPtr->battleBars[battler].currValue,
                    B_EXPBAR_PIXELS / 8, expFraction);
    }

    MoveBattleBarGraphically(battler, whichBar);

    if (currentBarValue == -1)
        gBattleSpritesDataPtr->battleBars[battler].currValue = 0;

    return currentBarValue;
}

// Fantasy Retold: width in pixels of a bar's current value. CalcNewBarValue keeps currValue in Q24.8
// while maxValue is below the bar's pacing scale; any non-zero value draws at least 1px.
static u32 GetBattleBarPixels(enum BattlerId battler, u32 scalePixels, u32 width)
{
    s32 maxValue = gBattleSpritesDataPtr->battleBars[battler].maxValue;
    s32 currValue = gBattleSpritesDataPtr->battleBars[battler].currValue;
    u32 pixels;

    if (maxValue <= 0 || currValue <= 0)
        return 0;

    if (maxValue < (s32)scalePixels)
        pixels = (currValue * width) / Q_24_8(maxValue);
    else
        pixels = (currValue * width) / maxValue;

    if (pixels == 0)
        pixels = 1;
    return min(pixels, width);
}

static void MoveBattleBarGraphically(enum BattlerId battler, u8 whichBar)
{
    u32 healthboxSpriteId = gBattleSpritesDataPtr->battleBars[battler].healthboxSpriteId;

    switch (whichBar)
    {
    case HEALTH_BAR:
        DrawHealthboxHpBar(healthboxSpriteId, battler, gBattleSpritesDataPtr->battleBars[battler].maxValue,
                           GetBattleBarPixels(battler, B_HEALTHBAR_PIXELS, GetHealthboxLayout(battler)->barW));
        break;
    case EXP_BAR:
        if (GetMonData(GetBattlerMon(battler), MON_DATA_LEVEL) >= MAX_LEVEL)
            DrawHealthboxExpBar(healthboxSpriteId, 0);
        else
            DrawHealthboxExpBar(healthboxSpriteId, GetBattleBarPixels(battler, B_EXPBAR_PIXELS, HB_EXP_W));
        break;
    }
}

static s32 CalcNewBarValue(s32 maxValue, s32 oldValue, s32 receivedValue, s32 *currValue, u8 scale, u16 toAdd)
{
    s32 ret, newValue;
    scale *= 8;

    if (*currValue == -32768) // first function call
    {
        if (maxValue < scale)
            *currValue = Q_24_8(oldValue);
        else
            *currValue = oldValue;
    }

    newValue = SubtractClamped(HP_EMPTY, maxValue, oldValue, receivedValue);
    if (maxValue < scale)
    {
        if (newValue == Q_24_8_TO_INT(*currValue) && (*currValue & 0xFF) == 0)
            return -1;
    }
    else
    {
        if (newValue == *currValue) // we're done, the bar's value has been updated
            return -1;
    }

    if (maxValue < scale) // handle cases of max var having less pixels than the whole bar
    {
        s32 toAdd = Q_24_8(maxValue) / scale;

        if (receivedValue < 0) // fill bar right
        {
            *currValue += toAdd;
            ret = Q_24_8_TO_INT(*currValue);
            if (ret >= newValue)
            {
                *currValue = Q_24_8(newValue);
                ret = newValue;
            }
        }
        else // move bar left
        {
            *currValue -= toAdd;
            ret = Q_24_8_TO_INT(*currValue);
            // try round up
            if ((*currValue & 0xFF) > 0)
                ret++;
            if (ret <= newValue)
            {
                *currValue = Q_24_8(newValue);
                ret = newValue;
            }
        }
    }
    else
    {
        if (receivedValue < 0) // fill bar right
        {
            *currValue += toAdd;
            if (*currValue > newValue)
                *currValue = newValue;
            ret = *currValue;
        }
        else // move bar left
        {
            *currValue -= toAdd;
            if (*currValue < newValue)
                *currValue = newValue;
            ret = *currValue;
        }
    }

    return ret;
}

static u8 GetScaledExpFraction(s32 oldValue, s32 receivedValue, s32 maxValue, u8 scale)
{
    s32 newVal, result;
    s8 oldToMax, newToMax;

    scale *= (B_FAST_EXP_GROW) ? 2 : 8;
    newVal = SubtractClamped(HP_EMPTY, maxValue, oldValue, receivedValue);

    oldToMax = oldValue * scale / maxValue;
    newToMax = newVal * scale / maxValue;
    result = oldToMax - newToMax;

    return abs(result);
}

u8 GetScaledHPFraction(s16 hp, s16 maxhp, u8 scale)
{
    u8 result = hp * scale / maxhp;

    if (result == 0 && hp > 0)
        return 1;

    return result;
}

u8 GetHPBarLevel(s16 hp, s16 maxhp)
{
    s32 currValue, maxValue;

    if (hp == maxhp)
        return HP_BAR_FULL;

    if (B_HPBAR_COLOR_THRESHOLD < GEN_5)
    {
        currValue = GetScaledHPFraction(hp, maxhp, B_HEALTHBAR_PIXELS);
        maxValue = B_HEALTHBAR_PIXELS;
    }
    else
    {
        currValue = hp;
        maxValue = maxhp;
    }

    if (currValue > (maxValue * 50 / 100)) // more than 50% hp
        return HP_BAR_GREEN;
    else if (currValue > (maxValue * 20 / 100)) // more than 20% hp
        return HP_BAR_YELLOW;
    else if (currValue > 0)
        return HP_BAR_RED; // 20% or less

    return HP_BAR_EMPTY;
}

#define ABILITY_POP_UP_POS_X_DIFF  64
#define ABILITY_POP_UP_POS_X_SLIDE 128
#define ABILITY_POP_UP_POS_X_SPEED 4

#define ABILITY_POP_UP_WIN_WIDTH   10
#define ABILITY_POP_UP_STR_WIDTH   (ABILITY_POP_UP_WIN_WIDTH * 8)

#define ABILITY_POP_UP_PLAYER_LEFT_WIN_W    6
#define ABILITY_POP_UP_PLAYER_RIGHT_WIN_W   4
#define ABILITY_POP_UP_OPPONENT_LEFT_WIN_W  7
#define ABILITY_POP_UP_OPPONENT_RIGHT_WIN_W 3

#define ABILITY_POP_UP_WAIT_FRAMES 48

#define sState          data[0]
#define sAutoDestroy    data[1]
#define sTimer          data[2]
#define sIsPlayerSide   data[3]
#define sBattlerId      data[4]
#define sIsMain         data[5]

static const union TextColor sBattlerTextColor =
{
    .background = 0,
    .foreground = 7,
    .shadow = 1,
    .accent = 0,
};

static const union TextColor sAbilityTextColor =
{
    .background = 0,
    .foreground = 9,
    .shadow = 1,
    .accent = 0,
};

enum
{
    APU_STATE_SLIDE_IN = 0,
    APU_STATE_IDLE,
    APU_STATE_SLIDE_OUT,
    APU_STATE_END
};

enum
{
    TAG_ABILITY_POP_UP = 0xD720, // Only used for the SpritePalette, the rest below is for the SpriteSheets.
    TAG_ABILITY_POP_UP_PLAYER1 = TAG_ABILITY_POP_UP,
    TAG_ABILITY_POP_UP_OPPONENT1,
    TAG_ABILITY_POP_UP_PLAYER2,
    TAG_ABILITY_POP_UP_OPPONENT2,
    TAG_LAST_BALL_WINDOW,
};

static const u32 sAbilityPopUpGfx[] = INCGFX_U32("graphics/battle_interface_fantasy_retold/ability_pop_up.png", ".4bpp", "-mwidth 8 -mheight 4");
static const u16 sAbilityPopUpPalette[] = INCGFX_U16("graphics/battle_interface_fantasy_retold/ability_pop_up.pal", ".gbapal");

static const struct SpriteSheet sSpriteSheet_AbilityPopUp =
{
    sAbilityPopUpGfx, sizeof(sAbilityPopUpGfx), TAG_ABILITY_POP_UP
};

static const struct SpritePalette sSpritePalette_AbilityPopUp =
{
    sAbilityPopUpPalette, TAG_ABILITY_POP_UP
};

static const struct OamData sOamData_AbilityPopUp =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x32),
    .size = SPRITE_SIZE(64x32),
    .priority = 0,
};

static const struct SpriteTemplate sSpriteTemplate_AbilityPopUp =
{
    .tileTag = TAG_NONE, // Changed on the fly.
    .paletteTag = TAG_ABILITY_POP_UP,
    .oam = &sOamData_AbilityPopUp,
    .callback = SpriteCb_AbilityPopUp
};

static const s16 sAbilityPopUpCoordsDoubles[MAX_BATTLERS_COUNT][2] =
{
    { 24, 80}, // Player left
    {178, 19}, // Opponent left
    { 24, 97}, // Player right
    {178, 36}, // Opponent right
};

static const s16 sAbilityPopUpCoordsSingles[MAX_BATTLERS_COUNT][2] =
{
    { 24, 97}, // Player
    {178, 57}, // Opponent
};

static void PrintOnAbilityPopUp(const u8 *str, u32 spriteId1, u32 spriteId2, u32 x, u32 y, bool32 isName)
{
    s16 data1[8];
    s16 data2[8];
    for (u32 i = 0; i < 8; i++)
    {
        data1[i] = gSprites[spriteId1].data[i];
        data2[i] = gSprites[spriteId2].data[i];
    }

    u32 font = GetFontIdToFit(str, FONT_SMALL, 0, ABILITY_POP_UP_STR_WIDTH);

    u8 ids[2] = {spriteId1, spriteId2};
    const u32 *spriteSrcs[2] = {&sAbilityPopUpGfx[0], &sAbilityPopUpGfx[256]};
    SetupSpritesForTextPrinting(ids, spriteSrcs, 2, 1);

    if (isName)
    {
        AddSpriteTextPrinterParameterized6(spriteId1, font, x, y, 0, 0, sBattlerTextColor, TEXT_SKIP_DRAW, str);
    }
    else
    {
        FillSpriteRectSprite(spriteId1, x, y, ABILITY_POP_UP_STR_WIDTH, 16);
        AddSpriteTextPrinterParameterized6(spriteId1, font, x, y, 0, 0, sAbilityTextColor, TEXT_SKIP_DRAW, str);
    }

    for (u32 i = 0; i < 8; i++)
    {
        gSprites[spriteId1].data[i] = data1[i];
        gSprites[spriteId2].data[i] = data2[i];
    }
}

static void PrintBattlerOnAbilityPopUp(enum BattlerId battler, u8 spriteId1, u8 spriteId2)
{
    u32 totalChar = 0, lastChar;
    struct Pokemon *illusionMon = GetIllusionMonPtr(battler);

    if (illusionMon != NULL)
        GetMonData(illusionMon, MON_DATA_NICKNAME, gStringVar1);
    else
        GetMonData(GetBattlerMon(battler), MON_DATA_NICKNAME, gStringVar1);

    while (gStringVar1[totalChar] != EOS)
        totalChar++;

    lastChar = gStringVar1[totalChar - 1];
    StringAppend(gStringVar1, COMPOUND_STRING("'"));
    if (lastChar != CHAR_S && lastChar != CHAR_s)
        StringAppend(gStringVar1, COMPOUND_STRING("s"));

    u32 x;
    u32 y = 0;
    if (IsOnPlayerSide(battler))
        x = 16;
    else
        x = 8;

    PrintOnAbilityPopUp(gStringVar1, spriteId1, spriteId2, x, y, TRUE);
}

static void PrintAbilityOnAbilityPopUp(enum Ability ability, u8 spriteId1, u8 spriteId2)
{
    u32 x;
    u32 y = 12;
    if (IsOnPlayerSide(gSprites[spriteId1].sBattlerId))
        x = 16;
    else
        x = 8;

    PrintOnAbilityPopUp(gAbilitiesInfo[ability].name, spriteId1, spriteId2, x, y, FALSE);
}

static void PrintItemOnItemPopUp(enum Item item, u8 spriteId1, u8 spriteId2)
{
    u32 x;
    u32 y = 12;
    if (IsOnPlayerSide(gSprites[spriteId1].sBattlerId))
        x = 16;
    else
        x = 8;

    PrintOnAbilityPopUp(GetItemName(item), spriteId1, spriteId2, x, y, FALSE);
}

bool32 IsAnyAbilityPopUpActive(void)
{
    u32 activeAbilityPopUps = 0;
    for (enum BattlerId battler = 0; battler < gBattlersCount; battler++)
    {
        if (gBattleStruct->battlerState[battler].activeAbilityPopUps)
            activeAbilityPopUps++;
    }

    return activeAbilityPopUps;
}

void CreateAbilityPopUp(enum BattlerId battler, enum Ability ability, bool32 isDoubleBattle)
{
    u8 *spriteIds;
    u32 xSlide, tileTag;
    enum BattlerPosition battlerPosition = GetBattlerPosition(battler);
    struct SpriteTemplate template;
    const s16 (*coords)[2];

    if (gBattleScripting.abilityPopupOverwrite)
        ability = gBattleScripting.abilityPopupOverwrite;

    if (gTestRunnerEnabled)
    {
        TestRunner_Battle_RecordAbilityPopUp(battler, ability);
        if (gTestRunnerHeadless)
            return;
    }

    if (!IsAnyAbilityPopUpActive())
        LoadSpritePalette(&sSpritePalette_AbilityPopUp);

    tileTag = (TAG_ABILITY_POP_UP_PLAYER1 + battler);
    if (IndexOfSpriteTileTag(tileTag) == 0xFF)
    {
        struct SpriteSheet sheet = sSpriteSheet_AbilityPopUp;
        sheet.tag = tileTag;
        LoadSpriteSheet(&sheet);
    }

    coords = isDoubleBattle ? sAbilityPopUpCoordsDoubles : sAbilityPopUpCoordsSingles;
    xSlide = IsOnPlayerSide(battler) ? -ABILITY_POP_UP_POS_X_SLIDE : ABILITY_POP_UP_POS_X_SLIDE;

    template = sSpriteTemplate_AbilityPopUp;
    template.tileTag = tileTag;
    spriteIds = gBattleStruct->abilityPopUpSpriteIds[battler];
    spriteIds[0] = CreateSprite(&template, coords[battlerPosition][0] + xSlide,
                                           coords[battlerPosition][1], 0);
    spriteIds[1] = CreateSprite(&template, coords[battlerPosition][0] + xSlide + ABILITY_POP_UP_POS_X_DIFF,
                                           coords[battlerPosition][1], 0);

    if (IsOnPlayerSide(battler))
    {
        gSprites[spriteIds[0]].sIsPlayerSide = TRUE;
        gSprites[spriteIds[1]].sIsPlayerSide = TRUE;
    }

    gSprites[spriteIds[1]].oam.tileNum += 32; // Second half of the pop up tiles.

    // Create only one instance, as it's only used for
    // tracking the SpriteSheet(s) and SpritePalette.
    if (!IsAnyAbilityPopUpActive())
        CreateTask(Task_FreeAbilityPopUpGfx, 5);

    gBattleStruct->battlerState[battler].activeAbilityPopUps = TRUE;

    gSprites[spriteIds[0]].sIsMain = TRUE;
    gSprites[spriteIds[0]].sBattlerId = battler;
    gSprites[spriteIds[1]].sBattlerId = battler;

    PrintBattlerOnAbilityPopUp(battler, spriteIds[0], spriteIds[1]);
    PrintAbilityOnAbilityPopUp(ability, spriteIds[0], spriteIds[1]);
}

void UpdateAbilityPopup(enum BattlerId battler)
{
    u8 *spriteIds = gBattleStruct->abilityPopUpSpriteIds[battler];
    enum Ability ability = (gBattleScripting.abilityPopupOverwrite) ? gBattleScripting.abilityPopupOverwrite
                                                           : gBattleMons[battler].ability;
    PrintAbilityOnAbilityPopUp(ability, spriteIds[0], spriteIds[1]);
}

static void SpriteCb_AbilityPopUp(struct Sprite *sprite)
{
    s16 *data = sprite->data;
    enum BattlerPosition battlerPosition = GetBattlerPosition(sBattlerId);
    u32 fullX = sprite->x + sprite->x2;
    u32 speed;

    switch (sState)
    {
    case APU_STATE_SLIDE_IN:
    {
        const s16 (*coords)[2] = IsDoubleBattle() ? sAbilityPopUpCoordsDoubles : sAbilityPopUpCoordsSingles;
        u32 xCoord = coords[battlerPosition][0];

        if (sIsMain && ++sTimer == 4)
            PlaySE(SE_BALL_TRAY_ENTER);

        if (!sIsMain)
            xCoord += ABILITY_POP_UP_POS_X_DIFF;

        if (fullX == xCoord)
        {
            sTimer = ABILITY_POP_UP_WAIT_FRAMES;
            sState = APU_STATE_IDLE;
            break;
        }

        speed = sIsPlayerSide ? ABILITY_POP_UP_POS_X_SPEED : -ABILITY_POP_UP_POS_X_SPEED;
        sprite->x2 += speed;
        break;
    }
    case APU_STATE_IDLE:
    {
        if (!sTimer || sAutoDestroy)
        {
            sState = APU_STATE_SLIDE_OUT;
            break;
        }

        if (!gBattleScripting.fixedPopup)
            sTimer--;

        break;
    }
    case APU_STATE_SLIDE_OUT:
    {
        if (fullX == sprite->x)
    {
            sState = APU_STATE_END;
            break;
    }

        speed = sIsPlayerSide ? -ABILITY_POP_UP_POS_X_SPEED : ABILITY_POP_UP_POS_X_SPEED;
        sprite->x2 += speed;
        break;
    }
    case APU_STATE_END:
    {
        if (sIsMain)
            gBattleStruct->battlerState[sBattlerId].activeAbilityPopUps = FALSE;

        DestroySprite(sprite);
        break;
    }
    }
}

void DestroyAbilityPopUp(enum BattlerId battler)
{
    if (gBattleStruct->battlerState[battler].activeAbilityPopUps)
    {
        gSprites[gBattleStruct->abilityPopUpSpriteIds[battler][0]].sAutoDestroy = TRUE;
        gSprites[gBattleStruct->abilityPopUpSpriteIds[battler][1]].sAutoDestroy = TRUE;
    }
}

static void Task_FreeAbilityPopUpGfx(u8 taskId)
{
    if (!IsAnyAbilityPopUpActive())
    {
        for (enum BattlerId battler = 0; battler < gBattlersCount; battler++)
        {
            if (IndexOfSpriteTileTag(TAG_ABILITY_POP_UP_PLAYER1 + battler) != 0xFF)
                FreeSpriteTilesByTag(TAG_ABILITY_POP_UP_PLAYER1 + battler);
        }
        FreeSpritePaletteByTag(TAG_ABILITY_POP_UP);
        DestroyTask(taskId);
    }
}

void CreateItemPopUp(enum BattlerId battler)
{
    u8 *spriteIds;
    u32 xSlide, tileTag;
    enum BattlerPosition battlerPosition = GetBattlerPosition(battler);
    struct SpriteTemplate template;
    const s16 (*coords)[2];

    if (gTestRunnerEnabled)
    {
        TestRunner_Battle_RecordItemPopUp(battler, gLastUsedItem);
        if (gTestRunnerHeadless)
            return;
    }

    if (!IsAnyAbilityPopUpActive())
        LoadSpritePalette(&sSpritePalette_AbilityPopUp);

    tileTag = (TAG_ABILITY_POP_UP_PLAYER1 + battler);
    if (IndexOfSpriteTileTag(tileTag) == 0xFF)
    {
        struct SpriteSheet sheet = sSpriteSheet_AbilityPopUp;
        sheet.tag = tileTag;
        LoadSpriteSheet(&sheet);
    }

    coords = IsDoubleBattle() ? sAbilityPopUpCoordsDoubles : sAbilityPopUpCoordsSingles;
    xSlide = IsOnPlayerSide(battler) ? -ABILITY_POP_UP_POS_X_SLIDE : ABILITY_POP_UP_POS_X_SLIDE;

    template = sSpriteTemplate_AbilityPopUp;
    template.tileTag = tileTag;
    spriteIds = gBattleStruct->abilityPopUpSpriteIds[battler];
    spriteIds[0] = CreateSprite(&template, coords[battlerPosition][0] + xSlide,
                                           coords[battlerPosition][1], 0);
    spriteIds[1] = CreateSprite(&template, coords[battlerPosition][0] + xSlide + ABILITY_POP_UP_POS_X_DIFF,
                                           coords[battlerPosition][1], 0);

    if (IsOnPlayerSide(battler))
    {
        gSprites[spriteIds[0]].sIsPlayerSide = TRUE;
        gSprites[spriteIds[1]].sIsPlayerSide = TRUE;
    }

    gSprites[spriteIds[1]].oam.tileNum += 32; // Second half of the pop up tiles.

    // Create only one instance, as it's only used for
    // tracking the SpriteSheet(s) and SpritePalette.
    if (!IsAnyAbilityPopUpActive())
        CreateTask(Task_FreeAbilityPopUpGfx, 5);

    gBattleStruct->battlerState[battler].activeAbilityPopUps = TRUE;

    gSprites[spriteIds[0]].sIsMain = TRUE;
    gSprites[spriteIds[0]].sBattlerId = battler;
    gSprites[spriteIds[1]].sBattlerId = battler;

    PrintBattlerOnAbilityPopUp(battler, spriteIds[0], spriteIds[1]);
    PrintItemOnItemPopUp(gLastUsedItem, spriteIds[0], spriteIds[1]);
}

void FreeAbilityPopUpGfx(void)
{
    for (u32 i = 0; i < NUM_TASKS; i++)
    {
        if (gTasks[i].isActive)
        {
            if (gTasks[i].func == Task_FreeAbilityPopUpGfx)
                DestroyTask(i);
        }
    }
}

#undef sState
#undef sAutoDestroy
#undef sTimer
#undef sIsPlayerSide
#undef sBattlerId
#undef sIsMain

// last used ball

static const struct OamData sOamData_LastUsedBall =
{
    .y = 0,
    .affineMode = 0,
    .objMode = 0,
    .mosaic = 0,
    .bpp = 0,
    .shape = (B_LAST_USED_BALL_CYCLE == TRUE ? SPRITE_SHAPE(32x64) : SPRITE_SHAPE(32x32)),
    .x = 0,
    .matrixNum = 0,
    .size = (B_LAST_USED_BALL_CYCLE == TRUE ? SPRITE_SIZE(32x64) : SPRITE_SIZE(32x32)),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};

static const struct SpriteTemplate sSpriteTemplate_LastUsedBallWindow =
{
    .tileTag = TAG_LAST_BALL_WINDOW,
    .paletteTag = TAG_ABILITY_POP_UP,
    .oam = &sOamData_LastUsedBall,
    .callback = SpriteCB_LastUsedBallWin
};

#define MOVE_INFO_WINDOW_TAG 0xE722

static const struct OamData sOamData_MoveInfoWindow =
{
    .y = 0,
    .affineMode = 0,
    .objMode = 0,
    .mosaic = 0,
    .bpp = 0,
    .shape = SPRITE_SHAPE(32x32),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};

static const struct SpriteTemplate sSpriteTemplate_MoveInfoWindow =
{
    .tileTag = MOVE_INFO_WINDOW_TAG,
    .paletteTag = TAG_ABILITY_POP_UP,
    .oam = &sOamData_MoveInfoWindow,
    .callback = SpriteCB_MoveInfoWin
};

#if B_LAST_USED_BALL_BUTTON == R_BUTTON && B_LAST_USED_BALL_CYCLE == TRUE
    static const u8 ALIGNED(4) sLastUsedBallWindowGfx[] = INCGFX_U8("graphics/battle_interface_fantasy_retold/last_used_ball_r_cycle.png", ".4bpp");
#elif B_LAST_USED_BALL_CYCLE == TRUE
    static const u8 ALIGNED(4) sLastUsedBallWindowGfx[] = INCGFX_U8("graphics/battle_interface_fantasy_retold/last_used_ball_l_cycle.png", ".4bpp");
#elif B_LAST_USED_BALL_BUTTON == R_BUTTON
    static const u8 ALIGNED(4) sLastUsedBallWindowGfx[] = INCGFX_U8("graphics/battle_interface_fantasy_retold/last_used_ball_r.png", ".4bpp");
#else
    static const u8 ALIGNED(4) sLastUsedBallWindowGfx[] = INCGFX_U8("graphics/battle_interface_fantasy_retold/last_used_ball_l.png", ".4bpp");
#endif
static const struct SpriteSheet sSpriteSheet_LastUsedBallWindow =
{
    sLastUsedBallWindowGfx, sizeof(sLastUsedBallWindowGfx), TAG_LAST_BALL_WINDOW
};

#if B_MOVE_DESCRIPTION_BUTTON == R_BUTTON
static const u8 sMoveInfoWindowGfx[] = INCGFX_U8("graphics/battle_interface_fantasy_retold/move_info_window_r.png", ".4bpp");
#else
static const u8 sMoveInfoWindowGfx[] = INCGFX_U8("graphics/battle_interface_fantasy_retold/move_info_window_l.png", ".4bpp");
#endif

static const struct SpriteSheet sSpriteSheet_MoveInfoWindow =
{
    sMoveInfoWindowGfx, sizeof(sMoveInfoWindowGfx), MOVE_INFO_WINDOW_TAG
};

#define LAST_USED_BALL_X_F    14
#define LAST_USED_BALL_X_0    -14
#define LAST_USED_BALL_Y      (IsDoubleBattle() ? 78 : 68)
#define LAST_USED_BALL_Y_BNC  (IsDoubleBattle() ? 76 : 66)

#define LAST_BALL_WIN_X_F       (LAST_USED_BALL_X_F - 0)
#define LAST_BALL_WIN_X_0       (LAST_USED_BALL_X_0 - 0)
#define LAST_USED_WIN_Y         (LAST_USED_BALL_Y - 8)

#define sHide  data[0]
#define sTimer  data[1]
#define sMoving data[2]
#define sBounce data[3] // 0 = Bounce down; 1 = Bounce up

#define sState     data[0]
#define sSameBall  data[1]

bool32 CanThrowLastUsedBall(void)
{
    if (B_LAST_USED_BALL == FALSE)
        return FALSE;
    if (!CanThrowBall())
        return FALSE;
    if (gBattleTypeFlags & (BATTLE_TYPE_TRAINER | BATTLE_TYPE_FRONTIER))
        return FALSE;
    if (!CheckBagHasItem(gBallToDisplay, 1))
        return FALSE;
    if (IsVictoryCatch())
        return FALSE;
    return TRUE;
}

void TryAddLastUsedBallItemSprites(void)
{
    if (B_LAST_USED_BALL == FALSE)
        return;
    if (gLastThrownBall == 0
      || (gLastThrownBall != 0 && !CheckBagHasItem(gLastThrownBall, 1)))
    {
        // we're out of the last used ball, so just set it to the first ball in the bag
        u16 firstBall;

        // we have to compact the bag first bc it is typically only compacted when you open it
        CompactItemsInBagPocket(POCKET_POKE_BALLS);

        firstBall = GetBagItemId(POCKET_POKE_BALLS, 0);
        if (firstBall > ITEM_NONE)
            gBallToDisplay = firstBall;
    }

    if (!CanThrowLastUsedBall())
        return;

    // ball
    if (gBattleStruct->ballSpriteIds[0] == MAX_SPRITES)
    {
        gBattleStruct->ballSpriteIds[0] = AddItemIconSprite(102, 102, gBallToDisplay);
        if (gBattleStruct->ballSpriteIds[0] != MAX_SPRITES)
        {
            gSprites[gBattleStruct->ballSpriteIds[0]].x = LAST_USED_BALL_X_0;
            gSprites[gBattleStruct->ballSpriteIds[0]].y = LAST_USED_BALL_Y;
            gSprites[gBattleStruct->ballSpriteIds[0]].sHide = FALSE;
            gLastUsedBallMenuPresent = TRUE;
            gSprites[gBattleStruct->ballSpriteIds[0]].callback = SpriteCB_LastUsedBall;
        }
    }

    // window
    LoadSpritePalette(&sSpritePalette_AbilityPopUp);
    if (GetSpriteTileStartByTag(TAG_LAST_BALL_WINDOW) == 0xFFFF)
        LoadSpriteSheet(&sSpriteSheet_LastUsedBallWindow);

    if (gBattleStruct->ballSpriteIds[1] == MAX_SPRITES)
    {
        gBattleStruct->ballSpriteIds[1] = CreateSprite(&sSpriteTemplate_LastUsedBallWindow,
                                                       LAST_BALL_WIN_X_0,
                                                       LAST_USED_WIN_Y, 5);
        gSprites[gBattleStruct->ballSpriteIds[1]].sHide = FALSE;
        gSprites[gBattleStruct->moveInfoSpriteId].sHide = TRUE;
        gLastUsedBallMenuPresent = TRUE;
    }
    if (B_LAST_USED_BALL_CYCLE == TRUE)
        ArrowsChangeColorLastBallCycle(0); //Default the arrows to be invisible
}

static void DestroyLastUsedBallWinGfx(struct Sprite *sprite)
{
    FreeSpriteTilesByTag(TAG_LAST_BALL_WINDOW);
    if (GetSpriteTileStartByTag(MOVE_INFO_WINDOW_TAG) == 0xFFFF)
        FreeSpritePaletteByTag(TAG_ABILITY_POP_UP);
    DestroySprite(sprite);
    gBattleStruct->ballSpriteIds[1] = MAX_SPRITES;
}

static void DestroyLastUsedBallGfx(struct Sprite *sprite)
{
    FreeSpriteTilesByTag(102);
    FreeSpritePaletteByTag(102);
    DestroySprite(sprite);
    gBattleStruct->ballSpriteIds[0] = MAX_SPRITES;
}

void TryToAddMoveInfoWindow(void)
{
    if (!B_SHOW_MOVE_DESCRIPTION)
        return;

    if (B_MOVE_DESCRIPTION_BUTTON == L_BUTTON && gSaveBlock2Ptr->optionsButtonMode == OPTIONS_BUTTON_MODE_L_EQUALS_A)
        return;

    LoadSpritePalette(&sSpritePalette_AbilityPopUp);
    if (GetSpriteTileStartByTag(MOVE_INFO_WINDOW_TAG) == 0xFFFF)
        LoadSpriteSheet(&sSpriteSheet_MoveInfoWindow);

    if (gBattleStruct->moveInfoSpriteId == MAX_SPRITES)
    {
        gBattleStruct->moveInfoSpriteId = CreateSprite(&sSpriteTemplate_MoveInfoWindow, LAST_BALL_WIN_X_0, LAST_USED_WIN_Y + 32, 6);
        gSprites[gBattleStruct->moveInfoSpriteId].sHide = FALSE;
    }
}

void TryToHideMoveInfoWindow(void)
{
    gSprites[gBattleStruct->moveInfoSpriteId].sHide = TRUE;
}

static void DestroyMoveInfoWinGfx(struct Sprite *sprite)
{
    FreeSpriteTilesByTag(MOVE_INFO_WINDOW_TAG);
    if (GetSpriteTileStartByTag(TAG_LAST_BALL_WINDOW) == 0xFFFF)
        FreeSpritePaletteByTag(TAG_ABILITY_POP_UP);
    DestroySprite(sprite);
    gBattleStruct->moveInfoSpriteId = MAX_SPRITES;
}

static void SpriteCB_LastUsedBallWin(struct Sprite *sprite)
{
    if (sprite->sHide)
    {
        if (sprite->x != LAST_BALL_WIN_X_0)
            sprite->x--;

        if (sprite->x == LAST_BALL_WIN_X_0)
            DestroyLastUsedBallWinGfx(sprite);
    }
    else
    {
        if (sprite->x != LAST_BALL_WIN_X_F)
            sprite->x++;
    }
}

static void SpriteCB_LastUsedBall(struct Sprite *sprite)
{
    if (sprite->sHide)
    {
        if (sprite->y < LAST_USED_BALL_Y) // Used to recover from an incomplete bounce before hiding the window
            sprite->y++;

        if (sprite->x != LAST_USED_BALL_X_0)
            sprite->x--;

        if (sprite->x == LAST_USED_BALL_X_0)
            DestroyLastUsedBallGfx(sprite);
    }
    else
    {
        if (sprite->x != LAST_USED_BALL_X_F)
            sprite->x++;
    }
}

static void SpriteCB_MoveInfoWin(struct Sprite *sprite)
{
    if (sprite->sHide)
    {
        if (sprite->x != LAST_BALL_WIN_X_0)
            sprite->x--;

        if (sprite->x == LAST_BALL_WIN_X_0)
            DestroyMoveInfoWinGfx(sprite);
    }
    else
    {
        if (sprite->x != LAST_BALL_WIN_X_F)
            sprite->x++;
    }
}

static void TryHideOrRestoreLastUsedBall(u8 caseId)
{
    if (B_LAST_USED_BALL == FALSE)
        return;
    if (gBattleStruct->ballSpriteIds[0] == MAX_SPRITES)
        return;

    switch (caseId)
    {
    case 0: // hide
        if (gBattleStruct->ballSpriteIds[0] != MAX_SPRITES)
            gSprites[gBattleStruct->ballSpriteIds[0]].sHide = TRUE;
        if (gBattleStruct->ballSpriteIds[1] != MAX_SPRITES)
            gSprites[gBattleStruct->ballSpriteIds[1]].sHide = TRUE;
        gLastUsedBallMenuPresent = FALSE;
        break;
    case 1: // restore
        if (gBattleStruct->ballSpriteIds[0] != MAX_SPRITES)
            gSprites[gBattleStruct->ballSpriteIds[0]].sHide = FALSE;
        if (gBattleStruct->ballSpriteIds[1] != MAX_SPRITES)
            gSprites[gBattleStruct->ballSpriteIds[1]].sHide = FALSE;
        gLastUsedBallMenuPresent = TRUE;
        break;
    }
    if (B_LAST_USED_BALL_CYCLE == TRUE)
        ArrowsChangeColorLastBallCycle(0); //Default the arrows to be invisible
}

void TryHideLastUsedBall(void)
{
    if (B_LAST_USED_BALL_BUTTON == L_BUTTON && gSaveBlock2Ptr->optionsButtonMode == OPTIONS_BUTTON_MODE_L_EQUALS_A)
        return;

    if (B_LAST_USED_BALL == TRUE)
        TryHideOrRestoreLastUsedBall(0);
}

void TryRestoreLastUsedBall(void)
{
    if (B_LAST_USED_BALL == FALSE)
        return;

    if (B_LAST_USED_BALL_BUTTON == L_BUTTON && gSaveBlock2Ptr->optionsButtonMode == OPTIONS_BUTTON_MODE_L_EQUALS_A)
        return;

    if (gBattleStruct->ballSpriteIds[0] != MAX_SPRITES)
        TryHideOrRestoreLastUsedBall(1);
    else
        TryAddLastUsedBallItemSprites();
}

static void SpriteCB_LastUsedBallBounce(struct Sprite *sprite)
{
    if ((sprite->sTimer++ % 4) != 0)  // Change the image every 4 frame
        return;
    if (sprite->sBounce)
    {
        if (sprite->y > LAST_USED_BALL_Y_BNC)
            sprite->y--;
        else
            sprite->sMoving = FALSE;
    }
    else
    {
        if (sprite->y < LAST_USED_BALL_Y)
            sprite->y++;
        else
            sprite->sMoving = FALSE;
    }
}

static void Task_BounceBall(u8 taskId)
{
    struct Sprite *sprite = &gSprites[gBattleStruct->ballSpriteIds[0]];
    struct Task *task = &gTasks[taskId];
    switch (task->sState)
    {
    case 0:  // Bounce up
        sprite->sBounce = TRUE;
        sprite->sMoving = TRUE;
        sprite->callback = SpriteCB_LastUsedBallBounce;
        if (task->sSameBall)
            task->sState = 3;
        else
            task->sState = 1;
        break;
    case 1:  // Destroy Icon
        if (!sprite->sMoving)
        {
            DestroyLastUsedBallGfx(sprite);
            task->sState++;
        }  // Passthrough
    case 2: //Create New Icon
        if (!sprite->inUse)
        {
            gBattleStruct->ballSpriteIds[0] = AddItemIconSprite(102, 102, gBallToDisplay);
            if (gBattleStruct->ballSpriteIds[0] != MAX_SPRITES)
            {
                gSprites[gBattleStruct->ballSpriteIds[0]].x = LAST_USED_BALL_X_F;
                gSprites[gBattleStruct->ballSpriteIds[0]].y = LAST_USED_BALL_Y_BNC;
            }
            task->sState++;
        }  // Fallthrough
    case 3: // Bounce Down
        if (!sprite->sMoving)
        {
            sprite->sBounce = FALSE;
            sprite->sMoving = TRUE;
            sprite->callback = SpriteCB_LastUsedBallBounce; //Show and bounce down
            task->sState++;
        }
        break;
    case 4:  // Destroy Task
        if (!sprite->sMoving)
        {
            sprite->callback = SpriteCB_LastUsedBall;
            DestroyTask(taskId);
        }
    }
    if (!gLastUsedBallMenuPresent)
    {
        // Used to check if the R button was released before the animation was complete
        sprite->callback = SpriteCB_LastUsedBall;
        DestroyTask(taskId);
    }
}

void SwapBallToDisplay(bool32 sameBall)
{
    u8 taskId;
    taskId = CreateTask(Task_BounceBall, 10);
    gTasks[taskId].sSameBall = sameBall;
}

void ArrowsChangeColorLastBallCycle(bool32 showArrows)
{
#if B_LAST_USED_BALL == TRUE && B_LAST_USED_BALL_CYCLE == TRUE
    u16 paletteNum = 16 + gSprites[gBattleStruct->ballSpriteIds[1]].oam.paletteNum;
    struct PlttData *defaultPlttArrow;
    struct PlttData *defaultPlttOutline;
    struct PlttData *pltArrow;
    struct PlttData *pltOutline;
    if (gBattleStruct->ballSpriteIds[1] == MAX_SPRITES)
        return;
    paletteNum *= 16;
    pltArrow = (struct PlttData *)&gPlttBufferFaded[paletteNum + 9];  // Arrow color is in idx 9
    pltOutline = (struct PlttData *)&gPlttBufferFaded[paletteNum + 8];  // Arrow outline is in idx 8
    if (!showArrows) //Make invisible
    {
        defaultPlttArrow = (struct PlttData *)&gPlttBufferFaded[paletteNum + 13];  // Background color is idx 13
        pltArrow->r = defaultPlttArrow->r;
        pltArrow->g = defaultPlttArrow->g;
        pltArrow->b = defaultPlttArrow->b;
        pltOutline->r = defaultPlttArrow->r;
        pltOutline->g = defaultPlttArrow->g;
        pltOutline->b = defaultPlttArrow->b;
    }
    else // Make gray
    {
        defaultPlttArrow = (struct PlttData *)&gPlttBufferFaded[paletteNum + 11];  // Grey color is idx 11
        defaultPlttOutline = (struct PlttData *)&gPlttBufferFaded[paletteNum + 10];  //Light grey color for outline is idx 10
        pltArrow->r = defaultPlttArrow->r;
        pltArrow->g = defaultPlttArrow->g;
        pltArrow->b = defaultPlttArrow->b;
        pltOutline->r = defaultPlttOutline->r;
        pltOutline->g = defaultPlttOutline->g;
        pltOutline->b = defaultPlttOutline->b;
    }
#endif
}

void CategoryIcons_LoadSpritesGfx(void)
{
    LoadCompressedSpriteSheet(&gSpriteSheet_CategoryIcons);
    LoadSpritePalette(&gSpritePal_CategoryIcons);
}
