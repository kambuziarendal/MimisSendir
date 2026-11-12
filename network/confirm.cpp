/*
 * Copyright (C) 2025 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#include "confirm.h"


Confirm::Confirm(const gloox::Tag* tag)
	: gloox::StanzaExtension(ExtConfirm)
{
	if (!tag || tag->name() != "confirm" || tag->xmlns() != XMLNS_HTTP_AUTH)
		return;

	m_id = tag->findAttribute("id");
	m_method = tag->findAttribute("method");
	m_url = tag->findAttribute("url");
}

Confirm::Confirm(const char* id, const char* method, const char* url)
	: gloox::StanzaExtension(ExtReferences)
	, m_id(id)
	, m_method(method)
	, m_url(url)
{
}

const std::string& Confirm::filterString() const
{
	static const std::string filter = "/message/confirm[@xmlns='" + XMLNS_HTTP_AUTH + "']";
	return filter;
}

gloox::Tag* Confirm::tag() const
{
	if (m_id.empty() || m_method.empty() || m_url.empty())
		return 0;

	gloox::Tag* t = new gloox::Tag("confirm");
	t->setXmlns(XMLNS_HTTP_AUTH);
	t->addAttribute("id", m_id);
	t->addAttribute("method", m_method);
	t->addAttribute("url", m_url);

	return t;
}
