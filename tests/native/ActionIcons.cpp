// Verify the exact installed package contains usable alpha icon resources.
#include <Application.h>
#include <Bitmap.h>
#include <DataIO.h>
#include <File.h>
#include <Resources.h>
#include <TranslationUtils.h>
#include <cstdio>
#include <memory>

int main(int argc, char** argv)
{
    BApplication app("application/x-vnd.Kambuzi-ActionIconTests");
    if (argc != 2) return 2;
    BFile file(argv[1], B_READ_ONLY);
    BResources resources;
    if (file.InitCheck() != B_OK || resources.SetTo(&file) != B_OK) return 2;
    const char* names[] = {"conversations", "contacts", "groups", "emoji",
        "attachment", "send", "settings", "gif", "sticker", "sidebar",
        "sidebar-expand", "participants", "status-ready", "status-offline", "status-limited", "status-connecting"};
    int passed = 0;
    for (const auto* name : names) for (int size : {20, 24, 28, 48, 64, 96}) {
        char key[96]; std::snprintf(key, sizeof(key), "%s-%d", name, size);
        size_t length = 0;
        const void* data = resources.LoadResource('RICO', key, &length);
        if (!data || !length) { std::fprintf(stderr, "Missing resource %s\n", key); return 1; }
        BMemoryIO stream(data, length);
        std::unique_ptr<BBitmap> bitmap(BTranslationUtils::GetBitmap(&stream));
        if (!bitmap || bitmap->InitCheck() != B_OK || bitmap->Bounds() != BRect(0, 0, size-1, size-1)
            || bitmap->ColorSpace() != B_RGBA32) {
            std::fprintf(stderr, "Invalid image %s\n", key); return 1;
        }
        int transparent = 0, opaque = 0;
        for (int y=0; y<size; ++y) for (int x=0; x<size; ++x) {
            const auto* row = static_cast<const uint8*>(bitmap->Bits()) + y * bitmap->BytesPerRow();
            transparent += row[4*x+3] == 0;
            opaque += row[4*x+3] > 240;
        }
        if (!transparent || !opaque) { std::fprintf(stderr, "Empty/opaque icon %s\n", key); return 1; }
        ++passed;
    }
    std::printf("PASS: %d packaged icon resources decoded with correct size and transparency\n", passed);
    return 0;
}
