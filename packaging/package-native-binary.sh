#!/bin/sh
# Package an already-built native binary; no compilation, install or publication.
set -eu
if [ "$(uname -s)" != Haiku ] || [ "$(uname -m)" != x86_64 ]; then
    echo 'This helper requires native Haiku x86_64.' >&2
    exit 1
fi
binary=${1:?Usage: sh packaging/package-native-binary.sh /absolute/mimissendir /absolute/output-directory}
output=${2:?Output directory required}
case "$binary:$output" in /*:/*) ;; *) echo 'Absolute paths required.' >&2; exit 1 ;; esac
if [ ! -f "$binary" ] || [ ! -x "$binary" ]; then
    echo 'A built executable is required. Build and smoke-test it first.' >&2
    exit 1
fi
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
stage="$output/package-stage"
artifact="$output/mimissendir-0.1.0~beta9-1-x86_64.hpkg"
if [ -e "$stage" ] || [ -e "$artifact" ]; then
    echo 'Refusing to overwrite existing package staging or artifact. Use a new output directory.' >&2
    exit 1
fi
mkdir -p "$stage/apps" "$stage/data/licenses" "$stage/documentation/packages/mimissendir"
mkdir -p "$stage/data/fonts/ttfonts"
cp "$repo/resources/fonts/NotoEmoji.ttf" "$stage/data/fonts/ttfonts/MimisSendir-NotoEmoji.ttf"
cp "$repo/resources/fonts/NotoEmoji-OFL.txt" "$stage/data/licenses/OFL-1.1"
mkdir -p "$stage/data/mimissendir"
cp -R "$repo/resources/gifs" "$stage/data/mimissendir/"
cp -R "$repo/resources/giphy" "$stage/data/mimissendir/"
cp "$repo/resources/fonts/Liberation-LICENSE.txt" "$stage/documentation/packages/mimissendir/"
cp "$binary" "$stage/apps/mimissendir"
mkdir -p "$stage/bin"
cp "$(dirname "$binary")/RengaStickerLink" "$stage/bin/mimissendir-sticker-link"
cp "$repo/README.md" "$stage/documentation/packages/mimissendir/"
cp -R "$repo/docs/public" "$stage/documentation/packages/mimissendir/"
cp "$repo/docs/GPL-3.0.txt" "$stage/data/licenses/GNU GPL v3"
cp "$repo/THIRD-PARTY-NOTICES.md" "$stage/documentation/packages/mimissendir/"
cp "$repo/third_party/gloox/LICENSE" "$stage/documentation/packages/mimissendir/gloox-LICENSE"
cp "$repo/LICENSE" "$stage/data/licenses/MIT"
cp "$repo/resources/emoji/LICENSE.txt" "$stage/data/licenses/Unicode-3.0"
cp "$repo/resources/emoji/SOURCES.md" "$stage/documentation/packages/mimissendir/EMOJI-SOURCES.md"
cp "$repo/resources/fonts/NotoColorEmoji-LICENSE.txt" "$stage/documentation/packages/mimissendir/"
cp "$repo/docs/DEPENDENCY-LICENSES.txt" "$stage/documentation/packages/mimissendir/"
cp -R "$repo/resources/Renga" "$stage/documentation/packages/mimissendir/"
cat > "$stage/.PackageInfo" <<'PACKAGE'
name mimissendir
version 0.1.0~beta9-1
architecture x86_64
summary "MímisSendir: native XMPP conversations and Signal bridge support"
description "MímisSendir early beta by Kambuzi EDB, based on Renga with recent conversations, compact attachments and persistent logout. AI-assisted programming with OpenAI Codex through OpenClaw. Public early-beta build for Haiku R1 beta6 x86_64. No hosted service or provider API key included."
packager "Kambuzi EDB"
vendor "Kambuzi EDB"
copyrights {
    "1999-2026 Renga contributors"
    "2026 Kambuzi EDB"
}
licenses { "GNU GPL v3" "MIT" "OFL-1.1" "Unicode-3.0" }
provides {
    mimissendir = 0.1.0~beta9
    app:mimissendir = 0.1.0~beta9
}
requires {
    haiku >= r1~beta6_hrev59866_79
    lib:libcrypto
    lib:libcss
    lib:libiconv
    lib:libgloox
    lib:libwapcaplet
    lib:libcurl
    lib:libgif
    lib:libwebp
    lib:libwebpdemux
}
conflicts {
    mimisendir
}
replaces { mimisendir }
urls { "https://github.com/kambuziarendal/MimisSendir" }
PACKAGE
package create -C "$stage" "$artifact"
printf 'Native package candidate: %s\n' "$artifact"
printf 'Installation and functional acceptance are still required.\n'
