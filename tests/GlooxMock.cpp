/*
 * Copyright (C) 2019 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 *
 */

/** @file
 *
 * Implements mocks for Gloox, to be able to run the unit tests.
 */

#include <gloox/client.h>


namespace gloox {

Client::Client(const std::string& server)
	: ClientBase("", server)
	, m_presence(gloox::PresenceType(), gloox::JID())
{
}

};
