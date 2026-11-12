#!/usr/bin/env python3
"""Regenerate bundled PNG sizes from our SVG masters. Requires CairoSVG.

The native build embeds committed PNGs, so Haiku needs no SVG renderer or
Python dependencies. Keep the SVG masters as the editable source of truth.
"""
from pathlib import Path
import cairosvg

root = Path(__file__).resolve().parents[1] / 'resources/icons/actions'
for source in sorted(root.glob('*.svg')):
    for size in (20, 24, 28, 48, 64, 96):
        cairosvg.svg2png(url=str(source), output_width=size, output_height=size,
                        write_to=str(source.with_name(f'{source.stem}-{size}.png')))
