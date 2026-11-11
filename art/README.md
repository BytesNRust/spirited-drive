# Spirited Drive art

Our own drawings: nothing here comes from the original game's files, and no font is used. The colours are values
measured from the 1996 title screen.

| Piece | Made by | Output |
|---|---|---|
| Banner | `tools/spirited_logo.py` | `art/logo/spirited_logo.svg` (the vector), `data/logo/spirited_logo.png` (580×140 RGBA, what the boot screen loads) |
| Icon | `tools/spirited_icon.py` | `data/icon/spirited_<size>.png`, `data/icon/spirited.ico` (Windows), `data/icon/tnfs.icns` (macOS) |

Both need Python with Pillow, and `rsvg-convert`. Run them from the repository's root; the same input gives the same
files. To change a colour or a letter, edit the tool and run it again.

## Palette

| Use | Colours |
|---|---|
| Top row of the lettering, red | `#a00018` → `#f60030` → `#ff1818`, left to right |
| Bottom row, orange to amber | `#ff4a10` → `#ff7118` → `#ffae10`, left to right |
| Shadow | black, soft, heavier near the letters and trailing off to the left |
| Title red (backgrounds) | `#ff0000` `#ff1818` `#e60000` `#ce0000` `#b60000` `#950000` `#790000`, mottled |
| Leather, light (gloves) | `#e2ad68` `#cf9350` `#8a5220`, outline `#4a2810` |
| Leather, dark (fob) | `#5a2a18` → `#1e0a05`, stitching and initials in cream `#f6e2b8` |
| Steel (ring, key) | `#f4f0e8` → `#9a958c` |

## Lettering

- Heavy capitals, wide and low: about 1.35 times wider than tall, leaning forward 18°.
- Letters almost touch; the two rows almost touch.
- Both rows have the same letter size and end at the same place on the right, so the shorter word leaves room on the
  left for its streaks.
- Two speed streaks trail to the left of each row, in that row's colour, fading out.
- The letters are outlines drawn for this (`LETTERS` in `tools/spirited_logo.py`). There are only the eight the name
  needs: D, E, I, P, R, S, T, V.

## Finish

Everything is drawn at the game's resolution and left with hard pixels: the front end is 640×480 and is enlarged
whole to the window, so the banner shows the same thick pixels as the original title. Icons of 128 px and above are
the 128 px picture enlarged by whole factors. A fine, fixed grain goes over what is drawn, and the icons are cut to a
short palette (16 to 48 colours by size).

## Icon

- 48 px and above: a pair of driving gloves with the key and its leather fob on top, seen as on a table in front of
  you, over the title red.
- 16 and 32 px: the key, ring and fob alone. The 16 px one is drawn pixel by pixel (`PIXELS_16`).
- The fob carries the initials, "S" over "D", in cream: the banner's own letters.
