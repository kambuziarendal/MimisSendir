/*
 * Copyright (C) 2023 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once


#include <gloox/stanzaextension.h>
#include <gloox/tag.h>

#include "Extensions.h"


class Reference: public gloox::StanzaExtension
{
	public:
		Reference(const gloox::Tag* tag = 0);
		Reference(const char* type, const char* uri);

		virtual ~Reference() {}

		const std::string& filterString() const override;
		gloox::Tag* tag() const override;

		gloox::StanzaExtension* newInstance( const gloox::Tag* tag ) const override
		{
			Reference* instance = new Reference( tag );
			return instance;
		}

		gloox::StanzaExtension* clone() const override
		{
			// TODO how is the span handled?
			return new Reference(m_type.c_str(), m_uri.c_str());
		}

		const std::string& type() const { return m_type; }
		const std::string& uri() const { return m_uri; }
		const std::pair<int, int> span() const { return {m_begin, m_end}; }

	private:
		std::string m_type;
		std::string m_uri;
		int m_begin;
		int m_end;
};

