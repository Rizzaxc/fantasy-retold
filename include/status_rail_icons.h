#ifndef GUARD_STATUS_RAIL_ICONS_H
#define GUARD_STATUS_RAIL_ICONS_H

// Fantasy Retold: the status-line icons of the battle healthbox's status column (design.md, "Healthbox
// layout" and "Status Outside Battle"), shared with the party screen and the summary. A cell is a 7x7 icon in
// the line's colour plus one pip per stage the line can reach, lit bottom-up to the current stage (combat.md
// §6). Everything is drawn at runtime into 4bpp sprite tiles (1D mapping) on the healthbox palette
// (gBattleInterface_BallStatusBarPal).

struct Pokemon;
struct StatusRailEntry;

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

// The status lines, in the order their cells fill a box.
enum StatusIconLine
{
    STATUS_ICON_SOAK,
    STATUS_ICON_CHILL,     // Chilled -> Frozen | Frostbitten
    STATUS_ICON_BURN,
    STATUS_ICON_POISON,    // Poison -> Toxic
    STATUS_ICON_ELECTRIC,  // Dazed -> Shocked -> Paralyzed
    STATUS_ICON_SLEEP,     // Drowsy -> Asleep -> Nightmare
    STATUS_ICON_CONFUSION, // Confused -> Stupefied -> Frightened
    STATUS_ICON_LINE_COUNT,
};

#define STATUS_ICON_CELL_W 12
#define STATUS_ICON_CELL_H 9

enum StatusIconBoxAlign
{
    STATUS_ICON_ALIGN_LEFT,
    STATUS_ICON_ALIGN_RIGHT,
    STATUS_ICON_ALIGN_CENTER,
};

// Fills `stages` (0 = none) from a set of rails; returns them packed 2 bits per line (a redraw signature).
u32 GetStatusIconStages(const struct StatusRailEntry *rails, u8 stages[STATUS_ICON_LINE_COUNT]);
// The same for a party mon: its live rails in battle, else its party status.
u32 GetMonStatusIconStages(struct Pokemon *mon, u8 stages[STATUS_ICON_LINE_COUNT]);
// The first active line (the one flag the party screen shows), or STATUS_ICON_LINE_COUNT for none.
u32 GetFirstStatusIconLine(const u8 stages[STATUS_ICON_LINE_COUNT]);

// widthTiles is the sprite's width in tiles; rectangles are inclusive, in sprite pixels.
void FillStatusIconRect(u32 *tiles, u32 widthTiles, u32 x0, u32 y0, u32 x1, u32 y1, u32 color);
void DrawStatusIconCell(u32 *tiles, u32 widthTiles, u32 cx, u32 cy, enum StatusIconLine line, u32 stage);
// The healthbox's status box: a corner-cut plate h pixels tall with one cell per active line, two per
// column. Clears the whole sprite (widthTiles x heightTiles) first; draws nothing when no line is active.
// Returns the number of active lines.
u32 DrawStatusIconBox(u32 *tiles, u32 widthTiles, u32 heightTiles, u32 h, const u8 stages[STATUS_ICON_LINE_COUNT], enum StatusIconBoxAlign align);

#endif // GUARD_STATUS_RAIL_ICONS_H
