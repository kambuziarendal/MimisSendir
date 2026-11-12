#ifndef FILE_XML_READER_H
#define FILE_XML_READER_H

#include "gloox/taghandler.h"

#include <optional>
#include <map>
#include <mutex>

class FileXMLReader : public gloox::TagHandler {
public:
	enum file_status {FILE_OK, FILE_NOT_FOUND, FILE_CORRUPTED};
	
public:
	                  FileXMLReader(const char *filename, bool in_settings = false);
	                 ~FileXMLReader();

	void			  CreatePath();
	
	void              handleTag(gloox::Tag *entity) override;

	std::optional<std::string> Data(const char *key);
	void              SetData(const char *key, const char *value);
	void              SetIntData(const char *key, int value);
	void              SetFloatData(const char *key, float value);
	void              RemoveData(const char* key);

	bool              Tag(const char *key);
	void              SetTag(const char *key, bool enabled);

	file_status       FileStatus();
		
	bool              WriteToFile();
	// A missing value removes a key; an empty string keeps a presence-only flag.
	// Commit to disk before changing the live settings tree.
	bool              SaveChanges(const std::map<std::string, std::optional<std::string>>& changes);
	
	const	gloox::Tag&	EntityTree();
	
private:
	bool            WriteTree(const gloox::Tag& tree);
	std::recursive_mutex _lock;
	gloox::Tag*      _entity_tree;
	char            *_path_name;
	file_status      _file_status;
};

#endif
