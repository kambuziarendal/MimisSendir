// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Chat {
// One tracker per conversation. Observe only fully visible incoming content in
// the foreground. Scrolling past a message is not enough: require stable dwell.
class ReadTracker {
public:
    std::vector<std::string> Observe(const std::vector<std::string>& visible, int64_t now) {
        std::map<std::string, int64_t> next;
        std::vector<std::string> ready;
        for (const auto& id : visible) {
            if (id.empty() || id.size() > 1024 || fSent.count(id)) continue;
            auto found = fVisible.find(id);
            const auto since = found == fVisible.end() ? now : found->second;
            next.emplace(id, since);
            if (now - since >= 400000) ready.push_back(id);
            if (next.size() == 1000) break;
        }
        fVisible.swap(next);
        return ready;
    }
    void Submitted(const std::string& id) {
        if (!fSent.insert(id).second) return;
        fOrder.push_back(id);
        if (fOrder.size() > 4096) { fSent.erase(fOrder.front()); fOrder.pop_front(); }
        fVisible.erase(id);
    }
    bool SubmittedAlready(const std::string& id) const { return fSent.count(id); }
private:
    std::map<std::string, int64_t> fVisible;
    std::set<std::string> fSent;
    std::deque<std::string> fOrder;
};
}
