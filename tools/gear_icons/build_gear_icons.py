#!/usr/bin/env python3
"""Regenerate the in-app gear glyphs.

  python3 tools/gear_icons/build_gear_icons.py

Writes docs/art/ash_ember/gear/ic_gear_*.svg and converts each with tools/svg2vd.py
into app/src/main/res/drawable/ic_gear_*.xml (48dp intrinsic size).
Calm legendary variants come from their own no-glow SVGs rather than svg2vd --calm,
because --calm would also dim the amber inlay (approved rec 10: calm keeps the inlay).
"""
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import gear_glyphs  # noqa: E402

SVG_DIR = ROOT / 'docs/art/ash_ember/gear'
RES = ROOT / 'app/src/main/res/drawable'


def main():
    SVG_DIR.mkdir(parents=True, exist_ok=True)
    for stem, text in gear_glyphs.all_outputs():
        svg = SVG_DIR / f'{stem}.svg'
        svg.write_text(text)
        out = RES / f'{stem}.xml'
        subprocess.run([sys.executable, str(ROOT / 'tools/svg2vd.py'), str(svg), str(out)], check=True)
        xml = out.read_text()
        xml = re.sub(r'android:width="256dp"', 'android:width="48dp"', xml, count=1)
        xml = re.sub(r'android:height="256dp"', 'android:height="48dp"', xml, count=1)
        out.write_text(xml)
        print('wrote', out.relative_to(ROOT))
        if stem == 'ic_gear_potion':
            # Disabled state for the combat Potion button (selector ic_gear_potion_button.xml).
            dim = RES / 'ic_gear_potion_disabled.xml'
            dim.write_text(xml.replace('android:height="48dp"', 'android:height="48dp"\n    android:alpha="0.38"', 1))
            print('wrote', dim.relative_to(ROOT))


if __name__ == '__main__':
    main()
