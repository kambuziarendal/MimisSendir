/*
 * Copyright (C) 2025 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once


#include <gloox/stanzaextension.h>
#include <gloox/tag.h>

#include "Extensions.h"


class Confirm: public gloox::StanzaExtension
{
	public:
		Confirm(const gloox::Tag* tag = 0);
		Confirm(const char* id, const char*  method, const char* url);

		virtual ~Confirm() {}

		const std::string& filterString() const override;
		gloox::Tag* tag() const override;

		gloox::StanzaExtension* newInstance( const gloox::Tag* tag ) const override
		{
			Confirm* instance = new Confirm( tag );
			return instance;
		}

		gloox::StanzaExtension* clone() const override
		{
			return new Confirm(m_id.c_str(), m_method.c_str(), m_url.c_str());
		}

		const std::string& id() const { return m_id; }
		const std::string& method() const { return m_method; }
		const std::string& url() const { return m_url; }

	private:
		std::string m_id;
		std::string m_method;
		std::string m_url;
};
