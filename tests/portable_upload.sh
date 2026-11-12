#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/portable
# Linux-only isolated dependency extraction, not an installation into the host.
deps="$(pwd)/.local-deps/root/usr"
if [ -d "$deps/include/gloox" ]; then
    export LD_LIBRARY_PATH="$deps/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    c++ -std=c++20 -Wall -Wextra -Werror -g -I. -I"$deps/include" \
        -I"$deps/include/x86_64-linux-gnu" network/HttpUpload.cpp network/HttpTransfer.cpp \
        tests/portable_upload.cpp -L"$deps/lib/x86_64-linux-gnu" \
        -lgloox -l:libcurl.so.4 -pthread -o build/portable/upload-test
else
    c++ -std=c++20 -Wall -Wextra -Werror -g -I. network/HttpUpload.cpp \
        network/HttpTransfer.cpp tests/portable_upload.cpp -lgloox -lcurl -pthread \
        -o build/portable/upload-test
fi
build/portable/upload-test
python3 tests/portable_https.py build/portable/upload-test
c++ -std=c++20 -Wall -Wextra -Werror -g -I. support/ImageSafety.cpp \
    tests/portable_image.cpp -o build/portable/image-test
build/portable/image-test

# Incoming metadata uses the same real gloox parser/types as the application.
if [ -d "$deps/include/gloox" ]; then
    c++ -std=c++20 -Wall -Wextra -Werror -g -I. -I"$deps/include" \
        network/HttpUpload.cpp network/AttachmentMetadata.cpp tests/attachments/metadata.cpp \
        -L"$deps/lib/x86_64-linux-gnu" -lgloox -o build/portable/attachment-metadata-test
else
    c++ -std=c++20 -Wall -Wextra -Werror -g -I. network/HttpUpload.cpp \
        network/AttachmentMetadata.cpp tests/attachments/metadata.cpp -lgloox \
        -o build/portable/attachment-metadata-test
fi
build/portable/attachment-metadata-test

# Exercise production lifecycle/cancel gates and exclusive save implementation.
for check in lifecycle cancellation save; do
    c++ -std=c++20 -Wall -Wextra -Werror -g -I. -pthread \
        "tests/attachments/$check.cpp" -o "build/portable/$check-test"
    "build/portable/$check-test"
done

c++ -std=c++20 -Wall -Wextra -Werror -g -I. support/ConversationStore.cpp tests/conversations.cpp -o build/portable/conversations-test
build/portable/conversations-test

if [ -d "$deps/include/gloox" ]; then
    c++ -std=c++20 -Wall -Wextra -Werror -g -I. -I"$deps/include" \
        network/bookmark2storage.cpp tests/bookmarks.cpp -L"$deps/lib/x86_64-linux-gnu" \
        -lgloox -o build/portable/bookmarks-test
else
    c++ -std=c++20 -Wall -Wextra -Werror -g -I. network/bookmark2storage.cpp \
        tests/bookmarks.cpp -lgloox -o build/portable/bookmarks-test
fi
build/portable/bookmarks-test

c++ -std=c++20 -Wall -Wextra -Werror -g -I. tests/delivery.cpp -o build/portable/delivery-test
build/portable/delivery-test
