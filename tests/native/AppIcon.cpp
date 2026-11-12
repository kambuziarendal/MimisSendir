// Verify the installed native icon and the MIME cache used by Deskbar.
#include <Application.h>
#include <Resources.h>
#include <AppFileInfo.h>
#include <Bitmap.h>
#include <File.h>
#include <IconUtils.h>
#include <MimeType.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <initializer_list>
int main(int argc, char** argv)
{
    BApplication app("application/x-vnd.Kambuzi-AppIconTests");
    if (argc != 2) return 2;
    BFile file(argv[1], B_READ_ONLY);
    BAppFileInfo info(&file);
    info.SetInfoLocation(B_USE_RESOURCES);
    version_info version;
    char signature[B_MIME_TYPE_LENGTH] = {};
    if (info.GetVersionInfo(&version, B_APP_VERSION_KIND) != B_OK
        || version.major != 0 || version.middle != 1 || version.minor != 0
        || version.variety != B_BETA_VERSION || version.internal != 3
        || info.GetSignature(signature) != B_OK
        || std::strcmp(signature, "application/x-vnd.Haiku-Jabber") != 0) {
        std::fprintf(stderr, "Missing or incorrect application version/signature resources\n");
        return 1;
    }
    std::printf("Version/signature: %s, %s\n", version.short_info, signature);
    uint8* icon = nullptr; size_t size = 0;
    auto status = info.GetIcon(&icon, &size);
    std::printf("GetIcon status=%ld size=%zu\n", (long)status, size);
    if (status != B_OK) {
        BResources resources(&file);
        type_code type; int32 id; const char* name; size_t length;
        for (int32 i = 0; resources.GetResourceInfo(i, &type, &id, &name, &length); ++i)
            if (name && std::strstr(name, "BEOS:")) std::printf("Resource %.4s id=%ld name=%s size=%zu\n", reinterpret_cast<char*>(&type), (long)id, name, length);
        return 1;
    }
    for (int pixels : {16, 32, 64}) {
        BBitmap bitmap(BRect(0, 0, pixels - 1, pixels - 1), B_RGBA32);
        status = BIconUtils::GetVectorIcon(icon, size, &bitmap);
        std::printf("Decode %d status=%ld\n", pixels, (long)status);
        if (status != B_OK) return 1;
        int opaque = 0, transparent = 0;
        for (int y = 0; y < pixels; ++y) for (int x = 0; x < pixels; ++x) {
            auto* row = static_cast<const uint8*>(bitmap.Bits()) + y * bitmap.BytesPerRow();
            opaque += row[x*4+3] > 240; transparent += row[x*4+3] == 0;
        }
        std::printf("Pixels opaque=%d transparent=%d\n", opaque, transparent);
        if (!opaque || !transparent) return 1;
    }
    uint8* cached = nullptr; size_t cachedSize = 0;
    BMimeType mime("application/x-vnd.Haiku-Jabber");
    const bool match = mime.GetIcon(&cached, &cachedSize) == B_OK && cachedSize == size
        && std::memcmp(icon, cached, size) == 0;
    std::free(icon); std::free(cached);
    std::printf("Native icon: 16/32/64 px decoded; Deskbar MIME cache matches=%s\n", match ? "yes" : "no");
    return match ? 0 : 1;
}
