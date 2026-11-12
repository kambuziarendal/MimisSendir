// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace Chat {
struct Link { size_t start, end; std::string target; };
inline std::vector<Link> FindLinks(const std::string& text)
{
    std::vector<Link> links;
    auto lower = text;
    for (auto& c : lower) c = std::tolower(static_cast<unsigned char>(c));
    for (size_t at = 0; at < text.size();) {
        size_t prefix = 0;
        for (const auto* scheme : {"https://", "http://", "ftps://", "ftp://", "www."}) {
            std::string value(scheme);
            if (lower.compare(at, value.size(), value) == 0) { prefix = value.size(); break; }
        }
        if (!prefix || (at && (std::isalnum(static_cast<unsigned char>(text[at - 1]))
            || text[at - 1] == '_' || text[at - 1] == '@'))) { ++at; continue; }
        size_t end = at + prefix;
        while (end < text.size()) {
            const auto c = static_cast<unsigned char>(text[end]);
            if (c <= 32 || c == 127 || c == '<' || c == '>' || c == '"') break;
            ++end;
        }
        // Preserve query strings, fragments, percent encoding and balanced
        // parentheses. Only sentence punctuation outside the URL is trimmed.
        int parentheses = 0, brackets = 0;
        for (size_t i = at; i < end; ++i) {
            parentheses += (text[i] == '(') - (text[i] == ')');
            brackets += (text[i] == '[') - (text[i] == ']');
        }
        while (end > at + prefix) {
            char c = text[end - 1];
            bool trim = c == '.' || c == ',' || c == '!' || c == ';' || c == ':' || c == '\'';
            if (c == ')' || c == ']') {
                trim = c == ')' ? parentheses < 0 : brackets < 0;
                if (trim) { if (c == ')') ++parentheses; else ++brackets; }
            }
            if (!trim) break;
            --end;
        }
        if (end <= at + prefix) { at += prefix; continue; }
        auto target = text.substr(at, end - at);
        if (lower.compare(at, 4, "www.") == 0) target = "https://" + target;
        links.push_back({at, end, target});
        at = end;
    }
    return links;
}

struct QuoteLine { std::string text; bool quote; };
inline std::vector<QuoteLine> QuoteLines(const std::string& text)
{
    std::vector<QuoteLine> lines;
    for (size_t at = 0; at < text.size();) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        else ++end;
        auto line = text.substr(at, end - at);
        size_t mark = 0;
        while (mark < 3 && mark < line.size() && line[mark] == ' ') ++mark;
        const bool quote = mark < line.size() && line[mark] == '>'
            && (mark + 1 == line.size() || line[mark + 1] == ' '
                || line[mark + 1] == '\n' || line[mark + 1] == '>');
        if (quote) {
            ++mark;
            if (mark < line.size() && line[mark] == ' ') ++mark;
            line.erase(0, mark);
        }
        lines.push_back({line, quote});
        at = end;
    }
    return lines;
}
}
