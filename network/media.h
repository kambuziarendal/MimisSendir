/*
 * media.h
 * Copyright (C) 2019 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once


#include <gloox/stanzaextension.h>

#include "mediahandler.h"

class Media: public gloox::StanzaExtension
{
	public:
		Media(const gloox::Tag* tag = 0);
		const std::string& filterString() const final;
		gloox::StanzaExtension* newInstance(const gloox::Tag* tag) const final
		{
			Media* instance = new Media(tag);
			if (fMediaHandler)
				fMediaHandler->handleMedia(instance);
			return instance;
		}

		gloox::Tag* tag() const final;
		gloox::StanzaExtension* clone() const final
		{
			return new Media(*this);
		}

		void RegisterMediaHandler(MediaHandler* handler) { fMediaHandler = handler; }

		const std::string& uri() const { return fURI; }
		const std::string& type() const { return fType; }

	private:
		std::string fType;
		std::string fURI;

		MediaHandler* fMediaHandler;
};
