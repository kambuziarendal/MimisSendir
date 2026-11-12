#!/usr/bin/env python3
"""Generate mimisendir's own faceted message icon as SVG and native Haiku HVIF.

The same integer polygon geometry drives both formats. No runtime converter.
Solid-color, closed line paths use Haiku's documented flat icon format.
Copyright 2026 Kambuzi EDB. MIT licensed.
"""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
# Back-to-front faces. Dark outlines are shapes, not scale-dependent strokes.
LAYERS = [
    ('26313e', [(4,22),(43,10),(60,22),(60,48),(20,60),(4,48)]),
    ('8c592c', [(21,39),(58,27),(58,47),(21,57)]),
    ('edb951', [(6,25),(19,34),(19,56),(6,47)]),
    ('f9d87c', [(7,23),(42,13),(57,23),(21,35)]),
    ('fff2bd', [(9,24),(42,15),(53,23),(21,32)]),
    ('c38b39', [(7,45),(17,36),(19,37),(19,53)]),
    ('ffd987', [(23,55),(39,36),(42,35),(56,46)]),
    ('26313e', [(23,8),(51,3),(61,10),(61,29),(47,32),(38,42),(38,34),(23,37)]),
    ('287daa', [(53,8),(59,12),(59,28),(45,31),(40,37),(40,29),(53,27)]),
    ('6bc5df', [(25,10),(51,6),(51,26),(39,28),(25,33)]),
    ('b9ecf5', [(26,11),(49,8),(49,12),(26,15)]),
    ('26313e', [(29,19),(33,18),(33,22),(29,23)]),
    ('26313e', [(37,17),(41,16),(41,20),(37,21)]),
    ('26313e', [(45,15),(49,14),(49,18),(45,19)]),
]
def main():
    colors = list(dict.fromkeys(color for color, _ in LAYERS))
    data = bytearray(b'ncif')
    data.append(len(colors))
    for color in colors: data.extend(bytes([3]) + bytes.fromhex(color))
    data.append(len(LAYERS))
    for _, points in LAYERS:
        data.extend([10, len(points)])  # closed + no curves
        for x, y in points:
            assert -32 <= x <= 95 and -32 <= y <= 95
            data.extend([x + 32, y + 32])
    data.append(len(LAYERS))
    for i, (color, _) in enumerate(LAYERS):
        data.extend([10, colors.index(color), 1, i, 0])
    (ROOT/'resources/icons/mimisendir.hvif').write_bytes(data)
    paths = '\n'.join('<polygon fill="#'+c+'" points="'+' '.join(f'{x},{y}' for x,y in p)+'"/>' for c,p in LAYERS)
    (ROOT/'resources/icons/mimisendir.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">\n'+paths+'\n</svg>\n')
    rdef = ROOT/'jabber/Resource.rdef'
    text = rdef.read_text(); start = text.index('resource("BEOS:ICON") vector_icon {'); end = text.index('};', start)+2
    h = data.hex().upper(); lines='\n'.join('\t$"'+h[i:i+64]+'"' for i in range(0,len(h),64))
    rdef.write_text(text[:start]+'resource("BEOS:ICON") vector_icon {\n'+lines+'\n};'+text[end:])
    print(f'mimisendir HVIF: {len(data)} bytes')

if __name__ == '__main__': main()
