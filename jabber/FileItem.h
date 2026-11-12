//////////////////////////////////////////////////
// Blabber [FileItem.h]
//     A list item pointing to a file.
//////////////////////////////////////////////////

#ifndef FILE_ITEM_H
#define FILE_ITEM_H

#ifndef __STRING__
	#include <string>
#endif

#ifndef _MENU_ITEM_H
	#include <interface/MenuItem.h>
#endif

class FileItem : public BMenuItem {
public:
	        FileItem(const char *label, const char *filename, BMessage *msg,
	        	char shortcut = 0, uint32 modifiers = 0);

	std::string  Filename();

private:
	std::string _filename;
};

#endif
