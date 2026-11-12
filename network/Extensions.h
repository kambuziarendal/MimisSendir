/*
 * Extensions.h
 * Copyright (C) 2019-2025 Adrien Destugues <pulkomandy@pulkomandy.tk>
 *
 * Distributed under terms of the MIT license.
 */

#pragma once

enum {
	ExtBase = gloox::ExtUser,
	ExtBOB,
	ExtMedia,
	ExtReferences, // XEP-0372 references
	ExtConfirm, // XEP-0070 "confirm" element
	ExtHttpUpload // XEP-0363 HTTP upload
};

const std::string XMLNS_BOB               = "urn:xmpp:bob";
#define XMLNS_PEP_BOOKMARKS                 "urn:xmpp:bookmarks:1"
const std::string XMLNS_MEDIA             = "urn:xmpp:media-element";
const std::string XMLNS_REFERENCES        = "urn:xmpp:reference:0";
const std::string XMLNS_HTTP_AUTH         = "http://jabber.org/protocol/http-auth";
