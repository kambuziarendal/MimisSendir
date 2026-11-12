#include "ui/AttachmentSave.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/stat.h>
static int checks;
static void Check(bool ok) { ++checks; if (!ok) std::abort(); }
int main()
{
	char pattern[] = "/tmp/renga-save-test-XXXXXX";
	char* directory = mkdtemp(pattern); Check(directory != nullptr);
	std::string base(directory), target = base + "/saved.txt", link = base + "/link.txt";
	FILE* source = tmpfile(); Check(source != nullptr);
	const char content[] = "Synthetic attachment\n";
	Check(fwrite(content, 1, sizeof(content) - 1, source) == sizeof(content) - 1);
	Check(fflush(source) == 0);
	std::atomic_bool cancelled{false};
	Check(Upload::SaveAttachmentExclusive(source, target, cancelled).empty());
	struct stat st; Check(stat(target.c_str(), &st) == 0 && (st.st_mode & 0777) == 0600);
	Check(st.st_size == static_cast<off_t>(sizeof(content) - 1));
	FILE* read = fopen(target.c_str(), "rb"); char data[64]{};
	Check(read && fread(data, 1, sizeof(data), read) == sizeof(content) - 1); fclose(read);
	Check(!strcmp(data, content));
	// A repeat save must not truncate an existing file, even if a dialog approved replacement.
	Check(!Upload::SaveAttachmentExclusive(source, target, cancelled).empty());
	Check(stat(target.c_str(), &st) == 0 && st.st_size == static_cast<off_t>(sizeof(content) - 1));
	Check(symlink(target.c_str(), link.c_str()) == 0);
	Check(!Upload::SaveAttachmentExclusive(source, link, cancelled).empty());
	Check(lstat(link.c_str(), &st) == 0 && S_ISLNK(st.st_mode));
	Check(!Upload::SaveAttachmentExclusive(source, base, cancelled).empty());
	cancelled = true;
	Check(!Upload::SaveAttachmentExclusive(source, base + "/cancelled.txt", cancelled).empty());
	Check(access((base + "/cancelled.txt").c_str(), F_OK) != 0);
	fclose(source); unlink(link.c_str()); unlink(target.c_str()); rmdir(directory);
	std::cout << checks << " exclusive attachment save checks passed\n";
}
