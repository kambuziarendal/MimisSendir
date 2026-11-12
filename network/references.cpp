/*
 * Copyright (C) 2023 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#include "references.h"


Reference::Reference(const gloox::Tag* tag)
	: gloox::StanzaExtension(ExtReferences)
{
	if (!tag || tag->name() != "reference" || tag->xmlns() != XMLNS_REFERENCES)
		return;

	m_type = tag->findAttribute("type");
	m_uri = tag->findAttribute("uri");

	m_begin = strtol(tag->findAttribute("begin").c_str(), 0, 10);
	m_end = strtol(tag->findAttribute("end").c_str(), 0, 10);
}

Reference::Reference(const char* type, const char* uri)
	: gloox::StanzaExtension(ExtReferences)
	, m_type(type)
	, m_uri(uri)
	, m_begin(0)
	, m_end(0)
{
}

const std::string& Reference::filterString() const
{
	static const std::string filter = "/message/reference[@xmlns='" + XMLNS_REFERENCES + "']";
	return filter;
}

gloox::Tag* Reference::tag() const
{
	if (m_type.empty() || m_uri.empty())
		return 0;

	gloox::Tag* t = new gloox::Tag("reference");
	t->setXmlns(XMLNS_REFERENCES);
	t->addAttribute("type", m_type);
	t->addAttribute("uri", m_uri);

	if (m_begin >= 0 && m_end >= 0) {
		t->addAttribute("begin", m_begin);
		t->addAttribute("end", m_end);
	}

	return t;
}
