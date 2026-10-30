#!/usr/bin/env python3
"""Generate the Fantasy Retold healthbox plates (design.md, "Healthbox layout").

Each box is a dark corner-cut plate drawn at the left of a 128px-wide canvas (two 64px sprites).
Only the static parts are drawn here: plate, HP / Stamina bar frames, the 40% Recuperate tick and
the EXP track. Name, level, HP numbers and every bar fill are drawn at runtime by battle_interface.c,
whose HB_* layout constants must match the ones below.

Run from this folder:  python generate_healthbox_art.py
"""
import os
from PIL import Image

# Healthbox palette (embedded in ball_status_bar.png -> gBattleInterface_BallStatusBarPal).
# Keep in sync with generate_dark_palettes.py / fix_palette_source_pngs.py.
PALETTE = [
    '#FF00FF', '#1A1A2E', '#2D2D44', '#0F0F1A', '#4A4A5E', '#E8E8F0',
    '#8888A0', '#D03040', '#70C040', '#8848A0', '#40B8C8', '#D87830',
    '#6070D0', '#F0D850', '#3890D8', '#E070B0',
]
TRANSPARENT, NAVY, NAVY2, INK, SLATE, WHITE, GREY, RED, GREEN = 0, 1, 2, 3, 4, 5, 6, 7, 8

# Layouts (plate-local == canvas coords; the plate starts at x = 0, y = 0).
PLAYER = dict(w=104, h=32, bar_x=5, bar_w=94, hp_y=13, sta_y=20, exp=None)
PLAYER_EXP = dict(PLAYER, exp=(6, 28, 50))  # (x, y, width) of the 1px EXP track
OPPONENT = dict(w=100, h=25, bar_x=5, bar_w=90, hp_y=13, sta_y=20, exp=None)
RECUPERATE_STAMINA_PERCENT = 40


def new_canvas(h):
    im = Image.new('P', (128, h), TRANSPARENT)
    flat = []
    for c in PALETTE:
        flat += [int(c[1:3], 16), int(c[3:5], 16), int(c[5:7], 16)]
    im.putpalette(flat)
    return im


def rect(im, x0, y0, x1, y1, c):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            im.putpixel((x, y), c)


def plate(im, w, h):
    for r in range(h):
        cut = max(0, 2 - r, r - (h - 3))
        rect(im, cut, r, w - 1 - cut, r, INK)
        if 0 < r < h - 1:
            rect(im, max(1, cut), r, w - 2 - max(0, cut - 1), r, NAVY)
    rect(im, 2, 1, w - 3, 1, SLATE)


def bars(im, L):
    x, w = L['bar_x'], L['bar_w']
    # Frames match the fill; runtime redraws both (the Stamina frame follows its current fill colour).
    rect(im, x - 1, L['hp_y'] - 1, x + w, L['hp_y'] + 4, RED)
    rect(im, x, L['hp_y'], x + w - 1, L['hp_y'] + 3, NAVY2)
    rect(im, x - 1, L['sta_y'] - 1, x + w, L['sta_y'] + 2, GREEN)
    rect(im, x, L['sta_y'], x + w - 1, L['sta_y'] + 1, NAVY2)
    # Recuperate threshold: a white tick through the Stamina frame (also drawn at runtime).
    tick = x + (RECUPERATE_STAMINA_PERCENT * w) // 100
    im.putpixel((tick, L['sta_y'] - 1), WHITE)
    im.putpixel((tick, L['sta_y'] + 2), WHITE)
    if L['exp']:
        ex, ey, ew = L['exp']
        rect(im, ex, ey, ex + ew - 1, ey, NAVY2)


def box(L, canvas_h, with_bars=True):
    im = new_canvas(canvas_h)
    plate(im, L['w'], L['h'])
    if with_bars:
        bars(im, L)
    return im


def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    box(PLAYER_EXP, 64).save('healthbox_singles_player.png')
    box(OPPONENT, 32).save('healthbox_singles_opponent.png')
    box(PLAYER, 32).save('healthbox_doubles_player.png')
    box(OPPONENT, 32).save('healthbox_doubles_opponent.png')
    box(PLAYER, 64, with_bars=False).save('healthbox_safari.png')
    print('Wrote healthbox plates.')


if __name__ == '__main__':
    main()
