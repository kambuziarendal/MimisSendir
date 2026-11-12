// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include "HttpUpload.h"
#include <atomic>
#include <cstdio>
#include <functional>

namespace Upload {
struct TransferResult {
	bool success = false;
	long httpStatus = 0;
	std::string error;
	uint64_t bytes = 0;
	std::string contentType;
};

// Call only from a transfer worker. Caller retains FILE ownership and must keep
// it open until return. The file must be seekable and regular. No credentials,
// response bodies or signed URLs are included in errors.
TransferResult PutFile(const Slot& slot, const std::string& authority,
	FILE* file, uint64_t size, const std::string& contentType,
	const std::atomic_bool& cancelled,
	const std::function<void(uint64_t, uint64_t)>& progress = {},
	const std::string& caFile = {});

// Destination must be a new, empty regular file opened for writing. Failed or
// cancelled transfers are truncated to zero. Callers must not decode/open the
// file until success, and must unlink their temporary file after failure.
TransferResult GetFile(const std::string& url, const std::string& authority,
	FILE* destination, const std::atomic_bool& cancelled,
	const std::function<void(uint64_t, uint64_t)>& progress = {},
	const std::string& caFile = {}, uint64_t limit = kMaxFileSize);
}
