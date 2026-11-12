/*
 * Xhtml.h
 * Copyright (C) 2022 pulkomandy <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef XHTML_H
#define XHTML_H

#include "ui/TalkView.h"
#include "ui/textview/TextSpan.h"

#include <gloox/tag.h>

#define restrict /* C99 compatibility */
#include <libcss/libcss.h>
#undef restrict

#include <list>


struct CustomCssColor {
	lwc_string* colorString;
	uint32_t	cssColor;
};


class XhtmlProcessor {
public:
	XhtmlProcessor();
	~XhtmlProcessor();
	void ProcessXhtml(const gloox::Tag* tag, CharacterStyle, std::list<TextSpan>&);
private:
	css_select_ctx *select_ctx;
	css_stylesheet_params params;
	css_stylesheet* base_sheet;

	CustomCssColor colors[3];
};


#endif /* !XHTML_H */
