"""Generate an illustrative SVG preview from the source theme."""
from pathlib import Path
import math
import re

root = Path(__file__).resolve().parents[1]
theme = (root / 'theme.h').read_text()
def constant(name):
    return float(re.search(r'#define ' + name + r' ([\d.]+)', theme)[1])
colors = [int(c, 16) for c in re.findall(r'0x([0-9a-fA-F]{6})', theme)]
count = int(constant('BAR_COUNT'))
width, height = 1440, 540
step = width / count
thickness = step * constant('BAR_WIDTH_RATIO')
svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-label="Hypr visualizer: pastel mirrored audio bars, bass in the center, fading at the bottom">', '<rect width="100%" height="100%" fill="#1e2044"/>', '<defs>']
for i in range(count):
    position = i / (count - 1) * (len(colors) - 1)
    a = int(position)
    b = min(a + 1, len(colors) - 1)
    t = position - a
    rgb = [round(((colors[a] >> shift) & 255) * (1 - t) + ((colors[b] >> shift) & 255) * t) for shift in (16, 8, 0)]
    color = '#' + ''.join(f'{v:02x}' for v in rgb)
    svg.append(f'<linearGradient id="b{i}" x1="0" y1="0" x2="0" y2="1"><stop stop-color="{color}" stop-opacity="0.6"/><stop offset="0.65" stop-color="{color}" stop-opacity="0.6"/><stop offset="1" stop-color="{color}" stop-opacity="0.132"/></linearGradient>')
svg.extend(['</defs>', '<text x="64" y="85" fill="#f3f0ff" font-family="sans-serif" font-size="38" font-weight="600">hypr-visualizer</text>', '<text x="66" y="124" fill="#b9bad3" font-family="sans-serif" font-size="18">Native audio bars for Hyprland</text>'])
for i in range(count):
    distance = abs(i - (count-1)/2) / ((count-1)/2)
    level = .24 + .67 * (1-distance) + .085 * math.sin(distance * 24)
    bar = max(12, level * 340)
    x = (i + .5) * step - thickness / 2
    svg.append(f'<rect x="{x:.2f}" y="{height-bar:.2f}" width="{thickness:.2f}" height="{bar+thickness:.2f}" rx="{thickness/2:.2f}" fill="url(#b{i})"/>')
svg.append('</svg>')
(root / 'docs/preview.svg').write_text('\n'.join(svg) + '\n')
