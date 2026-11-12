#include "FileXMLReader.h"

#include <gloox/parser.h>

#include <storage/Directory.h>
#include <storage/File.h>
#include <storage/FindDirectory.h>
#include <storage/StorageDefs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <vector>
#include <memory>

#include "XMLEntity.h"

FileXMLReader::FileXMLReader(const char *filename, bool in_settings) {
	_entity_tree = NULL;

	_path_name = (char *)malloc(B_PATH_NAME_LENGTH * sizeof(char));
	
	// generate full pathname
	if (in_settings) {
		// locate settings file
		find_directory(B_USER_SETTINGS_DIRECTORY, 0, false, _path_name, B_PATH_NAME_LENGTH - strlen(filename) + 1);

		strcat(_path_name, "/");
		strcat(_path_name, filename);
	} else {
		strcpy(_path_name, filename);
	}

	// make sure the directory exists
	CreatePath();

	// open file (or create)
	BFile *_fh = new BFile();

	if (_fh->SetTo(_path_name, B_READ_ONLY) == B_NO_ERROR) {
		int bytes_read;
		char buffer[1001];
		gloox::Parser parser(this, false);
		bool error = false;

		while((bytes_read = _fh->Read(buffer, 1000)) > 0) {
			std::string tmp(buffer, bytes_read);
			if (parser.feed(tmp) >= 0)
				error = true;
		}
		
		if (bytes_read < 0 || error || !_entity_tree) {
			_entity_tree = new gloox::Tag("container");
			_file_status = FILE_CORRUPTED;

			// immediately save a fresh copy
			WriteToFile();
		} else {
			_file_status = FILE_OK;
		}
	} else {
		_entity_tree = new gloox::Tag("container");
		_file_status = FILE_NOT_FOUND;
		
		// immediately save a fresh copy
		WriteToFile();
	}

	delete _fh;
}

FileXMLReader::~FileXMLReader() {
	WriteToFile();

	free(_path_name);
	delete _entity_tree;
}

void FileXMLReader::CreatePath() {
	// create directory to make sure it's there
	string directory = _path_name;
	
	// extract directory name
	string::size_type last_slash_pos = directory.rfind("/");

	// create the directory
	if (last_slash_pos != string::npos) {
		create_directory(directory.substr(0, last_slash_pos).c_str(), 0744);
	}
}	

void FileXMLReader::handleTag(gloox::Tag *entity) {
	if (entity->name() == "container")
		_entity_tree = entity;
}

std::optional<std::string> FileXMLReader::Data(const char *key) {
	std::lock_guard<std::recursive_mutex> guard(_lock);
	if (!_entity_tree) {
		return std::nullopt;
	}

	if (_entity_tree->findChild(key)) {
		// buffer 0 length data
		const std::string& data = _entity_tree->findChild(key)->cdata();
		
		if (!data.empty()) {
			// FIXME address of temporary
			return data;
		} else {
			return std::nullopt;
		}
	} else {
		return std::nullopt;
	}
}

void FileXMLReader::SetData(const char *key, const char *value) {
	std::lock_guard<std::recursive_mutex> guard(_lock);
	if (!_entity_tree) {
		return;
	}

	if (_entity_tree->findChild(key)) {
		_entity_tree->findChild(key)->setCData(value);
	} else {
		_entity_tree->addChild(new gloox::Tag((gloox::Tag*)NULL, key, value));
	}
}

void FileXMLReader::SetIntData(const char *key, int value) {
	char buffer[64];
	
	// translate integer into string
	sprintf(buffer, "%d", value);
	
	// set the data
	SetData(key, buffer);
}

void FileXMLReader::SetFloatData(const char *key, float value) {
	char buffer[64];
	
	// translate float into string
	sprintf(buffer, "%.0f", value);
	
	// set the data
	SetData(key, buffer);
}

void FileXMLReader::RemoveData(const char* key)
{
	std::lock_guard<std::recursive_mutex> guard(_lock);
	if (!_entity_tree) {
		return;
	}

	if (_entity_tree->findChild(key)) {
		_entity_tree->removeChild(key);
	}
}

bool FileXMLReader::Tag(const char *key) {
	std::lock_guard<std::recursive_mutex> guard(_lock);
	if (!_entity_tree) {
		return false;
	}

	return _entity_tree->findChild(key);
}

void FileXMLReader::SetTag(const char *key, bool enabled) {
	std::lock_guard<std::recursive_mutex> guard(_lock);
	if (!_entity_tree) {
		return;
	}

	if (enabled) {
		// set the value
		if (_entity_tree->findChild(key) == NULL) {
			_entity_tree->addChild(new gloox::Tag((gloox::Tag*)NULL, key));
		}
	} else {
		// clear the value
		if (_entity_tree->findChild(key) != NULL) {
			_entity_tree->removeChild(key);
		}
	}
}

FileXMLReader::file_status FileXMLReader::FileStatus() {
	return _file_status;
}

bool FileXMLReader::WriteTree(const gloox::Tag& tree) {
	// Same-directory rename prevents a failed/partial write from destroying the
	// previous profile. mkstemp creates a private (0600) file, never a fixed name.
	std::string pattern = std::string(_path_name) + ".tmp-XXXXXX";
	std::vector<char> path(pattern.begin(), pattern.end());
	path.push_back('\0');
	int fd = mkstemp(path.data());
	if (fd < 0) return false;
	const auto buffer = tree.xml();
	size_t offset = 0;
	bool ok = true;
	while (offset < buffer.size()) {
		ssize_t written = write(fd, buffer.data() + offset, buffer.size() - offset);
		if (written < 0 && errno == EINTR) continue;
		if (written <= 0) { ok = false; break; }
		offset += written;
	}
	if (ok && fsync(fd) != 0) ok = false;
	if (close(fd) != 0) ok = false;
	if (ok && rename(path.data(), _path_name) != 0) ok = false;
	if (!ok) unlink(path.data());
	return ok;
}

bool FileXMLReader::WriteToFile() {
	std::lock_guard<std::recursive_mutex> guard(_lock);
	return _entity_tree && WriteTree(*_entity_tree);
}

bool FileXMLReader::SaveChanges(const std::map<std::string, std::optional<std::string>>& changes) {
	std::lock_guard<std::recursive_mutex> guard(_lock);
	if (!_entity_tree) return false;
	std::unique_ptr<gloox::Tag> candidate(_entity_tree->clone());
	for (const auto& [key, value] : changes) {
		candidate->removeChild(key);
		if (value) candidate->addChild(new gloox::Tag(nullptr, key, *value));
	}
	if (!WriteTree(*candidate)) return false;
	delete _entity_tree;
	_entity_tree = candidate.release();
	return true;
}

const gloox::Tag& FileXMLReader::EntityTree() {
	return *_entity_tree;
}
