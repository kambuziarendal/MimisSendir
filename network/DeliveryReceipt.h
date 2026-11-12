// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <gloox/stanzaextension.h>
#include <gloox/tag.h>

namespace Chat {
// XEP-0184. The pinned Signal adapter has PROPER_RECEIPTS=True, so its
// received receipt is upstream delivery, not mere acceptance by the bridge.
class Receipt : public gloox::StanzaExtension {
public:
    static constexpr int kExtension = gloox::ExtUser + 40;
    explicit Receipt(const gloox::Tag* tag = nullptr) : StanzaExtension(kExtension) {
        if (tag && tag->xmlns() == "urn:xmpp:receipts") {
            fRequest = tag->name() == "request";
            if (tag->name() == "received") fId = tag->findAttribute("id");
        }
    }
    explicit Receipt(bool request) : StanzaExtension(kExtension), fRequest(request) {}
    const std::string& id() const { return fId; }
    bool request() const { return fRequest; }
    const std::string& filterString() const override {
        static const std::string filter = "/message/received[@xmlns='urn:xmpp:receipts']|/message/request[@xmlns='urn:xmpp:receipts']";
        return filter;
    }
    gloox::StanzaExtension* newInstance(const gloox::Tag* tag) const override { return new Receipt(tag); }
    gloox::StanzaExtension* clone() const override { return new Receipt(*this); }
    gloox::Tag* tag() const override {
        auto* tag = new gloox::Tag(fRequest ? "request" : "received");
        tag->setXmlns("urn:xmpp:receipts");
        if (!fRequest) tag->addAttribute("id", fId);
        return tag;
    }
private:
    bool fRequest = false;
    std::string fId;
};
}
