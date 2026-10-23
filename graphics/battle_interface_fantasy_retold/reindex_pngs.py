#!/usr/bin/env python3
"""Reindex battle interface PNGs to use the dark/tactical palette.

This script:
1. Creates ball_status_bar.png and ball_display.png with the correct dark palettes
   (these are the palette source files that the build system extracts .gbapal from)
2. Remaps existing healthbox/bar PNGs to use the dark palette via nearest-color matching
"""
from PIL import Image
import struct
import os

def hex_to_rgb(h):
    h = h.lstrip('#')
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))

def make_indexed_palette(hex_colors):
    """Create a flat palette list for PIL (256 entries x 3 channels = 768 values)."""
    pal = []
    for h in hex_colors:
        pal.extend(hex_to_rgb(h))
    # Pad to 256 colors
    while len(pal) < 768:
        pal.extend([0, 0, 0])
    return pal

def color_distance(c1, c2):
    return sum((a - b) ** 2 for a, b in zip(c1, c2))

def remap_image(img, old_palette_rgb, new_palette_hex):
    """Remap an indexed image from old palette to new palette via nearest color."""
    new_palette_rgb = [hex_to_rgb(h) for h in new_palette_hex]
    new_img = img.copy()
    pixels = list(new_img.getdata())
    new_pixels = []

    for old_idx in pixels:
        if old_idx >= len(old_palette_rgb):
            new_pixels.append(0)
            continue
        old_color = old_palette_rgb[old_idx]
        # Transparent stays transparent
        if old_idx == 0:
            new_pixels.append(0)
            continue
        # Find nearest color in new palette (skip index 0 = transparent)
        best_idx = 1
        best_dist = float('inf')
        for i in range(1, len(new_palette_hex)):
            d = color_distance(old_color, new_palette_rgb[i])
            if d < best_dist:
                best_dist = d
                best_idx = i
        new_pixels.append(best_idx)

    new_img.putdata(new_pixels)
    new_img.putpalette(make_indexed_palette(new_palette_hex))
    return new_img

healthbox_palette = [
    '#FF00FF', '#1A1A2E', '#2D2D44', '#0F0F1A', '#4A4A5E', '#E8E8F0',
    '#8888A0', '#D03040', '#70C040', '#8848A0', '#40B8C8', '#D87830',
    '#6070D0', '#F0D850', '#3890D8', '#383848',
]

healthbar_palette = [
    '#FF00FF', '#0F0F1A', '#2D2D44', '#D03040', '#A02030', '#484858',
    '#303040', '#8848A0', '#583070', '#70C040', '#488828', '#40B8C8',
    '#287888', '#4A4A5E', '#E8E8F0', '#F0D850',
]

def create_palette_source_png(filename, palette_hex, width, height):
    """Create a minimal indexed PNG that serves as a palette source."""
    img = Image.new('P', (width, height))
    img.putpalette(make_indexed_palette(palette_hex))
    # Fill with index 0 (transparent) — the image content doesn't matter for palette-only files,
    # but ball_status_bar.png is also used for its .4bpp graphics.
    pixels = [0] * (width * height)
    img.putdata(pixels)
    img.save(filename, optimize=False)
    print(f"  Created {filename} ({width}x{height}, {len(palette_hex)} colors)")

def get_old_palette_rgb(img):
    """Extract the RGB palette from an indexed image."""
    pal = img.getpalette()
    if pal is None:
        return [(0, 0, 0)] * 256
    colors = []
    for i in range(0, len(pal), 3):
        colors.append((pal[i], pal[i+1], pal[i+2]))
    return colors

def remap_file(filename, new_palette_hex):
    """Remap a PNG file to the new palette."""
    if not os.path.exists(filename):
        print(f"  SKIP {filename} (not found)")
        return
    img = Image.open(filename)
    if img.mode != 'P':
        print(f"  SKIP {filename} (not indexed, mode={img.mode})")
        return
    old_pal = get_old_palette_rgb(img)
    new_img = remap_image(img, old_pal, new_palette_hex)
    new_img.save(filename, optimize=False)
    print(f"  Remapped {filename} ({img.width}x{img.height})")

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    print("Step 1: Creating palette source PNGs...")
    # ball_status_bar.png provides the healthbox palette
    # Original is 120x8 — preserve dimensions
    if os.path.exists('ball_status_bar.png'):
        orig = Image.open('ball_status_bar.png')
        w, h = orig.width, orig.height
    else:
        w, h = 120, 8
    create_palette_source_png('ball_status_bar.png', healthbox_palette, w, h)

    # ball_display.png provides the healthbar palette
    if os.path.exists('ball_display.png'):
        orig = Image.open('ball_display.png')
        w, h = orig.width, orig.height
    else:
        w, h = 24, 8
    create_palette_source_png('ball_display.png', healthbar_palette, w, h)

    print("\nStep 2: Remapping healthbox PNGs to dark palette...")
    healthbox_files = [
        'healthbox_singles_player.png',
        'healthbox_singles_opponent.png',
        'healthbox_doubles_player.png',
        'healthbox_doubles_opponent.png',
        'healthbox_safari.png',
        'misc.png',
        'misc_frameend.png',
        'ability_pop_up.png',
        'numbers1.png',
        'numbers2.png',
        'level_up_banner.png',
        'status.png',
        'status2.png',
        'status3.png',
        'status4.png',
    ]
    for f in healthbox_files:
        remap_file(f, healthbox_palette)

    print("\nStep 3: Remapping bar PNGs to dark healthbar palette...")
    bar_files = [
        'hpbar.png',
        'expbar.png',
        'hpbar_anim.png',
    ]
    for f in bar_files:
        remap_file(f, healthbar_palette)

    print("\nDone! PNGs reindexed to dark palettes.")
    print("The images will look wrong until you draw new pixel art in Aseprite.")
    print("But they will BUILD correctly with the new palette structure.")

if __name__ == '__main__':
    main()
