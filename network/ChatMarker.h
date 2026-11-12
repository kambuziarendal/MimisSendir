// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <gloox/stanzaextension.h>
#include <gloox/tag.h>

namespace Chat {
// XEP-0333: displayed is distinct from XEP-0184 delivery and reactions.
class Marker : public gloox::StanzaExtension {
public:
    static constexpr int kExtension = gloox::ExtUser + 53;
    explicit Marker(const gloox::Tag* tag = nullptr) : StanzaExtension(kExtension) {
        if (!tag || tag->xmlns() != "urn:xmpp:chat-markers:0") return;
        fMarkable = tag->name() == "markable";
        if (tag->name() == "displayed") fId = tag->findAttribute("id");
        if (fId.size() > 1024) fId.clear();
    }
    explicit Marker(const std::string& id) : StanzaExtension(kExtension), fId(id) {}
    bool markable() const { return fMarkable; }
    const std::string& id() const { return fId; }
    const std::string& filterString() const override {
        static const std::string filter = "/message/markable[@xmlns='urn:xmpp:chat-markers:0']|/message/displayed[@xmlns='urn:xmpp:chat-markers:0']";
        return filter;
    }
    gloox::StanzaExtension* newInstance(const gloox::Tag* tag) const override { return new Marker(tag); }
    gloox::StanzaExtension* clone() const override { return new Marker(*this); }
    gloox::Tag* tag() const override {
        auto* tag = new gloox::Tag(fMarkable ? "markable" : "displayed");
        tag->setXmlns("urn:xmpp:chat-markers:0");
        if (!fMarkable) tag->addAttribute("id", fId);
        return tag;
    }
private:
    bool fMarkable = false;
    std::string fId;
};
}
