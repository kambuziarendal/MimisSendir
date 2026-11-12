// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <gloox/message.h>
#include <string>
namespace Upload {
// Preserve OOB-only attachments and captions even when inline fetching is not allowed.
std::string AttachmentBody(const gloox::Message& message);
// Only an exact configured HTTPS authority is eligible for native downloading.
std::string AttachmentUrl(const gloox::Message& message, const std::string& authority);
std::string AttachmentType(const std::string& filename);
std::string AttachmentDisplayName(const std::string& url);
std::string BodyAttachmentUrl(const std::string& body, const std::string& authority);
bool IsVideoFilename(const std::string& url);
bool IsPreviewFilename(const std::string& url);
std::string AttachmentFilename(const std::string& url);
std::string DisplayAttachmentBody(const std::string& body, const std::string& attachment);
}
