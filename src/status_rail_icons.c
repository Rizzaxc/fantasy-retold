#include "global.h"
#include "battle.h"
#include "pokemon.h"
#include "status_rail.h"
#include "status_rail_icons.h"

// Fantasy Retold: status-line icons (see status_rail_icons.h). Moved here from battle_interface.c so the party
// screen and the summary draw the same cells as the healthbox.

struct StatusIconLineInfo
{
    u8 rail;     // enum StatusRail
    u8 color;
    u8 maxStage; // pips drawn in the cell
    u8 glyph[7]; // 7x7 icon, one row per byte, bit 6 = leftmost pixel
};

static const struct StatusIconLineInfo sStatusIconLines[STATUS_ICON_LINE_COUNT] =
{
    [STATUS_ICON_SOAK]      = {RAIL_SOAK,      HB_COLOR_SKY,    1, {0x08, 0x1C, 0x1C, 0x36, 0x2E, 0x3E, 0x1C}}, // drop
    [STATUS_ICON_CHILL]     = {RAIL_CHILL,     HB_COLOR_CYAN,   2, {0x08, 0x2A, 0x1C, 0x7F, 0x1C, 0x2A, 0x08}}, // snowflake
    [STATUS_ICON_BURN]      = {RAIL_BURN,      HB_COLOR_ORANGE, 3, {0x08, 0x18, 0x1C, 0x3E, 0x36, 0x22, 0x1C}}, // flame
    [STATUS_ICON_POISON]    = {RAIL_POISON,    HB_COLOR_PURPLE, 2, {0x3E, 0x7F, 0x49, 0x7F, 0x36, 0x1C, 0x14}}, // skull
    [STATUS_ICON_ELECTRIC]  = {RAIL_ELECTRIC,  HB_COLOR_YELLOW, 3, {0x06, 0x0C, 0x1E, 0x3C, 0x0C, 0x18, 0x30}}, // bolt
    [STATUS_ICON_SLEEP]     = {RAIL_SLEEP,     HB_COLOR_BLUE,   3, {0x7F, 0x06, 0x0C, 0x18, 0x30, 0x7F, 0x00}}, // Z
    [STATUS_ICON_CONFUSION] = {RAIL_CONFUSION, HB_COLOR_PINK,   3, {0x3E, 0x41, 0x5D, 0x55, 0x51, 0x5E, 0x40}}, // swirl
};

u32 GetStatusIconStages(const struct StatusRailEntry *rails, u8 stages[STATUS_ICON_LINE_COUNT])
{
    u32 i, signature = 0;

    for (i = 0; i < STATUS_ICON_LINE_COUNT; i++)
    {
        stages[i] = rails[sStatusIconLines[i].rail].level;
        signature |= min(stages[i], 3) << (2 * i);
    }
    return signature;
}

u32 GetMonStatusIconStages(struct Pokemon *mon, u8 stages[STATUS_ICON_LINE_COUNT])
{
    struct StatusRailEntry rails[NUM_STATUS_RAILS];
    struct StatusRailEntry *live = GetPartyMonStatusRails(mon);

    if (live != NULL)
        return GetStatusIconStages(live, stages);
    UnpackStatusRails(GetMonData(mon, MON_DATA_HP) != 0 ? GetMonData(mon, MON_DATA_STATUS) : 0, rails);
    return GetStatusIconStages(rails, stages);
}

u32 GetFirstStatusIconLine(const u8 stages[STATUS_ICON_LINE_COUNT])
{
    u32 i;

    for (i = 0; i < STATUS_ICON_LINE_COUNT; i++)
    {
        if (stages[i] != 0)
            return i;
    }
    return STATUS_ICON_LINE_COUNT;
}

void FillStatusIconRect(u32 *tiles, u32 widthTiles, u32 x0, u32 y0, u32 x1, u32 y1, u32 color)
{
    u32 x, y;

    for (y = y0; y <= y1; y++)
    {
        for (x = x0; x <= x1; x++)
        {
            u32 *word = &tiles[(((y / 8) * widthTiles) + (x / 8)) * 8 + (y % 8)];
            u32 shift = (x % 8) * 4;
            *word = (*word & ~(0xF << shift)) | (color << shift);
        }
    }
}

void DrawStatusIconCell(u32 *tiles, u32 widthTiles, u32 cx, u32 cy, enum StatusIconLine line, u32 stage)
{
    const struct StatusIconLineInfo *info = &sStatusIconLines[line];
    u32 gx, gy, pip;

    FillStatusIconRect(tiles, widthTiles, cx, cy, cx + STATUS_ICON_CELL_W - 1, cy + STATUS_ICON_CELL_H - 1, HB_COLOR_EMPTY);
    for (gy = 0; gy < 7; gy++)
    {
        for (gx = 0; gx < 7; gx++)
        {
            if (info->glyph[gy] & (0x40 >> gx))
                FillStatusIconRect(tiles, widthTiles, cx + 1 + gx, cy + 1 + gy, cx + 1 + gx, cy + 1 + gy, info->color);
        }
    }
    for (pip = 0; pip < info->maxStage; pip++)
    {
        u32 py = cy + STATUS_ICON_CELL_H - 3 - pip * 3;
        FillStatusIconRect(tiles, widthTiles, cx + 9, py, cx + 10, py + 1, pip < stage ? info->color : HB_COLOR_SLATE);
    }
}

u32 DrawStatusIconBox(u32 *tiles, u32 widthTiles, u32 heightTiles, u32 h, const u8 stages[STATUS_ICON_LINE_COUNT], enum StatusIconBoxAlign align)
{
    u32 i, r, count = 0, active;
    u32 rows = 2, cols, w, x0, top;
    u32 spriteW = widthTiles * 8;

    CpuFill32(0, tiles, widthTiles * heightTiles * TILE_SIZE_4BPP);

    for (i = 0; i < STATUS_ICON_LINE_COUNT; i++)
    {
        if (stages[i] != 0)
            count++;
    }
    if (count == 0)
        return 0;
    active = count;

    cols = (count + rows - 1) / rows;
    w = cols * (STATUS_ICON_CELL_W + 1) + 3;
    if (align == STATUS_ICON_ALIGN_RIGHT)
        x0 = spriteW - w;
    else if (align == STATUS_ICON_ALIGN_CENTER)
        x0 = (spriteW - w) / 2;
    else
        x0 = 0;
    top = (h - rows * (STATUS_ICON_CELL_H + 1) + 1) / 2;

    // Same corner-cut silhouette as the healthbox plate.
    for (r = 0; r < h; r++)
    {
        u32 cut = max(0, max(2 - (s32)r, (s32)r - (s32)(h - 3)));
        FillStatusIconRect(tiles, widthTiles, x0 + cut, r, x0 + w - 1 - cut, r, HB_COLOR_INK);
        if (r != 0 && r != h - 1)
            FillStatusIconRect(tiles, widthTiles, x0 + max(1, cut), r, x0 + w - 2 - (cut > 0 ? cut - 1 : 0), r, HB_COLOR_PLATE);
    }
    FillStatusIconRect(tiles, widthTiles, x0 + 2, 1, x0 + w - 3, 1, HB_COLOR_SLATE);

    for (i = 0, count = 0; i < STATUS_ICON_LINE_COUNT; i++)
    {
        if (stages[i] == 0)
            continue;
        DrawStatusIconCell(tiles, widthTiles,
                           x0 + 2 + (count / rows) * (STATUS_ICON_CELL_W + 1),
                           top + (count % rows) * (STATUS_ICON_CELL_H + 1),
                           i, stages[i]);
        count++;
    }
    return active;
}
