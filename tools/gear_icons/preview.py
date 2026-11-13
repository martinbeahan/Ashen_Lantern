#!/usr/bin/env python3
"""Contact sheet of every in-app gear glyph inside its rarity socket.

  python3 tools/gear_icons/preview.py [out.png]      (needs cairosvg + Pillow)

Reads docs/art/ash_ember/gear/ic_gear_*.svg (run build_gear_icons.py first).
Sockets mirror bg_gear_socket_*: far-soot fill, rarity stroke, rim hairline.
Pips mirror ic_rarity_*: hollow dot / 1-3 diamonds / flame.
"""
import io
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import cairosvg

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import gear_glyphs  # noqa: E402

SVG = ROOT / 'docs/art/ash_ember/gear'
VOID, FAR, SOOT, RIM = (11, 10, 12), (20, 18, 22), (28, 26, 30), (58, 54, 64)
ASH, FOG, BONE, AMBER, CORE = (74, 70, 80), (110, 104, 114), (217, 210, 195), (242, 165, 65), (255, 210, 122)
RAR = {'common': (163, 157, 148), 'uncommon': (141, 191, 106), 'rare': (120, 180, 234),
       'epic': (166, 118, 214), 'legendary': AMBER}
FONTS = '/usr/share/fonts/truetype/sand-box/google/'


def font(kind, size):
    p = FONTS + ('Cinzel/Cinzel-VariableFont_wght.ttf' if kind == 'cin'
                 else 'Cormorant Garamond/CormorantGaramond-VariableFont_wght.ttf')
    try:
        return ImageFont.truetype(p, size)
    except Exception:
        return ImageFont.load_default()


def render(stem, size):
    png = cairosvg.svg2png(url=str(SVG / f'{stem}.svg'), output_width=size, output_height=size)
    return Image.open(io.BytesIO(png)).convert('RGBA')


def socket(stem, rarity, size, pad):
    side = size + pad * 2
    im = Image.new('RGBA', (side, side), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    w = max(2, side // 40)
    d.rounded_rectangle((0, 0, side - 1, side - 1), radius=max(3, side // 18), fill=FAR, outline=RAR[rarity], width=w)
    d.rounded_rectangle((w + 2, w + 2, side - w - 3, side - w - 3), radius=max(2, side // 24), outline=RIM, width=1)
    g = render(stem, size)
    im.alpha_composite(g, (pad, pad))
    return im


def pip(d, x, y, rarity):
    col = RAR[rarity]
    if rarity == 'common':
        d.ellipse((x, y, x + 12, y + 12), outline=col, width=2)
    elif rarity == 'legendary':
        d.polygon([(x + 6, y - 4), (x + 12, y + 8), (x + 6, y + 14), (x, y + 8)], fill=AMBER)
        d.polygon([(x + 6, y + 4), (x + 9, y + 9), (x + 6, y + 12), (x + 3, y + 9)], fill=CORE)
    else:
        n = {'uncommon': 1, 'rare': 2, 'epic': 3}[rarity]
        for i in range(n):
            cx = x + 6 + i * 16
            d.polygon([(cx, y), (cx + 6, y + 6), (cx, y + 12), (cx - 6, y + 6)], fill=col)


def main(out):
    base = list(gear_glyphs.GLYPHS)
    legs = sorted(set(gear_glyphs.LEGENDARY_GLYPHS))
    W = 1900
    cell, gap = 150, 26
    H = 2240
    sheet = Image.new('RGBA', (W, H), (*VOID, 255))
    d = ImageDraw.Draw(sheet)
    d.rectangle((0, 0, W, 8), fill=AMBER)
    d.text((48, 28), 'ASHEN LANTERN  ·  GEAR GLYPHS  v2.27', font=font('cin', 34), fill=BONE)
    d.text((48, 76), f'{len(base)} type glyphs · {len(legs)} Legendary inlay variants (+ calm). '
           'Rarity lives in the socket stroke and the row pip. Hero gear amber, never teal.',
           font=font('corm', 22), fill=FOG)

    def label(x, y, text, col=BONE, size=15):
        d.text((x, y), text, font=font('cin', size), fill=col)

    # 1) Every type glyph (natural rarity frame).
    y = 130
    label(48, y, 'TYPE GLYPHS', AMBER, 20)
    y += 40
    natural = {'ashen_lantern': 'rare', 'raid_key': 'rare', 'gold': 'legendary'}
    cols = 9
    for i, name in enumerate(base):
        x = 48 + (i % cols) * (cell + 50)
        yy = y + (i // cols) * (cell + 70)
        r = natural.get(name, 'common')
        s = socket(f'ic_gear_{name}', r, cell - 16, 8)
        sheet.alpha_composite(s, (x, yy))
        label(x, yy + cell + 6, name.replace('_', ' ').upper(), BONE, 14)
    y += 2 * (cell + 70) + 10

    # 2) Rarity frames on one glyph per slot.
    label(48, y, 'RARITY FRAMES  ·  common · uncommon · rare · epic · legendary · legendary calm (reduce flashing)', AMBER, 20)
    y += 66
    for row, name in enumerate(('sword', 'mail', 'mace')):
        for j, r in enumerate(('common', 'uncommon', 'rare', 'epic', 'legendary', 'calm')):
            x = 48 + j * (cell + 120)
            yy = y + row * (cell + 40)
            if r == 'legendary':
                stem, rr = f'ic_gear_{name}_legendary', 'legendary'
            elif r == 'calm':
                stem, rr = f'ic_gear_{name}_legendary_calm', 'legendary'
            else:
                stem, rr = f'ic_gear_{name}', r
            sheet.alpha_composite(socket(stem, rr, cell - 16, 8), (x, yy))
            pip(d, x + cell + 10, yy + cell // 2 - 6, rr)
            if row == 0:
                label(x, yy - 22, r.upper(), RAR[rr], 13)
    y += 3 * (cell + 40) + 10

    # 3) Legendary variants: glow vs calm.
    label(48, y, 'LEGENDARY  ·  amber inlay + static glow  /  calm: inlay kept, no glow', AMBER, 20)
    y += 40
    small = 120
    for i, name in enumerate(legs):
        x = 48 + (i % 6) * (2 * small + 50)
        yy = y + (i // 6) * (small + 60)
        sheet.alpha_composite(socket(f'ic_gear_{name}_legendary', 'legendary', small - 12, 6), (x, yy))
        sheet.alpha_composite(socket(f'ic_gear_{name}_legendary_calm', 'legendary', small - 12, 6), (x + small + 6, yy))
        label(x, yy + small + 6, name.replace('_', ' ').upper(), AMBER, 13)
    y += 3 * (small + 60) + 10

    # 4) Phone scale: 40dp socket at xxhdpi (~105px) and mdpi (40px), plus 24dp key/potion/coin.
    label(48, y, 'PHONE SCALE  ·  40dp socket at 1x (40px) and 2.625x (105px)  ·  24dp chips', AMBER, 20)
    y += 40
    for i, name in enumerate(base):
        x = 48 + i * 100
        sheet.alpha_composite(socket(f'ic_gear_{name}', natural.get(name, 'common'), 36, 2), (x, y))
    y += 60
    for i, name in enumerate(base[:16]):
        x = 48 + i * 116
        sheet.alpha_composite(socket(f'ic_gear_{name}', natural.get(name, 'common'), 99, 3), (x, y))
    y += 130
    for i, name in enumerate(('potion', 'raid_key', 'gold')):
        x = 48 + i * 260
        d.rounded_rectangle((x, y, x + 230, y + 70), radius=8, fill=SOOT)
        sheet.alpha_composite(render(f'ic_gear_{name}', 63), (x + 6, y + 4))
        d.text((x + 80, y + 20), {'potion': 'Potion', 'raid_key': 'Keys 2/3', 'gold': 'Gold: 140g'}[name],
               font=font('corm', 26), fill=BONE)
    sheet.convert('RGB').save(out)
    print('wrote', out)


if __name__ == '__main__':
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else Path('/workspace/gear_icons_preview.png'))
