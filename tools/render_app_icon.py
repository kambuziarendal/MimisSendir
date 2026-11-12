#!/usr/bin/env python3
"""Generate mimisendir's own speaking Mímir icon as SVG and native Haiku HVIF.

The same integer polygon geometry drives both formats. No runtime converter.
Solid-color, closed line paths use Haiku's documented flat icon format.
Copyright 2026 Kambuzi EDB. MIT licensed.
"""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
# Back-to-front faces. Dark outlines are shapes, not scale-dependent strokes.
LAYERS = [
    # A speaking head, with the beard becoming the speech-bubble tail.
    ('203e42', [(20,5),(41,5),(51,11),(57,22),(56,35),(50,44),(39,50),(16,60),(19,48),(11,40),(7,29),(10,16)]),
    ('357b7b', [(21,8),(40,8),(49,14),(54,23),(53,34),(47,42),(37,48),(20,55),(23,46),(14,38),(10,28),(13,18)]),
    ('64aaa0', [(21,8),(40,8),(47,12),(25,11),(16,19),(13,30),(10,28),(13,18)]),
    # Broad brow and cheeks. Faceted bronze keeps the small icon legible.
    ('bf8752', [(21,15),(42,15),(48,23),(46,35),(40,42),(25,42),(17,33),(16,24)]),
    ('e7bc7d', [(21,15),(32,13),(42,16),(44,25),(41,34),(25,36),(19,30),(18,23)]),
    ('f8db9e', [(22,17),(31,15),(39,17),(28,19),(21,24),(19,23)]),
    # A high forehead, silvered temples and calm, heavy brows.
    ('d6a368', [(25,18),(37,18),(39,19),(24,19)]),
    ('d6a368', [(26,20),(36,20),(36,21),(26,21)]),
    ('e4dcc0', [(18,21),(20,18),(20,29),(18,32),(16,26)]),
    ('c9c7ae', [(43,19),(46,23),(46,30),(44,33),(43,26)]),
    ('203e42', [(21,26),(28,25),(28,28),(24,29),(21,28)]),
    ('203e42', [(35,25),(43,26),(43,28),(39,29),(35,28)]),
    ('f2e6c5', [(21,24),(26,23),(29,25),(28,26),(23,26)]),
    ('f2e6c5', [(35,25),(38,23),(43,24),(43,26),(37,26)]),
    ('fff0c9', [(23,27),(25,27),(25,28),(23,28)]),
    ('fff0c9', [(39,27),(41,27),(41,28),(39,28)]),
    # Long nose and restrained mouth. Wisdom rather than a broad grin.
    ('a7673d', [(31,25),(34,25),(36,34),(32,35),(28,34)]),
    ('f8db9e', [(31,25),(33,25),(33,33),(30,33)]),
    # A full, pale bronze beard, with its point also forming the bubble tail.
    ('879b8f', [(18,32),(24,35),(29,36),(36,36),(42,33),(47,31),(46,41),(39,48),(29,52),(20,56),(23,46),(19,40)]),
    ('c9c7ae', [(19,34),(25,37),(31,38),(38,37),(45,34),(43,42),(37,47),(25,53),(27,46),(23,42)]),
    ('e4dcc0', [(25,39),(31,40),(37,39),(39,41),(34,47),(25,53),(29,45)]),
    ('203e42', [(27,37),(32,38),(38,37),(36,39),(30,40)]),
    ('f2e6c5', [(28,34),(32,36),(28,38),(22,39),(24,36)]),
    ('e4dcc0', [(33,36),(37,34),(42,36),(44,39),(38,38)]),

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
