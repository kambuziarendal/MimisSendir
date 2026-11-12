# Offline emoji library

3944 fully-qualified Unicode 17.0 emoji, including flags, skin tones and ZWJ
sequences. The native binary embeds a 24px BGRA atlas so no emoji fetch or
system color-font support is needed. Favorites are stored per account.

Data: https://www.unicode.org/Public/17.0.0/emoji/emoji-test.txt
SHA256: 1d8a944f88d7952f7ef7c5167fef3c67995bcae24543949710231b03a201acda
License: Unicode-3.0, see LICENSE.txt.

Artwork: Noto Color Emoji, OFL-1.1, see ../fonts/NotoColorEmoji-LICENSE.txt.
Source: https://github.com/googlefonts/noto-emoji/tree/99701e07d3d1fe1bdbcdb14964547f0fd592966f
Font: 2D/fonts/NotoColorEmoji.ttf
SHA256: 15671215ab769fdc7162a045d56fd7d7e477c51b04e6b3c761d914d8fdd6cc44

Regenerate with tools/generate_emoji_library.py DATA FONT using Pillow/RAQM.
The generator verifies both inputs and rejects missing/non-ligated glyphs.
Atlas SHA256: d0ce6b2bbedc7d0c06590136bf73e5449a0cd9d785a91796380eaf9e94cb78f9.
