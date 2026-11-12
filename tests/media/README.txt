Synthetic fixtures generated with Pillow, not private/user media.
All fixtures are original test data licensed with the repository (MIT).
- animation.gif/webp: 8x6 red/green/blue frames, 100/200/300ms, loop.
- sticker.png: solid 8x6 red PNG.
- disposal.gif: transparent 8x6 canvas with changing red rectangles,
  disposal modes 1/3/2 and durations 10/20/300ms.
- disposal.rgba: Pillow-composited RGBA reference for the three GIF frames.
The decoder test compares independent reference pixels, timing and bounds.

Image-quality fixtures (native MediaPreview tests):
- clarity.png/jpg: original synthetic 1920x1080 grid, text and simple shapes.
- clarity-animation.gif: original 512x512, 24-frame moving rectangle.
  Verifies the larger conversation preview stays below its 8 MiB frame budget.
