// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <gloox/stanzaextension.h>
#include <gloox/tag.h>
#include <string>
namespace Chat {
class Correction final : public gloox::StanzaExtension {
public:
    static constexpr int kExtension=gloox::ExtUser+52;
    explicit Correction(std::string id="") : StanzaExtension(kExtension),fId(std::move(id)) {}
    const std::string& id() const { return fId; }
    const std::string& filterString() const override { static const std::string s="/message/replace[@xmlns='urn:xmpp:message-correct:0']";return s; }
    gloox::StanzaExtension* newInstance(const gloox::Tag* t) const override { return new Correction(t?t->findAttribute("id"):""); }
    gloox::StanzaExtension* clone() const override { return new Correction(*this); }
    gloox::Tag* tag() const override {
        auto* t=new gloox::Tag("replace");t->setXmlns("urn:xmpp:message-correct:0");t->addAttribute("id",fId);return t;
    }
private:
    std::string fId;
};
}
