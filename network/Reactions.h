// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <gloox/message.h>
#include <gloox/stanzaextension.h>
#include <gloox/tag.h>
#include <algorithm>
#include <string>
#include <vector>
namespace Chat {
// XEP-0444: each update replaces this actor's complete reaction set.
class Reactions : public gloox::StanzaExtension {
public:
    static constexpr int kExtension = gloox::ExtUser + 41;
    explicit Reactions(const gloox::Tag* t = nullptr) : StanzaExtension(kExtension) {
        if (!t || t->name() != "reactions" || t->xmlns() != "urn:xmpp:reactions:0") return;
        fId = t->findAttribute("id");
        if (fId.empty() || fId.size() > 1024) { fId.clear(); return; }
        for (const auto* child : t->children()) {
            if (child->name() != "reaction") continue;
            const auto& text = child->cdata();
            if (text.empty() || text.size() > 64 || text.find_first_of("\r\n\t") != std::string::npos
                || fValues.size() >= 16) { fId.clear(); fValues.clear(); return; }
            if (std::find(fValues.begin(), fValues.end(), text) == fValues.end()) fValues.push_back(text);
        }
    }
    Reactions(std::string id, std::vector<std::string> values)
        : StanzaExtension(kExtension), fId(std::move(id)), fValues(std::move(values)) {}
    const std::string& id() const { return fId; }
    const std::vector<std::string>& values() const { return fValues; }
    const std::string& filterString() const override {
        static const std::string f = "/message/reactions[@xmlns='urn:xmpp:reactions:0']"; return f;
    }
    gloox::StanzaExtension* newInstance(const gloox::Tag* t) const override { return new Reactions(t); }
    gloox::StanzaExtension* clone() const override { return new Reactions(*this); }
    gloox::Tag* tag() const override {
        auto* t = new gloox::Tag("reactions"); t->setXmlns("urn:xmpp:reactions:0"); t->addAttribute("id", fId);
        for (const auto& value : fValues) new gloox::Tag(t, "reaction", value);
        return t;
    }
private:
    std::string fId;
    std::vector<std::string> fValues;
};
// MUC reactions reference the stanza ID assigned by the room (XEP-0359).
class StableId : public gloox::StanzaExtension {
public:
    static constexpr int kExtension = gloox::ExtUser + 42;
    explicit StableId(const gloox::Tag* t = nullptr) : StanzaExtension(kExtension) {
        if (t && t->xmlns() == "urn:xmpp:sid:0") { id=t->findAttribute("id"); by=t->findAttribute("by"); }
    }
    std::string id, by;
    const std::string& filterString() const override {
        static const std::string f="/message/stanza-id[@xmlns='urn:xmpp:sid:0']"; return f;
    }
    gloox::StanzaExtension* newInstance(const gloox::Tag* t) const override { return new StableId(t); }
    gloox::StanzaExtension* clone() const override { return new StableId(*this); }
    gloox::Tag* tag() const override {
        auto* t=new gloox::Tag("stanza-id");t->setXmlns("urn:xmpp:sid:0");t->addAttribute("id",id);t->addAttribute("by",by);return t;
    }
};
inline std::string ReactionId(const gloox::Message& m, bool group) {
    if(group) for(const auto* ext:m.extensions()) {
        const auto* s=dynamic_cast<const StableId*>(ext);
        if(s && s->by==m.from().bare() && !s->id.empty() && s->id.size()<=1024)return s->id;
    }
    return m.id().size()<=1024 ? m.id() : "";
}
}
