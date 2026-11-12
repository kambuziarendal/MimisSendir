// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include "AttachmentMetadata.h"
#include "Reactions.h"
#include "Correction.h"
#include <gloox/error.h>

namespace Chat {
enum class MessageKind { Ignore, Content, DeliveryError, Reaction, Correction };

inline MessageKind Classify(const gloox::Message& message)
{
    // Errors may echo the original body/OOB payload. They are never replies
    // from the contact and must never trigger an attachment download.
    if (message.subtype() == gloox::Message::Error || message.error())
        return MessageKind::DeliveryError;
    if (message.subtype() != gloox::Message::Chat
        && message.subtype() != gloox::Message::Normal)
        return MessageKind::Ignore;
    if (message.findExtension<Correction>(Correction::kExtension)) return MessageKind::Correction;
    if (message.findExtension<Reactions>(Reactions::kExtension)) return MessageKind::Reaction;
    // Receipts, typing states and other bodyless control stanzas are not chat.
    return Upload::AttachmentBody(message).empty()
        ? MessageKind::Ignore : MessageKind::Content;
}
}
