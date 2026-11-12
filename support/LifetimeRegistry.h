// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <cstdint>
#include <map>
#include <mutex>

// Queued native UI messages carry raw addresses, so address membership alone
// cannot distinguish an object from a later allocation at the same address.
class LifetimeRegistry {
public:
	void Remember(const void* object) {
		std::lock_guard<std::mutex> lock(fLock);
		fTokens[object] = fNextToken++;
	}
	void Forget(const void* object) {
		std::lock_guard<std::mutex> lock(fLock);
		fTokens.erase(object);
	}
	uint64_t Token(const void* object) const {
		std::lock_guard<std::mutex> lock(fLock);
		auto found = fTokens.find(object);
		return found == fTokens.end() ? 0 : found->second;
	}
	bool Matches(const void* object, uint64_t token) const {
		return token != 0 && Token(object) == token;
	}
	void Clear() {
		std::lock_guard<std::mutex> lock(fLock);
		fTokens.clear();
	}
private:
	mutable std::mutex fLock;
	std::map<const void*, uint64_t> fTokens;
	uint64_t fNextToken = 1;
};
