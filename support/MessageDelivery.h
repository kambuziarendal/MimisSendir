// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <map>
#include <deque>
#include <string>

namespace Chat {
enum class Delivery { Queued, Unconfirmed, Accepted, Delivered, Failed };
struct Outgoing {
    std::string text;
    Delivery state = Delivery::Queued;
    std::string replyId, replyTo, replyPrefix, replyLabel;
};
// View-owned, bounded correlation. No reconnect replay, guesses or global
// "last message failed" shortcuts. Unknown/empty IDs never mutate a message.
class Deliveries {
public:
    bool Add(const std::string& id, const std::string& text, const std::string& replyId="",
        const std::string& replyTo="", const std::string& replyPrefix="", const std::string& replyLabel="") {
        if (id.empty() || text.size() > 65536 || fMessages.count(id)) return false;
        if (fMessages.size() >= 256) {
            fMessages.erase(fOrder.front());
            fOrder.pop_front();
        }
        fOrder.push_back(id);
        return fMessages.emplace(id, Outgoing{text,Delivery::Queued,replyId,replyTo,replyPrefix,replyLabel}).second;
    }
    bool Update(const std::string& id, Delivery state) {
        auto it = fMessages.find(id);
        if (it == fMessages.end() || it->second.state == state) return false;
        auto old = it->second.state;
        if (old == Delivery::Delivered || (old == Delivery::Failed && state != Delivery::Delivered)) return false;
        if (state == Delivery::Queued || (state == Delivery::Unconfirmed && old != Delivery::Queued)) return false;
        it->second.state = state;
        return true;
    }
    const Outgoing* Find(const std::string& id) const {
        auto it = fMessages.find(id);
        return it == fMessages.end() ? nullptr : &it->second;
    }
    static const char* Label(Delivery state) {
        switch (state) {
            case Delivery::Queued: return "Sending";
            case Delivery::Accepted: return "Accepted by bridge; delivery unconfirmed";
            case Delivery::Delivered: return "Delivered (receipt received)";
            case Delivery::Failed: return "Send failed; delivery unconfirmed";
            default: return "Sent to XMPP; delivery unconfirmed";
        }
    }
private:
    std::map<std::string, Outgoing> fMessages;
    std::deque<std::string> fOrder;
};
}
