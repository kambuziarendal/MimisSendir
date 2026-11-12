/*
 * LogHandler.cpp
 * Copyright (C) 2019 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#include <gloox/loghandler.h>
#include "BeDC.h"


class LogHandler: public gloox::LogHandler
{
	public:
	LogHandler() 
		: fDebugContext("Renga"),
		fWarningContext("Renga", DC_YELLOW),
		fErrorContext("Renga", DC_RED)
	{
	}

	private:
	void handleLog(gloox::LogLevel level, gloox::LogArea area, const std::string &message) override {
		switch(level) {
			case gloox::LogLevelDebug:
			default:
				fDebugContext.SendFormat("%d %s", area, message.c_str());
				break;
			case gloox::LogLevelWarning:
				fWarningContext.SendFormat("%d %s", area, message.c_str());
				break;
			case gloox::LogLevelError:
				fErrorContext.SendFormat("%d %s", area, message.c_str());
				break;
		}
	}

	BeDC fDebugContext;
	BeDC fWarningContext;
	BeDC fErrorContext;
};
