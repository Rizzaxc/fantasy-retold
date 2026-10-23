#!/usr/bin/env python3
"""Generate JASC-PAL format .pal files for the dark theme."""
import os

def hex_to_rgb(h):
    h = h.lstrip('#')
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))

def write_jasc_pal(filename, palette_hex):
    with open(filename, 'w', newline='\r\n') as f:
        f.write('JASC-PAL\n')
        f.write('0100\n')
        f.write(f'{len(palette_hex)}\n')
        for h in palette_hex:
            r, g, b = hex_to_rgb(h)
            f.write(f'{r} {g} {b}\n')
    print(f"  Wrote {filename} ({len(palette_hex)} colors)")

text_palette = [
    '#000000', '#E8E8F0', '#8888A0', '#4A4A5E', '#D03040', '#D87830',
    '#70C040', '#488828', '#3890D8', '#206090', '#F0D850', '#8848A0',
    '#40B8C8', '#1A1A2E', '#2D2D44', '#C0C0D0',
]

textbox_palette_0 = [
    '#000000', '#1A1A2E', '#0F0F1A', '#2D2D44', '#4A4A5E', '#E8E8F0',
    '#8888A0', '#D03040', '#70C040', '#3890D8', '#F0D850', '#8848A0',
    '#D87830', '#383848', '#505068', '#C0C0D0',
]

textbox_palette_1 = [
    '#000000', '#E8E8F0', '#8888A0', '#4A4A5E', '#D03040', '#70C040',
    '#3890D8', '#F0D850', '#D87830', '#8848A0', '#40B8C8', '#1A1A2E',
    '#2D2D44', '#0F0F1A', '#383848', '#C0C0D0',
]

ability_pop_up_palette = [
    '#FF00FF', '#1A1A2E', '#2D2D44', '#0F0F1A', '#4A4A5E', '#E8E8F0',
    '#8888A0', '#D03040', '#70C040', '#8848A0', '#40B8C8', '#D87830',
    '#6070D0', '#F0D850', '#3890D8', '#383848',
]

misc_indicator_palette = [
    '#FF00FF', '#1A1A2E', '#2D2D44', '#0F0F1A', '#4A4A5E', '#E8E8F0',
    '#8888A0', '#D03040', '#70C040', '#8848A0', '#40B8C8', '#D87830',
    '#6070D0', '#F0D850', '#3890D8', '#383848',
]

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    print("Generating JASC-PAL .pal files...")
    write_jasc_pal('text.pal', text_palette)
    write_jasc_pal('text_pp.pal', text_palette)
    write_jasc_pal('textbox_0.pal', textbox_palette_0)
    write_jasc_pal('textbox_1.pal', textbox_palette_1)
    write_jasc_pal('ability_pop_up.pal', ability_pop_up_palette)
    write_jasc_pal('misc_indicator.pal', misc_indicator_palette)
    print("Done!")

if __name__ == '__main__':
    main()
