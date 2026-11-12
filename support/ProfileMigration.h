// Copyright 2026 Kambuzi EDB. Distributed under the MIT license.
#pragma once
#include <string>

namespace ProfileMigration {
// Copy into a private staging directory, then publish. Never modifies source
// or an existing destination. Symlinks/special files and changing input fail.
bool Copy(const std::string& source, const std::string& destination, std::string& error);
}
