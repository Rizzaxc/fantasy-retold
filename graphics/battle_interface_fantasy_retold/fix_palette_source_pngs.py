#!/usr/bin/env python3
"""Fix ball_status_bar.png and ball_display.png — preserve graphics, replace palette only."""
from PIL import Image
import os

def hex_to_rgb(h):
    h = h.lstrip('#')
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))

def make_indexed_palette(hex_colors):
    pal = []
    for h in hex_colors:
        pal.extend(hex_to_rgb(h))
    while len(pal) < 768:
        pal.extend([0, 0, 0])
    return pal

healthbox_palette = [
    '#FF00FF', '#1A1A2E', '#2D2D44', '#0F0F1A', '#4A4A5E', '#E8E8F0',
    '#8888A0', '#D03040', '#70C040', '#8848A0', '#40B8C8', '#D87830',
    '#6070D0', '#F0D850', '#3890D8', '#E070B0',
]

healthbar_palette = [
    '#FF00FF', '#0F0F1A', '#2D2D44', '#D03040', '#A02030', '#484858',
    '#303040', '#8848A0', '#583070', '#70C040', '#488828', '#40B8C8',
    '#287888', '#4A4A5E', '#E8E8F0', '#F0D850',
]

def replace_palette_only(filename, orig_dir, new_palette_hex):
    """Load image from orig_dir, copy pixel data, apply new palette, save to current dir."""
    orig_path = os.path.join(orig_dir, os.path.basename(filename))
    if not os.path.exists(orig_path):
        print(f"  SKIP {filename} (original not found at {orig_path})")
        return
    orig = Image.open(orig_path)
    if orig.mode != 'P':
        print(f"  SKIP {filename} (not indexed)")
        return
    new_img = orig.copy()
    new_img.putpalette(make_indexed_palette(new_palette_hex))
    new_img.save(filename, optimize=False)
    print(f"  Fixed {filename} ({orig.width}x{orig.height}) — original graphics preserved, new palette applied")

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)
    orig_dir = os.path.join(script_dir, '..', 'battle_interface')

    print("Fixing palette source PNGs (preserving original graphics)...")
    replace_palette_only('ball_status_bar.png', orig_dir, healthbox_palette)
    replace_palette_only('ball_display.png', orig_dir, healthbar_palette)
    print("Done!")

if __name__ == '__main__':
    main()
