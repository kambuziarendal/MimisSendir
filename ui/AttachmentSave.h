// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace Upload {
// The caller supplies an already downloaded private regular FILE. Never
// overwrite a destination or execute it. Return an empty string on success.
inline std::string SaveAttachmentExclusive(FILE* source, const std::string& path,
	const std::atomic_bool& cancelled)
{
	int fd = -1;
	std::string error;
	try {
		if (cancelled) throw std::runtime_error("Save cancelled.");
		fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
		if (fd < 0) throw std::runtime_error("Destination exists or is not writable. Choose a new filename.");
		rewind(source);
		unsigned char buffer[65536];
		size_t count;
		while ((count = fread(buffer, 1, sizeof(buffer), source)) != 0) {
			if (cancelled) throw std::runtime_error("Save cancelled.");
			size_t offset = 0;
			while (offset < count) {
				ssize_t written = write(fd, buffer + offset, count - offset);
				if (written < 0 && errno == EINTR) continue;
				if (written <= 0) throw std::runtime_error("Could not write attachment.");
				offset += written;
			}
		}
		if (ferror(source) || fsync(fd) != 0 || cancelled)
			throw std::runtime_error("Save incomplete or cancelled.");
	} catch (const std::exception& failure) {
		error = failure.what();
		struct stat opened, current;
		if (fd >= 0 && fstat(fd, &opened) == 0 && lstat(path.c_str(), &current) == 0
			&& opened.st_dev == current.st_dev && opened.st_ino == current.st_ino) unlink(path.c_str());
	}
	if (fd >= 0) close(fd);
	return error;
}
}
