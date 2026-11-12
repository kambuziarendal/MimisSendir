// Copyright 2026 Kambuzi EDB. Distributed under the MIT license.
#include "support/ProfileMigration.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
namespace fs = std::filesystem;
std::string Read(const fs::path& p) { std::ifstream f(p); return {std::istreambuf_iterator<char>(f), {}}; }
int main() {
    char temporary[] = "/tmp/mimissendir-migration-test-XXXXXX";
    assert(mkdtemp(temporary)); fs::path root(temporary), old = root/"old", copy = root/"new";
    fs::create_directories(old/"history-v1/account/peer");
    std::ofstream(old/"app-settings") << "<container><last-login>alice@example.test</last-login></container>";
    std::ofstream(old/"history-v1/account/peer/message") << "history with receipts";
    fs::create_directories(old/"media-test/stickers");
    std::ofstream(old/"media-test/stickers/example.webp") << "test bytes";
    std::string error;
    assert(ProfileMigration::Copy(old.string(), copy.string(), error));
    for (auto& entry : fs::recursive_directory_iterator(old)) {
        auto other = copy/entry.path().lexically_relative(old); struct stat st{}; assert(!stat(other.c_str(), &st));
        if (entry.is_regular_file()) { assert(Read(entry.path()) == Read(other)); assert((st.st_mode & 0777) == 0600); }
        else assert((st.st_mode & 0777) == 0700);
    }
    std::ofstream(copy/"app-settings") << "new settings";
    assert(Read(old/"app-settings").find("alice@example.test") != std::string::npos);
    assert(!ProfileMigration::Copy(old.string(), copy.string(), error));
    assert(Read(copy/"app-settings") == "new settings");
    fs::create_symlink(old, root/"linked");
    assert(!ProfileMigration::Copy((root/"linked").string(), (root/"symlink-copy").string(), error));
    fs::create_symlink(root/"missing", root/"dangling-destination");
    assert(!ProfileMigration::Copy(old.string(), (root/"dangling-destination").string(), error));
    fs::create_symlink(root/"missing", old/"unsafe");
    assert(!ProfileMigration::Copy(old.string(), (root/"failed-copy").string(), error));
    assert(!fs::exists(root/"failed-copy")); fs::remove(old/"unsafe");
    assert(!ProfileMigration::Copy(old.string(), (old/"nested-copy").string(), error));
    assert(!ProfileMigration::Copy((root/"missing").string(), (root/"absent-copy").string(), error));
    // Exhaust staging writes through a genuinely unreadable source as a non-root test.
    if (getuid() != 0) {
        chmod((old/"app-settings").c_str(), 0000);
        assert(!ProfileMigration::Copy(old.string(), (root/"unreadable-copy").string(), error));
        assert(!fs::exists(root/"unreadable-copy")); chmod((old/"app-settings").c_str(), 0600);
    }
    for (auto& entry : fs::directory_iterator(root))
        assert(!entry.path().filename().string().starts_with(".mimissendir-import-"));
    fs::remove_all(root);
    std::cout << "Profile migration: copy, isolation, permissions, no overwrite, links, failure cleanup passed\n";
}
