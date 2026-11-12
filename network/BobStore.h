/*
 * BobStore.h
 * Copyright (C) 2019 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once

#include "bobhandler.h"

#include <map>
#include <string>


class BobStore: public BobHandler
{
	public:
		static BobStore* Instance();

		std::string Get(std::string key);

		void handleBob(const gloox::BOB* bob) final;
	
	private:
		std::map<std::string, std::string> fStorage;
};
