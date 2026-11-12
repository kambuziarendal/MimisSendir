// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
namespace Giphy {
struct Item { std::string title, url, creator; };
struct Transfer {
    std::string path, directory;
    uint64_t token = 0;
    ~Transfer();
};
bool MediaURL(const std::string& url);
std::vector<Item> Parse(const std::string& json, std::string& error);
std::vector<Item> Search(const std::string& query, unsigned offset,
    const std::atomic_bool& cancelled, std::string& error);
std::shared_ptr<Transfer> Download(const std::string& url,
    const std::atomic_bool& cancelled, std::string& error);
// Weak registry: a synchronous UI handoff acquires ownership before the picker closes.
std::shared_ptr<Transfer> Acquire(uint64_t token);
}
