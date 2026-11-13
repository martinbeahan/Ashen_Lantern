"""Ash & Ember SVG helper lib — emits only VectorDrawable-safe primitives
(<path>, <circle>, flat fills, fill-opacity, evenodd, simple strokes). No transforms."""
import math, re

S, F, V, W, R = '#1C1A1E', '#141216', '#0B0A0C', '#2A262C', '#3A3640'
ASH, FOG, B = '#4A4650', '#6E6872', '#D9D2C3'
T, A, AC, EM, OB = '#5FD3C6', '#F2A541', '#FFD27A', '#E0532B', '#7A1E22'

def f(v):
    return ('%.1f' % v).rstrip('0').rstrip('.')

def P(d, fill=S, op=None, rule=None):
    a = f'<path fill="{fill}"'
    if op is not None: a += f' fill-opacity="{op}"'
    if rule: a += ' fill-rule="evenodd"'
    return a + f' d="{d}"/>'

def C(cx, cy, r, fill, op=None):
    a = f'<circle cx="{f(cx)}" cy="{f(cy)}" r="{f(r)}" fill="{fill}"'
    if op is not None: a += f' fill-opacity="{op}"'
    return a + '/>'

def STK(d, col=R, w=2.5, op=None):
    a = f'<path fill="none" stroke="{col}" stroke-width="{w}" stroke-linecap="round" stroke-linejoin="round"'
    if op is not None: a += f' stroke-opacity="{op}"'
    return a + f' d="{d}"/>'

def rim(d, w=2.5):
    return STK(d, R, w)

def circ_d(cx, cy, r, ry=None):
    ry = r if ry is None else ry
    return f'M{f(cx-r)} {f(cy)} a{f(r)},{f(ry)} 0 1,0 {f(2*r)},0 a{f(r)},{f(ry)} 0 1,0 {f(-2*r)},0 Z'

def ring(cx, cy, r, t, fill=S, op=None, ry=None, ty=None):
    ry = r if ry is None else ry
    ty = t if ty is None else ty
    return P(circ_d(cx, cy, r, ry) + ' ' + circ_d(cx, cy, r - t, ry - ty), fill, op, rule=True)

def eye(cx, cy, r=3.5, g=11, op=0.14):
    return C(cx, cy, g, T, op) + C(cx, cy, r, T)

def slit(x, y, w=14, h=4, up=6):
    """slanted glowing slit; up = rise toward its inner end"""
    return (C(x + w/2, y - up/2, w*0.8, T, 0.13) +
            P(f'M{f(x)} {f(y+h)} L{f(x+w)} {f(y-up)} L{f(x+w+1)} {f(y-up+h)} L{f(x+3)} {f(y+h+3)} Z', T))

def flame(cx, cy, s=1.0, col=A, core=AC, glow=True):
    """teardrop flame, base centre at (cx,cy), height 28*s"""
    out = ''
    if glow:
        out += C(cx, cy - 12*s, 30*s, col, 0.08) + C(cx, cy - 11*s, 16*s, col, 0.16)
    out += P(f'M{f(cx)} {f(cy-28*s)} Q{f(cx+12*s)} {f(cy-10*s)} {f(cx+7*s)} {f(cy-2*s)} '
             f'Q{f(cx)} {f(cy+5*s)} {f(cx-7*s)} {f(cy-2*s)} Q{f(cx-12*s)} {f(cy-10*s)} {f(cx)} {f(cy-28*s)} Z', col)
    if core:
        out += P(f'M{f(cx)} {f(cy-14*s)} Q{f(cx+5*s)} {f(cy-6*s)} {f(cx+3*s)} {f(cy-1*s)} '
                 f'Q{f(cx)} {f(cy+1.5*s)} {f(cx-3*s)} {f(cy-1*s)} Q{f(cx-5*s)} {f(cy-6*s)} {f(cx)} {f(cy-14*s)} Z', core)
    return out

def tf(d, dx=0, dy=0, s=1.0, ox=0, oy=0, flipx=False):
    """transform an absolute M/L/Q/C/Z path: scale s about (ox,oy), optional mirror, then translate"""
    toks = re.findall(r'[MLQCZ]|-?\d+\.?\d*', d)
    out, xy = [], 0
    for t in toks:
        if t in 'MLQCZ':
            out.append(t); continue
        v = float(t)
        if xy == 0:
            v = (v - ox) * s
            v = -v if flipx else v
            v = v + ox + dx
        else:
            v = (v - oy) * s + oy + dy
        out.append(f(v)); xy ^= 1
    return ' '.join(out).replace(' Z', ' Z')

def _cr(pts, n=8):
    """Catmull-Rom sample of polyline -> list of points"""
    p = [pts[0]] + pts + [pts[-1]]
    out = []
    for i in range(1, len(p) - 2):
        p0, p1, p2, p3 = p[i-1], p[i], p[i+1], p[i+2]
        for k in range(n):
            t = k / n; t2, t3 = t*t, t*t*t
            out.append(tuple(0.5 * ((2*p1[j]) + (-p0[j] + p2[j]) * t +
                                   (2*p0[j] - 5*p1[j] + 4*p2[j] - p3[j]) * t2 +
                                   (-p0[j] + 3*p1[j] - 3*p2[j] + p3[j]) * t3) for j in (0, 1)))
    out.append(pts[-1])
    return out

def band(pts, w0, w1, fill=S, op=None, n=8, smooth=True, tip=None):
    """tapered limb/neck along points; width w0 -> w1; returns path"""
    q = _cr(pts, n) if smooth else pts
    L, Rr = [], []
    m = len(q)
    for i, (x, y) in enumerate(q):
        a = q[max(0, i-1)]; b = q[min(m-1, i+1)]
        dx, dy = b[0]-a[0], b[1]-a[1]; ln = math.hypot(dx, dy) or 1
        nx, ny = -dy/ln, dx/ln
        w = (w0 + (w1 - w0) * i / (m - 1)) / 2
        L.append((x + nx*w, y + ny*w)); Rr.append((x - nx*w, y - ny*w))
    pts2 = L + ([tip] if tip else []) + Rr[::-1]
    d = 'M' + ' L'.join(f'{f(x)} {f(y)}' for x, y in pts2) + ' Z'
    return P(d, fill, op)

def poly(pts, fill=S, op=None):
    return P('M' + ' L'.join(f'{f(x)} {f(y)}' for x, y in pts) + ' Z', fill, op)

def flakes(pts, fill=F, op=None, s=5):
    out = ''
    for (x, y) in pts:
        out += P(f'M{f(x)} {f(y)} L{f(x+s)} {f(y-s*0.3)} L{f(x+s*1.1)} {f(y+s*0.8)} L{f(x+s*0.1)} {f(y+s)} Z', fill, op)
    return out

def chain(p0, p1, p2, n, r=6, t=2.2, fill=S):
    """links along a quadratic curve"""
    out = ''
    for i in range(n):
        u = i / (n - 1) if n > 1 else 0
        x = (1-u)**2*p0[0] + 2*(1-u)*u*p1[0] + u*u*p2[0]
        y = (1-u)**2*p0[1] + 2*(1-u)*u*p1[1] + u*u*p2[1]
        if i % 2 == 0: out += ring(x, y, r, t, fill)
        else: out += ring(x, y, r*0.75, t, fill, ry=r*1.1, ty=t)
    return out

def svg(body, title):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">\n'
            f'  <!-- {title} — Ashen Lantern original (Ash & Ember). Faces left, feet ~y478. '
            f'VectorDrawable-safe: paths/circles, flat fills + fill-opacity, evenodd, simple strokes. -->\n'
            f'{body}\n</svg>\n')
