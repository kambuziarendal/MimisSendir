// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
namespace SignalStickers {
struct Link { std::string id, key; };
struct Sticker { uint32_t id; std::string emoji; };
struct Pack { Link link; std::string title, author; std::vector<Sticker> stickers; };
using Bytes = std::vector<unsigned char>;
using Fetch = std::function<Bytes(const std::string&, size_t, const std::atomic_bool&, std::string&)>;
std::optional<Link> ParseLink(const std::string& text);
std::optional<Link> ParseLinkFile(const std::string& text);
std::optional<Link> ReadLinkFile(const std::string& path, std::string& error);
Bytes Decrypt(const Bytes&, const std::string& key, std::string& error);
std::optional<Pack> ParseManifest(const Bytes&, const Link&, std::string& error);
Bytes FetchHTTPS(const std::string&, size_t, const std::atomic_bool&, std::string&);
std::optional<Pack> Load(const Link&, const std::atomic_bool&, std::string&, Fetch = FetchHTTPS);
Bytes Download(const Pack&, uint32_t id, const std::atomic_bool&, std::string&, Fetch = FetchHTTPS);
}
