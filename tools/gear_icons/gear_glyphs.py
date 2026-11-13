"""Ash & Ember in-app gear glyphs (v2.27).

Extends draw_gear.py (the approved proposal samples) with the glyphs the proposal
left for the PR: wand, mace, hammer, maul, cloth, coat and gold. Glyphs here are
drawn WITHOUT the sample plate: in the app the rarity socket (bg_gear_socket_*)
is the plate and frame, so the glyph only carries the object.

Legendary variants add an amber inlay on the object plus static glow circles;
the calm (reduce-flash) variant keeps the inlay and drops the glow circles.
Hero gear is amber / ember / bone / iron — never teal.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ae_lib import P, C, ring, flame, poly, f, F, V, S, W, R, ASH, FOG, B, A, AC, EM, OB  # noqa: F401
import draw_gear as g
from draw_gear import rrect, wick


def svg(body, name):
    return (f'<?xml version="1.0" encoding="UTF-8"?>\n'
            f'<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">\n'
            f'<!-- Ash & Ember gear glyph: {name}. Original Ashen Lantern art; paths/circles only. -->\n'
            f'{body}\n</svg>\n')


# ---------------------------------------------------------------------------
# New weapons
# ---------------------------------------------------------------------------

def wand_glyph():
    """Arcane Wand — short tapered diagonal rod, iron ferrules, amber shard tip.
    Diagonal and caged-free so it never reads as the vertical staff."""
    out = ''
    # Rod: tapered quad from the grip (bottom-left) to the tip (top-right).
    out += poly([(150, 432), (178, 452), (346, 178), (328, 166)], W)
    out += poly([(150, 432), (160, 440), (334, 170), (328, 166)], A, 0.5)   # lantern rim
    # Grip wrap.
    out += poly([(140, 420), (186, 450), (214, 404), (170, 376)], R)
    for t in (0.0, 0.5, 1.0):
        y0 = 384 + t * 40
        out += poly([(176 - t * 22, y0 - 6), (206 - t * 22, y0 + 12),
                     (202 - t * 22, y0 + 18), (172 - t * 22, y0)], ASH)
    out += C(150, 446, 16, R) + C(147, 443, 7, B)
    # Ferrule under the shard.
    out += poly([(316, 196), (356, 222), (370, 196), (332, 172)], R)
    out += C(343, 197, 6, A)
    # Shard: amber diamond with core, pointing up-right.
    out += poly([(352, 176), (404, 92), (382, 196), (360, 196)], A)
    out += poly([(360, 170), (396, 108), (378, 184)], AC)
    out += C(392, 118, 28, A, 0.10)
    return out


def _haft(x, top, bottom, half=16, fill=W, rim=True):
    out = poly([(x - half, top), (x + half, top), (x + half - 3, bottom), (x - half + 3, bottom)], fill)
    if rim:
        out += poly([(x - half, top), (x - half + 7, top), (x - half + 9, bottom), (x - half + 3, bottom)], A, 0.5)
    return out


def _grip(x, y0, y1, half=18):
    out = poly([(x - half, y0), (x + half, y0), (x + half, y1), (x - half, y1)], R)
    step = (y1 - y0) / 4
    for i in range(1, 4):
        y = y0 + step * i
        out += poly([(x - half, y - 4), (x + half, y + 2), (x + half, y + 6), (x - half, y)], ASH)
    return out


def mace_glyph():
    """Flanged mace — round iron head with six flanges on a short haft."""
    x, cy = 256, 168
    out = _haft(x, 210, 420)
    out += _grip(x, 360, 430)
    out += C(x, 448, 18, R) + C(x - 3, 445, 8, B)
    # Flanges (six, around the head).
    for dx, dy in ((0, -1), (0.87, -0.5), (0.87, 0.5), (0, 1), (-0.87, 0.5), (-0.87, -0.5)):
        nx, ny = -dy, dx
        bx, by = x + dx * 50, cy + dy * 50
        tx, ty = x + dx * 104, cy + dy * 104
        out += poly([(bx + nx * 22, by + ny * 22), (tx + nx * 8, ty + ny * 8),
                     (tx - nx * 8, ty - ny * 8), (bx - nx * 22, by - ny * 22)], FOG)
    out += C(x, cy, 62, R)
    out += C(x, cy, 40, ASH)
    out += P(f'M{f(x-62)} {f(cy)} A62 62 0 0 1 {f(x)} {f(cy-62)} L{f(x)} {f(cy-50)} '
             f'A50 50 0 0 0 {f(x-50)} {f(cy)} Z', A, 0.6)
    out += C(x, cy, 12, F)
    # Collar.
    out += poly([(x - 28, 222), (x + 28, 222), (x + 22, 246), (x - 22, 246)], R)
    return out


def hammer_glyph():
    """Cleric's Warhammer — flat striking face on one side, short beak on the other."""
    x = 256
    out = _haft(x, 150, 420, half=15)
    out += _grip(x, 352, 428, half=17)
    out += C(x, 446, 18, R) + C(x - 3, 443, 8, B)
    # Head block (centre).
    out += poly([(x - 46, 96), (x + 46, 96), (x + 50, 200), (x - 50, 200)], R)
    # Striking face (left): a squat block.
    out += poly([(x - 150, 112), (x - 44, 116), (x - 44, 180), (x - 150, 184)], FOG)
    out += poly([(x - 162, 104), (x - 140, 104), (x - 140, 192), (x - 162, 192)], R)
    out += poly([(x - 162, 104), (x - 154, 104), (x - 154, 192), (x - 162, 192)], A, 0.55)
    # Beak (right): tapering spike.
    out += poly([(x + 44, 124), (x + 166, 156), (x + 44, 176)], FOG)
    out += poly([(x + 44, 150), (x + 166, 156), (x + 44, 176)], ASH)
    # Rivets + top spike.
    out += C(x, 148, 10, A)
    out += poly([(x - 18, 96), (x, 58), (x + 18, 96)], ASH)
    return out


def maul_glyph():
    """War Maul — huge two-handed block head, banded, long thick haft."""
    x = 256
    out = _haft(x, 210, 440, half=19)
    out += _grip(x, 300, 420, half=21)
    out += C(x, 456, 20, R) + C(x - 3, 453, 9, B)
    # Big head.
    out += poly([(x - 150, 78), (x + 150, 78), (x + 158, 226), (x - 158, 226)], FOG)
    out += poly([(x - 150, 78), (x - 138, 78), (x - 146, 226), (x - 158, 226)], A, 0.5)
    # Iron bands.
    for bx in (-110, 0, 110):
        out += poly([(x + bx - 16, 72), (x + bx + 16, 72), (x + bx + 18, 232), (x + bx - 18, 232)], R)
        out += C(x + bx, 152, 7, A if bx == 0 else ASH)
    # Shadowed underside.
    out += poly([(x - 158, 214), (x + 158, 214), (x + 158, 226), (x - 158, 226)], R)
    return out


# ---------------------------------------------------------------------------
# New armour
# ---------------------------------------------------------------------------

def cloth_glyph():
    """Robes / clothes / vestments / mantle / cloak — hooded robe, wide flared hem."""
    out = ''
    # Hood cowl (behind shoulders).
    out += P('M184 150 Q256 64 328 150 L300 170 Q256 132 212 170 Z', R)
    # Robe body: narrow shoulders, flaring hem.
    out += poly([(184, 150), (212, 170), (256, 160), (300, 170), (328, 150),
                 (348, 260), (392, 430), (256, 446), (120, 430), (164, 260)], ASH)
    # Wide sleeves.
    out += poly([(184, 150), (120, 250), (112, 330), (166, 318), (186, 236)], W)
    out += poly([(328, 150), (392, 250), (400, 330), (346, 318), (326, 236)], W)
    # Lantern rim (left edge).
    out += poly([(164, 260), (172, 262), (130, 430), (120, 430)], A, 0.45)
    out += poly([(184, 150), (192, 154), (128, 252), (120, 250)], A, 0.4)
    # Centre fold + hem fold lines.
    out += poly([(250, 260), (262, 260), (268, 440), (244, 440)], W)
    out += poly([(196, 300), (206, 300), (176, 434), (164, 432)], W, 0.8)
    out += poly([(306, 300), (316, 300), (348, 432), (336, 434)], W, 0.8)
    # Rope sash.
    out += poly([(166, 244), (346, 244), (348, 266), (164, 266)], B)
    out += poly([(240, 262), (254, 262), (246, 330), (232, 330)], B)
    out += poly([(258, 262), (272, 262), (282, 322), (268, 322)], B)
    out += C(256, 255, 12, A)
    # Collar clasp.
    out += C(256, 172, 8, A)
    return out


def coat_glyph():
    """Troubadour Coat — fitted long coat, high collar, lapels, button row, split tails."""
    out = ''
    # Body to the waist.
    out += poly([(176, 156), (226, 128), (256, 150), (286, 128), (336, 156),
                 (350, 300), (330, 310), (182, 310), (162, 300)], W)
    # Split tails.
    out += poly([(182, 300), (252, 300), (250, 448), (154, 436)], W)
    out += poly([(330, 300), (260, 300), (262, 448), (358, 436)], W)
    out += poly([(252, 330), (260, 330), (262, 448), (250, 448)], F)
    # Sleeves (shoulder seams) in rim.
    out += poly([(176, 156), (140, 196), (130, 326), (164, 330), (174, 210)], R)
    out += poly([(336, 156), (372, 196), (382, 326), (348, 330), (338, 210)], R)
    # Cuffs in bone.
    out += poly([(130, 312), (164, 316), (164, 334), (130, 330)], B)
    out += poly([(382, 312), (348, 316), (348, 334), (382, 330)], B)
    # High collar + lapels.
    out += poly([(206, 112), (226, 128), (256, 196), (230, 210), (196, 150)], ASH)
    out += poly([(306, 112), (286, 128), (256, 196), (282, 210), (316, 150)], ASH)
    out += poly([(226, 128), (256, 150), (286, 128), (256, 196)], F)
    # Lantern rim on the left.
    out += poly([(162, 300), (170, 300), (162, 436), (154, 436)], A, 0.45)
    out += poly([(176, 156), (184, 152), (148, 194), (140, 196)], A, 0.4)
    # Belt.
    out += poly([(166, 290), (346, 290), (348, 314), (164, 314)], R)
    out += rrect(240, 286, 32, 32, 4, ASH)
    # Double button row (amber).
    for y in (220, 252):
        out += C(236, y, 7, A) + C(276, y, 7, A)
    return out


# ---------------------------------------------------------------------------
# Meta
# ---------------------------------------------------------------------------

def gold_glyph():
    """Gold — a stamped coin in front of a second coin edge; lantern-flame stamp."""
    out = ''
    # Back coin (edge on, upper right).
    out += C(318, 210, 132, OB)
    out += C(318, 210, 132, A, 0.55)
    out += C(318, 210, 104, R, 0.35)
    # Front coin.
    out += C(232, 300, 146, R)
    out += C(232, 300, 136, A)
    out += ring(232, 300, 112, 10, OB, 0.55)
    out += C(232, 300, 102, AC, 0.35)
    # Lantern-flame stamp (no glow).
    out += flame(232, 352, 3.4, OB, None, glow=False)
    out += flame(232, 344, 1.6, AC, None, glow=False)
    # Bone glint.
    out += P('M140 236 Q154 200 186 180 Q162 214 152 248 Z', B, 0.55)
    return out


# ---------------------------------------------------------------------------
# Legendary inlay + glow
# ---------------------------------------------------------------------------

def inlay(cx, cy, s=1.0):
    """Amber inlay set into the object: iron setting, amber diamond, core."""
    return (poly([(cx, cy - 40*s), (cx + 30*s, cy), (cx, cy + 40*s), (cx - 30*s, cy)], R) +
            poly([(cx, cy - 30*s), (cx + 21*s, cy), (cx, cy + 30*s), (cx - 21*s, cy)], A) +
            poly([(cx, cy - 15*s), (cx + 10*s, cy), (cx, cy + 15*s), (cx - 10*s, cy)], AC))


def glow(cx, cy, r=110):
    return C(cx, cy, r, A, 0.10) + C(cx, cy, r * 0.55, A, 0.16)


# glyph -> (builder, legendary anchor (x, y, scale) or None when it is never Legendary)
GLYPHS = {
    # Weapons
    'sword':         (lambda: g.sword_glyph(False), None),           # legendary = Crownbreaker build
    'short_blade':   (g.short_blade_glyph, (252, 190, 0.9)),
    'rapier':        (g.rapier_glyph, (232, 328, 0.9)),
    'staff':         (g.staff_glyph, (256, 310, 0.8)),
    'wand':          (wand_glyph, (246, 306, 0.75)),
    'mace':          (mace_glyph, (256, 168, 0.85)),
    'hammer':        (hammer_glyph, (256, 148, 0.8)),
    'maul':          (maul_glyph, (256, 152, 1.0)),
    'ashen_lantern': (g.ashen_lantern_glyph, None),                  # unique, Rare
    # Armour
    'leather':       (g.leather_glyph, (256, 332, 0.75)),
    'cloth':         (cloth_glyph, (256, 200, 0.8)),
    'mail':          (g.mail_glyph, (256, 316, 0.9)),
    'coat':          (coat_glyph, (256, 236, 0.0)),                  # see legendary_body
    'plate':         (lambda: g.plate_glyph(False), (256, 362, 0.6)),
    # Consumables + meta
    'potion':        (g.potion_glyph, None),
    'tonic':         (g.tonic_glyph, None),
    'raid_key':      (g.raid_key_glyph, None),
    'gold':          (gold_glyph, None),
}

LEGENDARY_GLYPHS = [k for k, (_b, a) in GLYPHS.items() if a is not None] + ['sword']


def body(name):
    return GLYPHS[name][0]()


def legendary_body(name, with_glow=True):
    if name == 'sword':
        return g.sword_glyph(True, with_glow)      # Crownbreaker: crown guard + amber fuller
    if name == 'plate':
        out = glow(256, 260, 120) if with_glow else ''
        return out + g.plate_glyph(False) + inlay(256, 362, 0.6)
    builder, (ax, ay, s) = GLYPHS[name]
    out = glow(256, 256, 120) if with_glow else ''
    out += builder()
    if name == 'coat':
        # Coat: the button row becomes amber inlays, plus a brooch on the collar.
        out += inlay(256, 206, 0.55)
        return out
    return out + inlay(ax, ay, s)


def all_outputs():
    """(file stem, svg text). ic_gear_<glyph>, ic_gear_<glyph>_legendary(_calm)."""
    outs = []
    for name in GLYPHS:
        outs.append((f'ic_gear_{name}', svg(body(name), name)))
    for name in sorted(set(LEGENDARY_GLYPHS)):
        outs.append((f'ic_gear_{name}_legendary', svg(legendary_body(name, True), name + ' legendary')))
        outs.append((f'ic_gear_{name}_legendary_calm',
                     svg(legendary_body(name, False), name + ' legendary (calm: no glow, inlay kept)')))
    return outs
