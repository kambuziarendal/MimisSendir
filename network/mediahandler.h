/*
 * mediahandler.h
 * Copyright (C) 2019 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once

class Media;

class MediaHandler
{
	public:
		virtual void handleMedia(const Media* media) = 0;
};
