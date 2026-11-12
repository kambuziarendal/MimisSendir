// Synthetic fixtures only. Run from the source directory.
#include "support/AnimatedImage.h"
#include "support/MediaLibrary.h"
#include "support/MessageFormatting.h"
#include "support/ReplyDraft.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
static int checks;
static void Check(bool ok, const char* name) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
}
static std::vector<uint8_t> Read(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
int main() {
    const std::string query = "https://www.facebook.com/events/123/?acontext=%7B%22action_history%22:%22null%22%7D&x=1#reply";
    auto links = Chat::FindLinks("See (" + query + "). More text.");
    Check(links.size() == 1 && links[0].target == query, "full query and fragment retained");
    Check(links[0].start == 5 && links[0].end == 5 + query.size(), "link byte offsets");
    links = Chat::FindLinks("https://example.test/a_(b) https://example.test/? https://example.test/?a=1&b=2");
    Check(links.size() == 3, "multiple URLs");
    Check(links[0].target == "https://example.test/a_(b)", "balanced parentheses preserved");
    Check(links[1].target == "https://example.test/?", "trailing query marker preserved");
    Check(links[2].target == "https://example.test/?a=1&b=2", "query separator retained");
    Check(Chat::FindLinks("www.example.test")[0].target == "https://www.example.test", "www normalized to HTTPS");
    Check(Chat::FindLinks("<HTTPS://example.test/>")[0].target == "HTTPS://example.test/", "case-insensitive scheme");
    Check(Chat::FindLinks("javascript:alert(1) file:///private mailto:a@b").empty(), "no unsupported scheme auto-linking");
    Check(Chat::FindLinks("nohttps://example.test").empty(), "word boundary required");
    auto quotes = Chat::QuoteLines("> Original sentence, wrapped in the UI.\n> Another line.\n\nMy reply\n2 > 1\n>not markup");
    Check(quotes.size() == 6 && quotes[0].quote && quotes[1].quote, "quote block recognized");
    Check(quotes[0].text == "Original sentence, wrapped in the UI.\n", "quote marker removed for display only");
    Check(!quotes[2].quote && !quotes[3].quote && !quotes[4].quote && !quotes[5].quote, "reply and comparisons not quoted");
    Check(Chat::QuoteLines(">> nested")[0].text == "> nested", "nested quote level preserved");
    size_t bundled=0;
    for(const auto& file:std::filesystem::directory_iterator("resources/gifs"))if(file.path().extension()==".gif") {
        auto bytes=Read(file.path().string());auto animation=Media::DecodeAnimation(bytes.data(),bytes.size());
        Check(animation.width==180 && animation.height==180,"local GIF dimensions");
        Check(animation.frames.size()==24,"local GIF animation frames");
        Check(animation.frames.front().rgba!=animation.frames[6].rgba,"local GIF visibly animates");++bundled;
    }
    Check(bundled==8,"eight original bundled GIFs");
    for (const auto* name : {"animation.gif", "animation.webp"}) {
        auto bytes = Read(std::string("tests/media/") + name);
        auto animation = Media::DecodeAnimation(bytes.data(), bytes.size());
        Check(animation.width == 8 && animation.height == 6, "canvas size");
        Check(animation.frames.size() == 3, "all animation frames decoded");
        for (size_t i = 0; i < 3; ++i) {
            Check(animation.frames[i].delayMs == int((i + 1) * 100), "frame timing");
            Check(animation.frames[i].rgba.size() == 8 * 6 * 4, "RGBA frame length");
            Check(animation.frames[i].rgba[i] > 100, "composited frame color");
            Check(animation.frames[i].rgba[3] == 255, "opaque alpha");
        }
        for (size_t size = 0; size < 12; ++size)
            Check(Media::DecodeAnimation(bytes.data(), size).frames.empty(), "short file rejected");
        Check(Media::DecodeAnimation(bytes.data(), bytes.size() / 2).frames.empty(), "truncated file rejected");
    }
    auto gif = Read("tests/media/animation.gif");
    auto disposal = Read("tests/media/disposal.gif");
    auto golden = Read("tests/media/disposal.rgba");
    auto composited = Media::DecodeAnimation(disposal.data(), disposal.size());
    Check(composited.frames.size() == 3, "partial transparent GIF frames decoded");
    for (size_t i = 0; i < 3; ++i) {
        auto begin = golden.begin() + i * 8 * 6 * 4;
        Check(composited.frames[i].rgba == std::vector<uint8_t>(begin, begin + 8 * 6 * 4), "disposal and transparency match reference");
    }
    Check(composited.frames[0].delayMs == 50 && composited.frames[1].delayMs == 50, "fast frames clamped");
    gif[6] = gif[7] = 255;
    Check(Media::DecodeAnimation(gif.data(), gif.size()).frames.empty(), "oversized GIF rejected before decode");
    Check(Media::DecodeAnimation(nullptr, Media::kFileLimit + 1).frames.empty(), "invalid source rejected");
    char temporary[] = "/tmp/renga-media-test-XXXXXX";
    Check(mkdtemp(temporary) != nullptr, "private test directory");
    std::string error, imported;
    const auto original = Read("tests/media/sticker.png");
    {
        Media::Library library(temporary, "alice@example.test");
        Check(library.List(true).empty(), "new library empty");
        imported = library.Import("tests/media/sticker.png", true, error);
        Check(!imported.empty() && error.empty(), "PNG sticker imported");
        Check(Read(imported) == original, "original bytes preserved");
        Check(library.Import("tests/media/sticker.png", true, error) == imported, "duplicate content reused");
        Check(library.List(true).size() == 1, "duplicate not listed twice");
        Check(library.Import("tests/media/sticker.png", false, error).empty(), "PNG excluded from GIF collection");
        Check(!error.empty(), "invalid import explained");
        Check(!library.Import("tests/media/animation.gif", false, error).empty(), "GIF imported");
        Check(!library.Import("tests/media/animation.webp", true, error).empty(), "WebP sticker imported");
        Check(library.List(false).size() == 1 && library.List(true).size() == 2, "collections separated");
        Check(library.Import("/dev/zero", true, error).empty(), "non-regular source rejected");
        Check(library.Import("missing", true, error).empty(), "missing source rejected");
    }
    Media::Library reopened(temporary, "alice@example.test"), other(temporary, "bob@example.test"), anonymous(temporary, "");
    Check(reopened.List(true).size() == 2, "library survives recreation");
    Check(other.List(true).empty(), "account isolation");
    Check(anonymous.Import("tests/media/sticker.png", true, error).empty(), "anonymous import refused");
    Check(Read("tests/media/sticker.png") == original, "source not modified");
    Check(!reopened.Remove("tests/media/sticker.png", true), "source removal refused");
    Check(!other.Remove(imported, true), "other account removal refused");
    Check(reopened.Remove(imported, true), "library copy removed");
    Check(reopened.List(true).size() == 1, "removed copy absent");
    Check(Read("tests/media/sticker.png") == original, "removal preserves source");
    Check(Media::Emojis().size() == 3944, "complete Unicode 17 emoji catalogue");
    Check(reopened.SetFavoriteEmoji("🫩",true,error),"new Unicode emoji favorite");
    Check(reopened.SetFavoriteEmoji("👩🏽‍🚀",true,error),"combined emoji favorite");
    Check(reopened.SetFavoriteEmoji("🫩",true,error) && reopened.FavoriteEmojis().size()==2,"favorite deduplicated");
    Check(Media::Library(temporary,"alice@example.test").FavoriteEmojis().size()==2,"favorites survive reopening");
    Check(other.FavoriteEmojis().empty(),"favorite account isolation");
    Check(!anonymous.SetFavoriteEmoji("😀",true,error),"anonymous favorite refused");
    Check(!reopened.SetFavoriteEmoji("invalid",true,error),"invalid emoji rejected");
    Check(reopened.SetFavoriteEmoji("🫩",false,error) && reopened.FavoriteEmojis().size()==1,"favorite removed");
    Check(Chat::ReplyPrefix("Hei 🌞\nSecond") == "> Hei 🌞\n> Second\n\n", "multiline reply fallback");
    Check(Chat::Codepoints(Chat::ReplyPrefix("🌞"))==5,"reply fallback counts Unicode scalars, not bytes");
    Check(Chat::ReplyPrefix(std::string(2047,'a')+"🌞").find("🌞")==std::string::npos,"quote excerpt never cuts UTF8");
    for (const auto& e : Media::Emojis()) Check(*e.text && *e.keywords, "emoji has text and search keywords");
    const auto pack=std::string(temporary)+"/pack";
    std::filesystem::create_directories(pack+"/nested");
    std::filesystem::copy_file("tests/media/sticker.png",pack+"/sticker.png");
    std::filesystem::copy_file("tests/media/sticker.png",pack+"/duplicate.png");
    std::filesystem::copy_file("tests/media/animation.webp",pack+"/animation.webp");
    std::filesystem::copy_file("tests/media/animation.gif",pack+"/nested/not-imported.gif");
    {std::ofstream file(pack+"/README.txt");file<<"Not an image";}
    {std::ofstream file(pack+"/broken.gif");file<<"Invalid image";}
    std::filesystem::create_symlink("tests/media/animation.gif",pack+"/link.gif");
    Media::Library batch(temporary,"batch@example.test");
    auto result=batch.ImportSelection({pack},true);
    Check(result.paths.size()==2 && result.failed==1 && !result.error.empty(),"folder import reports partial failure and deduplicates contents");
    Check(batch.List(true).size()==2,"nested directories and links are not imported");
    result=batch.ImportSelection({pack+"/sticker.png",pack+"/animation.webp"},true);
    Check(result.paths.size()==2 && result.failed==0 && batch.List(true).size()==2,"multiple selected files reuse existing copies");
    std::atomic_bool cancel{true};
    result=batch.ImportSelection({pack},true,&cancel);
    Check(result.paths.empty() && !result.error.empty() && batch.List(true).size()==2,"cancelled import leaves existing library untouched");
    Check(batch.ImportSelection({pack+"/nested/missing"},true).failed==1,"missing import source is reported");
    Check(batch.ImportSelection({pack+"/link.gif"},true).failed==1,"selected symlink rejected");
    Check(batch.ImportSelection(std::vector<std::string>(513,pack),true).failed==1,"selection count bounded before importing");
    Check(Read(pack+"/sticker.png")==original,"batch import leaves original file unchanged");
    std::filesystem::remove_all(temporary);
    std::cout << checks << " media checks passed\n";
}
