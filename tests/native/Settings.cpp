// Verify transactional profile writes and input boundaries on Haiku.
#include "jabber/FileXMLReader.h"
#include "support/SettingsValidation.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <thread>
#include <atomic>
static std::string Read(const std::string& path) {
    std::ifstream file(path); return {std::istreambuf_iterator<char>(file), {}};
}
int main(int argc, char** argv) {
    assert(argc == 2);
    std::string path = std::string(argv[1]) + "/settings-test.xml";
    assert(access(path.c_str(), F_OK) != 0);
    {
        std::ofstream out(path);
        out << "<container><unknown attr='keep'><child>nested</child></unknown>"
            "<server>old.example.org</server><flag/></container>";
    }
    {
        FileXMLReader settings(path.c_str());
        assert(settings.FileStatus() == FileXMLReader::FILE_OK);
        assert(settings.SaveChanges({{"server", "new.example.org"}, {"flag", std::nullopt}, {"auto-login", ""}}));
        assert(settings.Data("server") == "new.example.org");
        assert(settings.Tag("auto-login") && !settings.Tag("flag"));
        auto saved = Read(path);
        assert(saved.find("nested") != std::string::npos && saved.find("keep") != std::string::npos);
        struct stat info; assert(stat(path.c_str(), &info) == 0 && (info.st_mode & 0777) == 0600);
        // Force rename failure after the entire temporary file was written.
        std::string retained = path + ".retained";
        assert(rename(path.c_str(), retained.c_str()) == 0);
        assert(mkdir(path.c_str(), 0700) == 0);
        assert(!settings.SaveChanges({{"server", "should-not-apply.example.org"}}));
        assert(settings.Data("server") == "new.example.org");
        assert(Read(retained) == saved);
        assert(rmdir(path.c_str()) == 0);
        assert(rename(retained.c_str(), path.c_str()) == 0);
        // The network thread may read while the settings window commits a clone.
        std::atomic<bool> running{true};
        std::thread reader([&] { while (running) { assert(settings.Data("server")); settings.Tag("auto-login"); } });
        for (int i = 0; i < 20; ++i) assert(settings.SaveChanges({{"counter", std::to_string(i)}}));
        running = false; reader.join();
    }
    {
        FileXMLReader reloaded(path.c_str());
        assert(reloaded.Data("server") == "new.example.org");
        assert(reloaded.Data("counter") == "19");
        assert(reloaded.Tag("unknown") && reloaded.Tag("auto-login"));
    }
    unlink(path.c_str());
    using namespace SettingsValidation;
    for (auto value : {"1", "5222", "65535"}) assert(Port(value));
    for (auto value : {"", "0", "65536", "-1", "5222x", " 5222", "999999999"}) assert(!Port(value));
    for (auto value : {"signal.example.org", "localhost", "xn--bcher-kva.example"}) assert(Domain(value));
    for (auto value : {"", "https://signal.example", "user@example.org", "example.org/room", "a..b", "-a.b", "a-.b", "a b", "a.b."}) assert(!Domain(value));
    assert(Host("::1") && Host("127.0.0.1") && !Host("[::1]"));
    for (auto value : {"upload.example.org", "upload.example.org:443", "[::1]:8443", "[::1]"}) assert(Authority(value));
    for (auto value : {"https://upload.example", "u.example:0", "u.example:70000", "u.example/a", "u@h", "::1", "[not-ip]:443"}) assert(!Authority(value));
    puts("Settings: transactional failure/reload/concurrent reads and input boundaries passed");
}
