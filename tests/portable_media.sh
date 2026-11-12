#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
# Optional isolated extracted dependency root, containing include/ and lib/.
mkdir -p build/portable
if [ -n "${MEDIA_DEPS:-}" ]; then
    export LD_LIBRARY_PATH="$MEDIA_DEPS/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    c++ -std=c++20 -Wall -Wextra -Werror -g -I. -I"$MEDIA_DEPS/include" \
        -I"$MEDIA_DEPS/include/x86_64-linux-gnu" support/AnimatedImage.cpp \
        support/MediaLibrary.cpp support/ImageSafety.cpp tests/media.cpp \
        -L"$MEDIA_DEPS/lib/x86_64-linux-gnu" -lgif -lwebp -lwebpdemux -l:libcrypto.so.3 \
        -o build/portable/media-test
else
    c++ -std=c++20 -Wall -Wextra -Werror -g -I. support/AnimatedImage.cpp \
        support/MediaLibrary.cpp support/ImageSafety.cpp tests/media.cpp \
        -lgif -lwebp -lwebpdemux -lcrypto -o build/portable/media-test
fi
build/portable/media-test
