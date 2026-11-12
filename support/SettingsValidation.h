// Copyright 2026 Kambuzi EDB. Distributed under the MIT license.
#pragma once
#include <arpa/inet.h>
#include <string>
namespace SettingsValidation {
inline bool Port(const std::string& text)
{
    if (text.empty() || text.size() > 5) return false;
    unsigned value = 0;
    for (unsigned char c : text) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + c - '0';
    }
    return value > 0 && value <= 65535;
}
inline bool Domain(const std::string& text)
{
    if (text.empty() || text.size() > 253) return false;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('.', start);
        if (end == std::string::npos) end = text.size();
        if (end == start || end - start > 63 || text[start] == '-' || text[end - 1] == '-') return false;
        for (size_t i = start; i < end; ++i) {
            unsigned char c = text[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '-')) return false;
        }
        if (end == text.size()) return true;
        start = end + 1;
    }
    return false;
}
inline bool Host(const std::string& text)
{
    in6_addr address;
    return Domain(text) || inet_pton(AF_INET6, text.c_str(), &address) == 1;
}
inline bool Authority(const std::string& text)
{
    if (text.empty()) return false;
    if (text.front() == '[') {
        size_t end = text.find(']');
        if (end == std::string::npos) return false;
        in6_addr address;
        if (inet_pton(AF_INET6, text.substr(1, end - 1).c_str(), &address) != 1) return false;
        return end + 1 == text.size() || (text[end + 1] == ':' && Port(text.substr(end + 2)));
    }
    size_t colon = text.find(':');
    return colon == std::string::npos ? Domain(text)
        : Domain(text.substr(0, colon)) && Port(text.substr(colon + 1));
}
}
