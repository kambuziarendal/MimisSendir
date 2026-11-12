/*
 * Copyright (C) 2019 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#include "BobStore.h"

#include "bob.h"


BobStore* BobStore::Instance()
{
	static BobStore* sInstance = nullptr;

	if (sInstance == nullptr)
		sInstance = new BobStore();

	return sInstance;
}


std::string BobStore::Get(std::string key)
{
	return fStorage[key];
}


void BobStore::handleBob(const gloox::BOB* bob)
{
	// FIXME store type and handle maxage (purge expired bobs)
	fStorage.insert(std::pair<std::string, std::string>(bob->cid(), bob->data()));
}
