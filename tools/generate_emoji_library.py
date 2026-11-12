#!/usr/bin/env python3
"""Generate the offline Unicode 17 picker and embedded Noto color atlas.

Inputs are pinned downloads, not host fonts. Requires Pillow with RAQM.
No network requests are made by this generator or by the resulting picker.
"""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

DATA_SHA = '1d8a944f88d7952f7ef7c5167fef3c67995bcae24543949710231b03a201acda'
FONT_SHA = '15671215ab769fdc7162a045d56fd7d7e477c51b04e6b3c761d914d8fdd6cc44'
ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('unicode_data', type=Path)
    parser.add_argument('noto_font', type=Path)
    args = parser.parse_args()
    for path, expected in [(args.unicode_data, DATA_SHA), (args.noto_font, FONT_SHA)]:
        assert hashlib.sha256(path.read_bytes()).hexdigest() == expected, str(path)
    entries = []
    category = ''
    for line in args.unicode_data.read_text().splitlines():
        if line.startswith('# group: '):
            category = line.split(': ', 1)[1]
        if '; fully-qualified' not in line:
            continue
        codes, annotation = line.split(';', 1)
        text = ''.join(chr(int(code, 16)) for code in codes.split())
        name = annotation.split('#', 1)[1].strip().split(' ', 2)[2]
        entries.append((text, name, category))
    assert len(entries) == 3944 and len({e[0] for e in entries}) == len(entries)
    (ROOT / 'resources/emoji').mkdir(exist_ok=True)
    quote = lambda text: json.dumps(text, ensure_ascii=False)
    header = '// Generated Unicode 17.0 data. See resources/emoji/SOURCES.md.\n'
    (ROOT / 'support/EmojiData.inc').write_text(header + ''.join(
        '{' + ', '.join(map(quote, entry)) + '},\n' for entry in entries))
    font = ImageFont.truetype(str(args.noto_font), 109)
    width = font.getlength('😀')
    atlas = bytearray()
    index = []
    for text, _, _ in sorted(entries):
        # A supported sequence must shape as one emoji, not separate components.
        assert font.getlength(text) <= width * 1.05, text
        canvas = Image.new('RGBA', (136, 128))
        ImageDraw.Draw(canvas).text((0, 0), text, font=font, embedded_color=True)
        assert canvas.getchannel('A').getbbox(), text
        glyph = canvas.resize((24, 24), Image.Resampling.LANCZOS)
        index.append((text, len(atlas)))
        atlas.extend(glyph.tobytes('raw', 'BGRA'))
    (ROOT / 'resources/emoji/emoji.bgra').write_bytes(atlas)
    (ROOT / 'ui/ColorEmojiData.h').write_text(header +
        'struct EmojiPixels { const char* text; size_t offset; };\n' +
        'static constexpr EmojiPixels kEmojiPixels[] = {\n' + ''.join(
            '{' + quote(text) + ', ' + str(offset) + '},\n' for text, offset in index) + '};\n')
    print(f'{len(entries)} emoji, {len(atlas)} atlas bytes, SHA256 {hashlib.sha256(atlas).hexdigest()}')


if __name__ == '__main__':
    main()
