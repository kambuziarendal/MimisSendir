// Copyright 2026 Kambuzi EDB. Distributed under the MIT license.
#include "ProfileMigration.h"
#include <filesystem>
#include <map>
#include <stdexcept>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace ProfileMigration {
namespace {
namespace fs = std::filesystem;
struct Descriptor {
    int fd;
    ~Descriptor() { if (fd >= 0) close(fd); }
};
using Snapshot = std::map<fs::path, struct stat>;
bool Same(const struct stat& a, const struct stat& b) {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_mode == b.st_mode
        && a.st_size == b.st_size && a.st_mtime == b.st_mtime
        && a.st_ctime == b.st_ctime && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec
        && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}
Snapshot Inspect(const fs::path& root) {
    Snapshot entries;
    struct stat info{};
    if (lstat(root.c_str(), &info) || !S_ISDIR(info.st_mode))
        throw std::runtime_error("The old profile must be a real directory, not a link.");
    entries[""] = info;
    uint64_t bytes = 0;
    for (auto it = fs::recursive_directory_iterator(root); it != fs::recursive_directory_iterator(); ++it) {
        if (it.depth() > 64 || entries.size() >= 100000)
            throw std::runtime_error("The old profile is too large to import automatically.");
        if (lstat(it->path().c_str(), &info) || (!S_ISREG(info.st_mode) && !S_ISDIR(info.st_mode)))
            throw std::runtime_error("The old profile contains a link or special file. Nothing was imported.");
        if (S_ISREG(info.st_mode)) bytes += info.st_size;
        if (bytes > 8ULL * 1024 * 1024 * 1024)
            throw std::runtime_error("The old profile exceeds the 8 GiB automatic import limit.");
        entries[it->path().lexically_relative(root)] = info;
    }
    return entries;
}
void CopyFile(const fs::path& from, const fs::path& to, const struct stat& expected) {
    Descriptor in{open(from.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK)};
    struct stat before{}, after{};
    if (in.fd < 0 || fstat(in.fd, &before) || !Same(before, expected))
        throw std::runtime_error("The old profile changed or could not be read. Close Renga and retry.");
    Descriptor out{open(to.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600)};
    if (out.fd < 0) throw std::runtime_error("Could not create the profile copy. Check free space and permissions.");
    char buffer[65536];
    off_t total = 0;
    for (;;) {
        ssize_t n = read(in.fd, buffer, sizeof buffer);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) throw std::runtime_error("Could not read the complete old profile.");
        if (!n) break;
        total += n;
        for (ssize_t offset = 0; offset < n;) {
            ssize_t written = write(out.fd, buffer + offset, n - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) throw std::runtime_error("Could not save the profile copy. Check free disk space.");
            offset += written;
        }
    }
    if (fsync(out.fd) || fstat(in.fd, &after) || !Same(before, after) || total != before.st_size)
        throw std::runtime_error("The profile copy was incomplete or its source changed. Nothing was imported.");
}
}
bool Copy(const std::string& source, const std::string& destination, std::string& error) {
    fs::path staging;
    error.clear();
    try {
        const fs::path from(source), to(destination);
        // No destination replacement, even an empty directory or dangling link.
        if (fs::symlink_status(to).type() != fs::file_type::not_found)
            throw std::runtime_error("A MímisSendir profile already exists; it was not replaced.");
        if (fs::weakly_canonical(to.parent_path()) == fs::weakly_canonical(from)
            || fs::weakly_canonical(to).string().starts_with(fs::weakly_canonical(from).string() + "/"))
            throw std::runtime_error("The new profile must be separate from the old profile.");
        const auto original = Inspect(from);
        std::string pattern = (to.parent_path() / ".mimissendir-import-XXXXXX").string();
        if (!mkdtemp(pattern.data())) throw std::runtime_error("Could not create a private import directory.");
        staging = pattern;
        for (const auto& [relative, info] : original) {
            if (relative.empty()) continue;
            const auto target = staging / relative;
            if (S_ISDIR(info.st_mode)) {
                if (mkdir(target.c_str(), 0700)) throw std::runtime_error("Could not create the profile copy.");
            } else CopyFile(from / relative, target, info);
        }
        const auto current = Inspect(from);
        if (original.size() != current.size()) throw std::runtime_error("The old profile changed. Close Renga and retry.");
        for (const auto& [path, info] : original) {
            auto entry = current.find(path);
            if (entry == current.end() || !Same(info, entry->second))
                throw std::runtime_error("The old profile changed. Close Renga and retry.");
        }
        if (fs::symlink_status(to).type() != fs::file_type::not_found)
            throw std::runtime_error("A new profile appeared during import; neither profile was replaced.");
        fs::rename(staging, to);
        return true;
    } catch (const std::exception& failure) {
        error = failure.what();
        if (!staging.empty()) { std::error_code ignored; fs::remove_all(staging, ignored); }
        return false;
    }
}
}
