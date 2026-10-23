#!/usr/bin/env python3
"""Generate dark/tactical palette files for Fantasy Retold battle UI.

Creates .gbapal files (raw GBA RGB555 binary) and reindexes PNGs to the new palettes.
Run from the battle_interface_fantasy_retold/ directory.
"""
import struct
import os

def rgb888_to_rgb555(r, g, b):
    """Convert 8-bit RGB to GBA 15-bit RGB555 (little-endian u16)."""
    r5 = (r >> 3) & 0x1F
    g5 = (g >> 3) & 0x1F
    b5 = (b >> 3) & 0x1F
    return r5 | (g5 << 5) | (b5 << 10)

def hex_to_rgb(h):
    """Convert '#RRGGBB' to (r, g, b) tuple."""
    h = h.lstrip('#')
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))

def write_gbapal(filename, palette_hex):
    """Write a 16-color .gbapal file (32 bytes, 16x u16 LE)."""
    data = bytearray()
    for h in palette_hex:
        r, g, b = hex_to_rgb(h)
        val = rgb888_to_rgb555(r, g, b)
        data += struct.pack('<H', val)
    with open(filename, 'wb') as f:
        f.write(data)
    print(f"  Wrote {filename} ({len(data)} bytes, {len(palette_hex)} colors)")

# --- Healthbox palette (from ball_status_bar.png) ---
healthbox_palette = [
    '#FF00FF',  # 0  Transparent (magenta)
    '#1A1A2E',  # 1  BG Dark
    '#2D2D44',  # 2  BG Mid
    '#0F0F1A',  # 3  Border Dark
    '#4A4A5E',  # 4  Border Light
    '#E8E8F0',  # 5  Text White
    '#8888A0',  # 6  Text Grey
    '#D03040',  # 7  HP Red
    '#70C040',  # 8  Stamina Green
    '#8848A0',  # 9  Trauma Purple
    '#40B8C8',  # 10 EXP Cyan
    '#D87830',  # 11 Status Warm
    '#6070D0',  # 12 Status Cool
    '#F0D850',  # 13 Highlight/PAR
    '#3890D8',  # 14 Status Water
    '#E070B0',  # 15 Status Pink (Confusion line in the healthbox status column)
]

# --- Healthbar palette (from ball_display.png) ---
healthbar_palette = [
    '#FF00FF',  # 0  Transparent
    '#0F0F1A',  # 1  Bar BG
    '#2D2D44',  # 2  Bar Border
    '#D03040',  # 3  HP Red bright
    '#A02030',  # 4  HP Red dark
    '#484858',  # 5  HP Empty Grey bright
    '#303040',  # 6  HP Empty Grey dark
    '#8848A0',  # 7  Trauma Purple bright
    '#583070',  # 8  Trauma Purple dark
    '#70C040',  # 9  Stamina Green bright
    '#488828',  # 10 Stamina Green dark
    '#40B8C8',  # 11 EXP Cyan bright
    '#287888',  # 12 EXP Cyan dark
    '#4A4A5E',  # 13 Bar frame light
    '#E8E8F0',  # 14 White
    '#F0D850',  # 15 Highlight
]

# --- Textbox palette (2x16 = 32 colors at BG_PLTT_ID(0)) ---
textbox_palette_0 = [
    '#000000',  # 0  Transparent / black
    '#1A1A2E',  # 1  Textbox BG fill
    '#0F0F1A',  # 2  Textbox border dark
    '#2D2D44',  # 3  Textbox border mid
    '#4A4A5E',  # 4  Textbox border light
    '#E8E8F0',  # 5  Text primary
    '#8888A0',  # 6  Text secondary
    '#D03040',  # 7  Accent red
    '#70C040',  # 8  Accent green
    '#3890D8',  # 9  Accent blue
    '#F0D850',  # 10 Accent yellow
    '#8848A0',  # 11 Accent purple
    '#D87830',  # 12 Accent orange
    '#383848',  # 13 Dark fill
    '#505068',  # 14 Mid tone
    '#C0C0D0',  # 15 Light text
]
textbox_palette_1 = [
    '#000000',  # 0
    '#E8E8F0',  # 1  Text white
    '#8888A0',  # 2  Text grey
    '#4A4A5E',  # 3  Text dark
    '#D03040',  # 4  Red
    '#70C040',  # 5  Green
    '#3890D8',  # 6  Blue
    '#F0D850',  # 7  Yellow
    '#D87830',  # 8  Orange
    '#8848A0',  # 9  Purple
    '#40B8C8',  # 10 Cyan
    '#1A1A2E',  # 11 BG dark
    '#2D2D44',  # 12 BG mid
    '#0F0F1A',  # 13 Border
    '#383848',  # 14 Panel
    '#C0C0D0',  # 15 Light
]

# --- Text palette (action menu / move text at BG_PLTT_ID(5)) ---
text_palette = [
    '#000000',  # 0  Transparent
    '#E8E8F0',  # 1  Text white
    '#8888A0',  # 2  Text grey (shadow)
    '#4A4A5E',  # 3  Text dark grey
    '#D03040',  # 4  Red (HP low)
    '#D87830',  # 5  Orange
    '#70C040',  # 6  Green (available)
    '#488828',  # 7  Dark green
    '#3890D8',  # 8  Blue
    '#206090',  # 9  Dark blue
    '#F0D850',  # 10 Yellow
    '#8848A0',  # 11 Purple
    '#40B8C8',  # 12 Cyan
    '#E8E8F0',  # 13 Window text (battle windows draw text 13 on 14 with shadow 15)
    '#2D2D44',  # 14 Window panel
    '#1A1A2E',  # 15 Window text shadow
]

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    print("Generating dark/tactical palette files...")

    # Textbox palette (.gbapal = 2 palettes concatenated = 64 bytes)
    data = bytearray()
    for h in textbox_palette_0 + textbox_palette_1:
        r, g, b = hex_to_rgb(h)
        data += struct.pack('<H', rgb888_to_rgb555(r, g, b))
    with open('textbox.gbapal', 'wb') as f:
        f.write(data)
    print(f"  Wrote textbox.gbapal ({len(data)} bytes, 32 colors)")

    # Individual palette variants
    write_gbapal('textbox_0.gbapal', textbox_palette_0)
    write_gbapal('textbox_1.gbapal', textbox_palette_1)

    # Text palette (action/move text)
    write_gbapal('text.gbapal', text_palette)

    # Text PP palette (reuse text palette)
    write_gbapal('text_pp.gbapal', text_palette)

    # Healthbox palette — written into ball_status_bar.png's embedded palette
    # (the .gbapal is extracted from the PNG by gbagfx during build)
    # We need to generate a separate .gbapal for reference
    write_gbapal('healthbox_dark.gbapal', healthbox_palette)
    write_gbapal('healthbar_dark.gbapal', healthbar_palette)

    print("\nDone! Palette files generated.")
    print("\nIMPORTANT: The healthbox palette must be embedded in ball_status_bar.png")
    print("and the healthbar palette in ball_display.png (as indexed PNG palettes).")
    print("Use these .gbapal files as reference when setting up Aseprite palettes.")

if __name__ == '__main__':
    main()
