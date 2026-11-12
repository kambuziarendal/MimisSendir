// Copyright 2026 Kambuzi EDB. Distributed under the MIT license.
#pragma once
#include <Message.h>
#include <Messenger.h>
#include <string>
namespace ServiceKeys {
constexpr uint32 kResult = 'gKey';
bool ValidGiphyKey(const std::string& key);
bool ValidIdentifier(const std::string& id);
// Only call Load on a worker: KeyStore permission prompts may block.
std::string Load(const std::string& id, std::string& error);
// Store creates a new key first. The caller commits its non-secret identifier
// before asking to remove the previous key. Replies never contain secrets.
bool Submit(BMessage request, const BMessenger& target);
bool Idle();
void Shutdown();
}
