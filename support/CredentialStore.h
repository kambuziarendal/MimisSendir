// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Message.h>
#include <Messenger.h>

namespace Credentials {
constexpr uint32 kResult = 'kRes';
// One process-owned worker, at most four queued/active operations. UI callers
// never wait for a keyring permission prompt. Results contain sensitive fields:
// keep them in memory and never PrintToStream/log them.
bool Submit(BMessage request, const BMessenger& target);
}
