/*
 * bobhandler.h
 * Copyright (C) 2019 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once

namespace gloox {
	class BOB;
};


class BobHandler
{
	public:
		virtual void handleBob(const gloox::BOB* bob) = 0;
};
