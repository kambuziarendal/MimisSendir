#!/usr/bin/env python3
"""Rebuild our local GIF starter pack and common reaction raster glyphs.
Requires Pillow and Debian fonts-noto-color-emoji (OFL-1.1). No network.
"""
from pathlib import Path
import re,math
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parents[1]
font=ImageFont.truetype('/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf',109)
textfont=ImageFont.truetype('/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf',21)
def emoji(text):
 im=Image.new('RGBA',(180,160));ImageDraw.Draw(im).text((0,0),text,font=font,embedded_color=True)
 return im.crop(im.getbbox())
values=re.findall(r'\{"([^"]+)",',(root/'support/MediaLibrary.cpp').read_text().split('Emojis()')[1])
values=list(dict.fromkeys(values+['👍','❤️','😂','😮','😢','🙏','🔥','🎉','👎','✅']))
lines=['// Generated from Noto Color Emoji (OFL-1.1). See resources/fonts.','struct EmojiPixels { const char* text; unsigned char bgra[1600]; };','static const EmojiPixels kEmojiPixels[] = {']
for text in values:
 im=emoji(text);im.thumbnail((20,20),Image.Resampling.LANCZOS);out=Image.new('RGBA',(20,20));out.paste(im,((20-im.width)//2,(20-im.height)//2))
 lines.append('{"'+text+'",{'+','.join(str(x) for x in out.tobytes('raw','BGRA'))+'}},')
lines.append('};');(root/'ui/ColorEmojiData.h').write_text('\n'.join(lines)+'\n')
pack=[('Enig','👍',(226,242,235)),('Takk','🙏',(245,232,205)),('Haha','😂',(255,238,184)),('Hjerte','❤️',(252,220,228)),('Feiring','🎉',(228,221,252)),('Pizza','🍕',(250,228,203)),('Skaal','🍺',(249,234,191)),('Klem','🤗',(230,240,218))]
for name,text,bg in pack:
 frames=[];glyph=emoji(text)
 for i in range(24):
  phase=2*math.pi*i/24;size=round(105+8*math.sin(phase))
  g=glyph.copy();g.thumbnail((size,size),Image.Resampling.LANCZOS);g=g.rotate(6*math.sin(phase),Image.Resampling.BICUBIC,expand=True)
  frame=Image.new('RGB',(192,192),bg);frame.paste(g,((192-g.width)//2,round(17+7*math.sin(phase))),g)
  d=ImageDraw.Draw(frame);label='Skål!' if name=='Skaal' else name+'!';d.text((96,159),label,font=textfont,anchor='mm',fill=(40,32,53))
  frames.append(frame)
 frames[0].save(root/'resources/gifs'/f'{name}.gif',save_all=True,append_images=frames[1:],duration=70,loop=0,disposal=2)
print('Generated',len(pack),'local GIFs and',len(values),'reaction glyphs')
