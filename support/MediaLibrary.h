// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <string>
#include <atomic>
#include <vector>
namespace Media {
struct Entry { std::string name, path; };
struct ImportResult {
    std::vector<std::string> paths;
    size_t failed = 0;
    std::string error;
};
class Library {
public:
    Library(std::string settingsDirectory, const std::string& account, std::string builtinDirectory = {});
    std::vector<Entry> List(bool stickers) const;
    // Copies to private account storage; never sends or modifies the source.
    std::string Import(const std::string& source, bool stickers, std::string& error) const;
    // Imports selected files or the immediate image children of selected folders.
    // Complete files are published individually; cancellation keeps completed copies.
    ImportResult ImportSelection(const std::vector<std::string>& sources, bool stickers,
        const std::atomic_bool* cancelled = nullptr) const;
    bool Remove(const std::string& path, bool stickers) const;
    bool IsImported(const std::string& path) const;
    std::vector<std::string> FavoriteEmojis() const;
    bool SetFavoriteEmoji(const std::string& emoji, bool favorite, std::string& error) const;
private:
    std::string fRoot, fBuiltin;
};
struct Emoji { const char* text; const char* keywords; const char* category; };
const std::vector<Emoji>& Emojis();
}
