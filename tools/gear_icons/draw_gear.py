"""Ash & Ember gear icon samples — hand-authored, VectorDrawable-safe.

512 canvas. Paths and circles only, flat fills + fill-opacity, evenodd cut-outs,
simple strokes. No filters, masks, gradients, transforms, rect or text.
Hero-side items use amber / ember / bone and never teal.

Glyphs sit on a far-soot rounded plate so they read on any ground. Rarity lives
in the frame (composited by the sheet), except legendary glyphs which carry a
static amber wick; the calm legendary variant drops glow circles.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ae_lib import P, C, STK, ring, circ_d, flame, poly, f, F, V, S, W, R, ASH, FOG, B, A, AC, EM, OB

# Healing / uncommon / rare / epic are frame colours only — not used on glyphs.
COMMON, LICHEN, RARE_C, EPIC_C = '#A39D94', '#8DBF6A', '#78B4EA', '#A676D6'


def svg(body, name):
    return (f'<?xml version="1.0" encoding="UTF-8"?>\n'
            f'<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">\n'
            f'<!-- Ash & Ember gear icon: {name}. Paths/circles, flat fills, evenodd, no filters. -->\n'
            f'{body}\n</svg>\n')


def rrect(x, y, w, h, r, fill, op=None):
    """Rounded rect as a single path (no <rect>)."""
    d = (f'M{f(x+r)} {f(y)} L{f(x+w-r)} {f(y)} Q{f(x+w)} {f(y)} {f(x+w)} {f(y+r)} '
         f'L{f(x+w)} {f(y+h-r)} Q{f(x+w)} {f(y+h)} {f(x+w-r)} {f(y+h)} '
         f'L{f(x+r)} {f(y+h)} Q{f(x)} {f(y+h)} {f(x)} {f(y+h-r)} '
         f'L{f(x)} {f(y+r)} Q{f(x)} {f(y)} {f(x+r)} {f(y)} Z')
    return P(d, fill, op)


def plate():
    # Outer plate + inset hairline (evenodd ring-like path would also work).
    outer = rrect(28, 28, 456, 456, 20, F)
    # Hairline as a thin evenodd frame in wax.
    d = (f'M{f(44)} {f(44)} L{f(468)} {f(44)} L{f(468)} {f(468)} L{f(44)} {f(468)} Z '
         f'M{f(48)} {f(48)} L{f(48)} {f(464)} L{f(464)} {f(464)} L{f(464)} {f(48)} Z')
    # Rounded hairline is nicer:
    inset = rrect(42, 42, 428, 428, 14, W, 0.55)
    hole = rrect(46, 46, 420, 420, 12, F)
    return outer + inset + hole


def wick(cx, cy, s=1.0, glow=True):
    """Compact wick + flame for legendary marks and the tonic cap."""
    return (C(cx, cy + 4*s, 3.2*s, V) +
            rrect(cx - 2.2*s, cy - 2*s, 4.4*s, 10*s, 1, V) +
            flame(cx, cy - 2*s, 0.72*s, A, AC, glow=glow))


# ---------------------------------------------------------------------------
# Weapons
# ---------------------------------------------------------------------------

def _blade(tip, guard, half, fill, fuller=None):
    """Tapered two-edge blade from guard centre to tip, optional fuller cut."""
    tx, ty = tip
    gx, gy = guard
    dx, dy = tx - gx, ty - gy
    ln = (dx*dx + dy*dy) ** 0.5 or 1
    nx, ny = -dy / ln, dx / ln  # left normal
    # Widen slightly at the guard, taper to a point.
    p = [
        (gx + nx * half, gy + ny * half),
        (gx + dx * 0.82 + nx * half * 0.72, gy + dy * 0.82 + ny * half * 0.72),
        (tx, ty),
        (gx + dx * 0.82 - nx * half * 0.72, gy + dy * 0.82 - ny * half * 0.72),
        (gx - nx * half, gy - ny * half),
    ]
    d = 'M' + ' L'.join(f'{f(x)} {f(y)}' for x, y in p) + ' Z'
    if fuller:
        fx0, fy0, fx1, fy1, fw = fuller
        fnx, fny = nx, ny
        d += (f' M{f(fx0+fnx*fw)} {f(fy0+fny*fw)} L{f(fx1+fnx*fw)} {f(fy1+fny*fw)} '
              f'L{f(fx1-fnx*fw)} {f(fy1-fny*fw)} L{f(fx0-fnx*fw)} {f(fy0-fny*fw)} Z')
        return P(d, fill, rule=True)
    return P(d, fill)


def _hilt(guard, pommel, half, fill=W):
    gx, gy = guard
    px, py = pommel
    dx, dy = px - gx, py - gy
    ln = (dx*dx + dy*dy) ** 0.5 or 1
    nx, ny = -dy / ln, dx / ln
    p = [
        (gx + nx * half, gy + ny * half),
        (px + nx * half * 0.85, py + ny * half * 0.85),
        (px - nx * half * 0.85, py - ny * half * 0.85),
        (gx - nx * half, gy - ny * half),
    ]
    return poly(p, fill)


def sword_glyph(legendary=False, glow=True):
    """Long cutting blade, slight right tilt. Legendary: crown guard + amber fuller."""
    tip, guard, pommel = (302, 72), (236, 356), (214, 456)
    # Legendary blade stops short of the guard so the crown tines stay visible.
    blade_end = (242, 316) if legendary else (236, 348)
    half = 34 if legendary else 28
    out = ''
    if legendary and glow:
        out += C(270, 200, 90, A, 0.10) + C(270, 200, 50, A, 0.16)
    out += _blade((tip[0] - 6, tip[1] + 2), (blade_end[0] - 8, blade_end[1]), half + 6, A if legendary else R)
    if legendary:
        out += _blade(tip, blade_end, half, B)
        out += _blade((290, 100), (248, 292), 7, A)
        out += _blade((286, 130), (252, 270), 3.5, AC)
    else:
        out += _blade(tip, blade_end, half, B,
                      fuller=(270, 120, 248, 320, 5.5))
    gx, gy = guard
    if legendary:
        # Solid crossbar, wider than the blade.
        out += poly([(gx - 90, gy + 10), (gx - 90, gy - 10), (gx + 94, gy - 12),
                     (gx + 94, gy + 10), (gx + 82, gy + 22), (gx - 80, gy + 22)], R)
        # Three rising tines — the Crownbreaker tell.
        def tine(x, h, col):
            w = 18 if h > 36 else 15
            return poly([(x - w, gy - 10), (x - w * 0.35, gy - 10 - h),
                         (x, gy - 18 - h), (x + w * 0.35, gy - 10 - h),
                         (x + w, gy - 10)], col)
        out += tine(gx - 52, 30, FOG)
        out += tine(gx, 48, A)
        out += tine(gx + 52, 30, FOG)
        out += C(gx, gy - 62, 5.5, AC)
    else:
        out += poly([(gx - 70, gy + 6), (gx - 66, gy - 10), (gx + 72, gy - 14),
                     (gx + 76, gy + 4), (gx + 66, gy + 16), (gx - 62, gy + 18)], R)
        out += poly([(gx - 58, gy - 2), (gx + 64, gy - 6), (gx + 62, gy + 4), (gx - 56, gy + 6)], FOG)
    out += _hilt((gx, gy + 8), pommel, 16, W)
    px, py = pommel
    for i, t in enumerate((0.22, 0.45, 0.68)):
        bx = gx + (px - gx) * t
        by = gy + 8 + (py - gy - 8) * t
        out += C(bx, by, 15, V if i == 1 else S)
        out += C(bx, by, 11, W)
    out += C(px, py, 22, R) + C(px - 3, py - 3, 14, ASH) + C(px - 4, py - 5, 6, A if legendary else B)
    return out


def short_blade_glyph():
    """Leaf dagger / stiletto / dirk / shortsword — short blade, long grip."""
    gx, gy = 248, 268
    px, py = 230, 456
    # Dark under-edge.
    out = poly([(248, 92), (328, 188), (276, 268), (220, 268), (232, 188)], R)
    # Leaf blade: wide at mid, short, pointed.
    out += poly([(256, 80), (318, 186), (268, 260), (228, 260), (236, 186)], B)
    # Soot mid-rib cut (evenodd overlay as a thin soot leaf).
    out += poly([(256, 118), (274, 186), (258, 248), (248, 248), (244, 186)], F)
    # Downswept quillons, wider than a longsword's, shorter blade above them.
    out += poly([(gx - 78, gy + 22), (gx - 86, gy + 2), (gx - 20, gy - 12),
                 (gx + 16, gy - 12), (gx + 80, gy + 6), (gx + 74, gy + 24),
                 (gx + 20, gy + 12), (gx - 24, gy + 12)], R)
    out += _hilt((gx, gy), (px, py), 14, W)
    for t in (0.25, 0.5, 0.75):
        out += C(gx + (px - gx) * t, gy + (py - gy) * t, 13, S)
    out += C(px, py, 18, R) + C(px - 3, py - 3, 8, B)
    return out


def rapier_glyph():
    """Thin thrust blade with a cup hilt — reads as a rapier even at 48px."""
    return _rapier_clean((310, 64), (232, 328), (208, 456))


def _rapier_clean(tip, cup, pommel):
    cx, cy = cup
    out = _blade((tip[0] - 4, tip[1] + 2), (cx - 4, cy - 18), 16, R)
    out += _blade(tip, (cx, cy - 16), 13, B)
    out += _blade((302, 96), (238, 292), 3.4, F)
    # Iron cup (filled disk) then inner evenodd to show plate, plus a left amber crescent.
    out += C(cx, cy, 58, R)
    out += C(cx, cy, 36, F)  # open cup
    # Left lantern rim on the cup.
    out += P(f'M{f(cx-58)} {f(cy)} A{f(58)} {f(58)} 0 0 1 {f(cx)} {f(cy-58)} '
             f'L{f(cx)} {f(cy-44)} A{f(44)} {f(44)} 0 0 0 {f(cx-44)} {f(cy)} Z', A, 0.7)
    # Knuckle-bow: a thick iron arc from cup to pommel (filled band via stroke-like path).
    px, py = pommel
    out += P(f'M{f(cx+40)} {f(cy+20)} Q{f(cx+86)} {f((cy+py)/2+10)} {f(px+16)} {f(py-8)} '
             f'L{f(px+4)} {f(py-2)} Q{f(cx+70)} {f((cy+py)/2+16)} {f(cx+28)} {f(cy+28)} Z', R)
    out += _hilt((cx, cy + 40), pommel, 11, W)
    out += C(px, py, 16, R) + C(px - 2, py - 3, 6, A)
    return out


def staff_glyph():
    """Vertical focus staff / quarterstaff / rod, caged amber at the head."""
    out = ''
    # Shaft (slightly right of centre so the head cage has room).
    x = 256
    out += poly([(x - 22, 168), (x + 22, 168), (x + 18, 430), (x - 18, 430)], W)
    # Left amber rim light.
    out += poly([(x - 22, 168), (x - 14, 168), (x - 12, 430), (x - 18, 430)], A, 0.55)
    # Wrap bands.
    for y, col in ((220, R), (280, S), (340, R), (400, S)):
        out += poly([(x - 26, y - 10), (x + 26, y - 10), (x + 24, y + 10), (x - 24, y + 10)], col)
    # Ferrule.
    out += poly([(x - 20, 424), (x + 20, 424), (x + 16, 456), (x - 16, 456)], R)
    out += C(x, 456, 8, ASH)
    # Iron cage around an ember (wizard tell).
    out += C(x, 126, 64, A, 0.10) + C(x, 126, 36, A, 0.16)
    # Cage ribs as a thick ring + four spokes, evenodd window.
    out += ring(x, 126, 58, 10, R)
    out += poly([(x - 8, 72), (x + 8, 72), (x + 8, 180), (x - 8, 180)], R)
    out += poly([(x - 54, 118), (x + 54, 118), (x + 54, 134), (x - 54, 134)], R)
    # Ember core inside the cage.
    out += flame(x, 148, 1.55, A, AC, glow=False)
    out += C(x, 132, 10, AC)
    # Collar under the cage.
    out += poly([(x - 30, 176), (x + 30, 176), (x + 24, 196), (x - 24, 196)], R)
    out += C(x, 186, 6, A)
    return out


def ashen_lantern_glyph():
    """Story weapon: the Ashen Lantern. Front view, four panes, amber flame."""
    out = ''
    # Glow first, so evenodd panes reveal it.
    out += C(256, 290, 120, A, 0.12) + C(256, 300, 70, A, 0.20)
    # Bail handle ring.
    out += ring(256, 86, 38, 11, R)
    out += C(256, 86, 8, A)
    # Bail arms.
    out += poly([(222, 108), (236, 108), (200, 168), (178, 168)], R)
    out += poly([(290, 108), (276, 108), (312, 168), (334, 168)], R)
    # Cap.
    out += poly([(160, 176), (256, 136), (352, 176), (336, 200), (176, 200)], R)
    out += poly([(200, 172), (256, 148), (312, 172), (300, 186), (212, 186)], ASH)
    out += C(256, 158, 8, A)
    # Body with four pane cuts.
    body = (f'M176 200 L336 200 L322 392 L190 392 Z ')
    panes = (
        f'M194 218 L246 218 L246 288 L194 288 Z '
        f'M266 218 L318 218 L318 288 L266 288 Z '
        f'M194 312 L246 312 L242 374 L198 374 Z '
        f'M266 312 L318 312 L314 374 L270 374 Z'
    )
    out += P(body + panes, W, rule=True)
    # Thin left highlight on the frame (not a heavy bar).
    out += poly([(176, 200), (184, 200), (196, 392), (190, 392)], A, 0.55)
    # Mullions.
    out += poly([(250, 212), (262, 212), (258, 378), (246, 378)], R)
    out += poly([(188, 294), (324, 294), (322, 310), (190, 310)], R)
    # Flame drawn AFTER the frame so it sits in the open panes.
    out += flame(220, 276, 1.15, A, AC, glow=False)
    out += flame(292, 276, 1.15, A, AC, glow=False)
    out += flame(220, 368, 1.05, A, AC, glow=False)
    out += flame(292, 368, 1.05, A, AC, glow=False)
    # Base.
    out += poly([(190, 392), (322, 392), (336, 428), (176, 428)], R)
    out += poly([(210, 400), (302, 400), (308, 416), (204, 416)], ASH)
    out += C(256, 408, 6, A)
    return out


# ---------------------------------------------------------------------------
# Armor
# ---------------------------------------------------------------------------

def leather_glyph():
    """Front-laced jerkin / leathers / padded vest — not a face."""
    out = ''
    # Two breast panels with a centre lace gap, round neck (collar, not a V-hole).
    # Left panel.
    out += poly([(158, 168), (198, 128), (236, 140), (236, 368), (180, 352), (164, 250)], ASH)
    # Right panel.
    out += poly([(354, 168), (314, 128), (276, 140), (276, 368), (332, 352), (348, 250)], ASH)
    # Hip tabs under the belt line.
    out += poly([(180, 352), (236, 368), (236, 404), (188, 388)], ASH)
    out += poly([(332, 352), (276, 368), (276, 404), (324, 388)], ASH)
    # Thin left lantern rim.
    out += poly([(158, 168), (198, 128), (206, 138), (172, 174), (176, 350), (164, 250)], A, 0.40)
    # Round collar / gorget band.
    out += poly([(198, 128), (256, 116), (314, 128), (300, 156), (256, 148), (212, 156)], R)
    out += C(256, 134, 5, A)
    # Shoulder caps.
    out += poly([(158, 168), (198, 128), (210, 148), (176, 196)], R)
    out += poly([(354, 168), (314, 128), (302, 148), (336, 196)], R)
    # Laces down the gap, stopping at the belt.
    out += poly([(244, 160), (268, 160), (268, 316), (244, 316)], F)
    for y in (184, 224, 264, 300):
        out += poly([(238, y - 6), (274, y + 4), (274, y + 10), (238, y)], B)
    # Belt.
    out += poly([(176, 316), (336, 316), (332, 350), (180, 350)], W)
    out += rrect(236, 310, 40, 44, 4, R)
    out += C(256, 332, 8, A)
    # Skirt panel closes the centre under the belt.
    out += poly([(180, 350), (332, 350), (324, 396), (256, 412), (188, 396)], ASH)
    return out


def mail_glyph():
    """Chain / scale / reinforced mail shirt — large rings, not tiny links."""
    out = ''
    # Solid torso, no face-like neck hole.
    out += poly([(150, 170), (200, 128), (256, 118), (312, 128), (362, 170),
                 (354, 250), (344, 360), (256, 390), (168, 360), (158, 250)], R)
    # Scale hem: three large overlapping scales.
    for x, y in ((200, 378), (256, 396), (312, 378)):
        out += P(f'M{f(x-36)} {f(y-16)} L{f(x+36)} {f(y-16)} '
                 f'Q{f(x+36)} {f(y+28)} {f(x)} {f(y+44)} Q{f(x-36)} {f(y+28)} {f(x-36)} {f(y-16)} Z', ASH)
        out += C(x, y, 6, F)
    # Three big chain rings (the tell at 48px).
    for cx, cy, rr in ((200, 250, 28), (256, 236, 32), (312, 250, 28)):
        out += ring(cx, cy, rr, 8, FOG)
    # Thin left lantern rim.
    out += poly([(150, 170), (200, 128), (208, 140), (166, 176), (168, 360), (158, 250)], A, 0.35)
    # Round collar band sitting on top of the neckline.
    out += poly([(208, 128), (256, 112), (304, 128), (296, 160), (256, 150), (216, 160)], W)
    out += C(256, 134, 6, A)
    return out


def plate_glyph(glow=True):
    """Heavy plate / aegis. Legendary sample: lantern sigil + static glow. Not a face."""
    out = ''
    if glow:
        out += C(256, 260, 120, A, 0.10) + C(256, 260, 64, A, 0.16)
    # Angular pauldrons (shoulders, not ears) sitting at the sides.
    out += poly([(88, 196), (120, 140), (200, 148), (210, 200), (196, 236), (100, 230)], R)
    out += poly([(424, 196), (392, 140), (312, 148), (302, 200), (316, 236), (412, 230)], R)
    out += C(136, 168, 7, A)
    # Cuirass — closed neck, no V-hole.
    out += poly([(176, 160), (256, 132), (336, 160), (358, 260),
                 (338, 360), (256, 392), (174, 360), (154, 260)], FOG)
    # Gorget: a horizontal collar sitting on the neckline.
    out += poly([(200, 136), (256, 118), (312, 136), (320, 170), (256, 158), (192, 170)], R)
    out += C(256, 144, 6, A)
    # Breast keel.
    out += poly([(248, 176), (264, 176), (258, 340), (246, 340)], ASH)
    # Thin left lantern rim.
    out += poly([(176, 160), (256, 132), (256, 144), (188, 168), (174, 360), (154, 260)], A, 0.32)
    # Lantern sigil only (no floating wick — that turned the breastplate into a face).
    out += poly([(230, 220), (256, 206), (282, 220), (276, 286), (236, 286)], R)
    out += P(f'M240 226 L272 226 L268 274 L244 274 Z '
             f'M250 234 L262 234 L260 256 L252 256 Z', A, rule=True)
    out += flame(256, 270, 0.85, A, AC, glow=False)
    out += poly([(236, 286), (276, 286), (270, 300), (242, 300)], R)
    # Faulds: two horizontal lames, not a jaw.
    out += poly([(174, 348), (338, 348), (348, 376), (164, 376)], R)
    out += poly([(180, 376), (332, 376), (342, 404), (170, 404)], ASH)
    out += poly([(200, 356), (312, 356), (310, 368), (202, 368)], FOG)
    return out


# ---------------------------------------------------------------------------
# Consumables and keys
# ---------------------------------------------------------------------------

def potion_glyph():
    """Round healing draught flask, cork, bone-wax liquor."""
    out = ''
    # Cork.
    out += poly([(230, 86), (282, 86), (274, 140), (238, 140)], W)
    out += poly([(236, 86), (276, 86), (274, 104), (238, 104)], B)
    # Neck + lip.
    out += poly([(236, 140), (276, 140), (276, 176), (236, 176)], R)
    out += poly([(226, 168), (286, 168), (286, 188), (226, 188)], R)
    # Bottle body (circle) with a highlight cut.
    out += C(256, 310, 118, ASH)
    # Liquid meniscus — evenodd of bottle ∩ liquid would need a clip; draw liquid as a
    # lower circle-segment path instead.
    out += P(f'M150 310 A106 106 0 1 0 362 310 L348 300 Q256 268 164 300 Z', B)
    out += P(f'M170 318 Q256 292 342 318 Q256 300 170 318 Z', AC, 0.45)
    # Glass highlight (left, lantern side).
    out += P(f'M176 250 Q168 310 184 370 Q196 310 188 250 Q182 230 176 250 Z', B, 0.28)
    # Iron band.
    out += poly([(148, 236), (364, 236), (360, 258), (152, 258)], R)
    out += C(256, 247, 7, A)
    return out


def tonic_glyph():
    """Wayfarer's Tonic — tall vial, amber liquor, wick cap."""
    out = ''
    # Wick cap (the tell).
    out += wick(256, 96, 1.35, glow=True)
    out += poly([(230, 118), (282, 118), (276, 152), (236, 152)], W)
    out += C(256, 128, 6, R)
    # Neck.
    out += poly([(238, 152), (274, 152), (274, 188), (238, 188)], R)
    # Tall vial body with a slight hex (chamfered) top and bottom.
    body = (f'M196 188 L316 188 L336 220 L328 400 L300 428 L212 428 L184 400 L176 220 Z')
    out += P(body, ASH)
    # Amber liquid.
    out += P(f'M184 250 L328 250 L328 400 L300 428 L212 428 L184 400 Z', A)
    out += P(f'M192 250 L320 250 L316 270 Q256 256 196 270 Z', AC, 0.55)
    # Left highlight.
    out += poly([(196, 220), (214, 220), (208, 400), (190, 388)], B, 0.22)
    # Paper wrap / label band.
    out += poly([(178, 300), (334, 300), (332, 348), (180, 348)], W)
    out += poly([(220, 312), (292, 312), (292, 336), (220, 336)], F)
    out += C(256, 324, 7, A)
    return out


def raid_key_glyph():
    """Raid Key — iron signet-key, lantern-window bow, amber ember."""
    out = ''
    # Bow (left).
    out += C(168, 256, 110, A, 0.10)
    out += C(168, 256, 96, R)
    # Evenodd window: outer already filled; cut a lantern pane cross.
    # Inner disk of plate, then a diamond pane of amber.
    out += C(168, 256, 58, F)
    out += poly([(168, 208), (216, 256), (168, 304), (120, 256)], A)
    out += poly([(168, 224), (200, 256), (168, 288), (136, 256)], AC)
    # Small iron mullion cross on the pane.
    out += poly([(164, 228), (172, 228), (172, 284), (164, 284)], R)
    out += poly([(140, 252), (196, 252), (196, 260), (140, 260)], R)
    # Collar.
    out += poly([(250, 230), (286, 230), (286, 282), (250, 282)], ASH)
    # Shaft.
    out += poly([(278, 236), (430, 236), (430, 276), (278, 276)], FOG)
    out += poly([(278, 236), (294, 236), (294, 276), (278, 276)], A, 0.45)  # left rim
    # Bit (lantern-stepped teeth, hanging down).
    out += poly([(370, 276), (430, 276), (430, 292),
                 (430, 356), (402, 356), (402, 320),
                 (386, 320), (386, 344), (360, 344), (360, 292),
                 (370, 292)], R)
    # Tip cap.
    out += poly([(430, 228), (452, 236), (452, 276), (430, 284)], R)
    return out


# ---------------------------------------------------------------------------
# Public catalogue
# ---------------------------------------------------------------------------

ICONS = [
    # filename, display name, rarity, builder
    ('sword_common',        'Sword · Common',           'common',    lambda: sword_glyph(False)),
    ('short_blade_rare',    'Short blade · Rare',       'rare',      short_blade_glyph),
    ('rapier_uncommon',     'Rapier · Uncommon',        'uncommon',  rapier_glyph),
    ('staff_epic',          'Staff · Epic',             'epic',      staff_glyph),
    ('ashen_lantern_rare',  'Ashen Lantern · Unique',   'rare',      ashen_lantern_glyph),
    ('sword_legendary',     'Crownbreaker · Legendary', 'legendary', lambda: sword_glyph(True, True)),
    ('sword_legendary_calm','Crownbreaker · Calm',      'legendary', lambda: sword_glyph(True, False)),
    ('leather_common',      'Leathers · Common',        'common',    leather_glyph),
    ('mail_uncommon',       'Mail · Uncommon',          'uncommon',  mail_glyph),
    ('plate_legendary',     'Aegis · Legendary',        'legendary', lambda: plate_glyph(True)),
    ('potion_common',       'Healing Draught · Common', 'common',    potion_glyph),
    ('tonic_uncommon',      "Wayfarer's Tonic · Uncommon", 'uncommon', tonic_glyph),
    ('raid_key',            'Raid Key',                 'rare',      raid_key_glyph),
]


def body_for(name):
    for fn, _label, _r, builder in ICONS:
        if fn == name:
            return plate() + builder()
    raise KeyError(name)


def write_all(out_dir):
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    written = []
    for fn, label, rarity, builder in ICONS:
        text = svg(plate() + builder(), fn)
        path = out_dir / f'{fn}.svg'
        path.write_text(text)
        written.append((fn, label, rarity, path))
    return written


if __name__ == '__main__':
    dest = Path(__file__).resolve().parent
    for fn, label, rarity, path in write_all(dest):
        print(f'wrote {path.name}  ({label})')
