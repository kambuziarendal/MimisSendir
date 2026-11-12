// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <gloox/stanzaextension.h>
#include <gloox/tag.h>
#include <string>
namespace Chat {
class Reply final : public gloox::StanzaExtension {
public:
    Reply(std::string id="",std::string to="") : StanzaExtension(gloox::ExtUser+50),fId(std::move(id)),fTo(std::move(to)) {}
    const std::string& filterString() const override { static const std::string s="/message/reply[@xmlns='urn:xmpp:reply:0']";return s; }
    gloox::StanzaExtension* newInstance(const gloox::Tag* t) const override { return new Reply(t?t->findAttribute("id"):"",t?t->findAttribute("to"):""); }
    gloox::StanzaExtension* clone() const override { return new Reply(*this); }
    gloox::Tag* tag() const override {
        auto* t=new gloox::Tag("reply");t->setXmlns("urn:xmpp:reply:0");t->addAttribute("id",fId);
        if(!fTo.empty())t->addAttribute("to",fTo);
        return t;
    }
private:
    std::string fId,fTo;
};
class ReplyFallback final : public gloox::StanzaExtension {
public:
    explicit ReplyFallback(size_t end=0) : StanzaExtension(gloox::ExtUser+51),fEnd(end) {}
    const std::string& filterString() const override { static const std::string s="/message/fallback[@xmlns='urn:xmpp:fallback:0']";return s; }
    gloox::StanzaExtension* newInstance(const gloox::Tag*) const override { return new ReplyFallback(); }
    gloox::StanzaExtension* clone() const override { return new ReplyFallback(*this); }
    gloox::Tag* tag() const override {
        auto* t=new gloox::Tag("fallback");t->setXmlns("urn:xmpp:fallback:0");t->addAttribute("for","urn:xmpp:reply:0");
        auto* body=new gloox::Tag(t,"body");body->addAttribute("start","0");body->addAttribute("end",std::to_string(fEnd));return t;
    }
private:
    size_t fEnd;
};
}
