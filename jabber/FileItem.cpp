//////////////////////////////////////////////////
// Blabber [FileItem.cpp]
//////////////////////////////////////////////////

#ifndef FILE_ITEM_H
	#include "FileItem.h"
#endif

FileItem::FileItem(const char *label, const char *filename, BMessage *msg,
	char shortcut, uint32 modifiers)
	: BMenuItem(label, msg, shortcut, modifiers) {
	_filename = filename;
}

std::string FileItem::Filename()	{
	return _filename;
}
