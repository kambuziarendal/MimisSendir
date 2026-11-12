// Measures native UI message-loop latency. Synthetic application fixture only.
#include <Application.h>
#include <Message.h>
#include <Messenger.h>
#include <OS.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv)
{
    BApplication app("application/x-vnd.Kambuzi-Renga-UiProbe");
    int count = argc > 1 ? std::atoi(argv[1]) : 100;
    if (count < 1 || count > 10000) return 2;
    BMessenger target("application/x-vnd.Haiku-Jabber");
    if (!target.IsValid()) return 2;
    std::vector<bigtime_t> times;
    int failed = 0;
    for (int i=0; i<count; ++i) {
        BMessage query(B_GET_PROPERTY), reply;
        query.AddSpecifier("Title");
        query.AddSpecifier("Window", int32(0));
        const auto started = system_time();
        const auto status = target.SendMessage(&query, &reply, 1000000, 1000000);
        const char* title = nullptr;
        if (status != B_OK || reply.FindString("result", &title) != B_OK) ++failed;
        else times.push_back(system_time() - started);
        snooze(50000);
    }
    if (times.empty()) return 1;
    std::sort(times.begin(),times.end());
    printf("Native UI-loop samples=%zu failures=%d p50_us=%lld p95_us=%lld max_us=%lld\n",
        times.size(), failed, (long long)times[times.size()/2],
        (long long)times[(times.size()-1)*95/100], (long long)times.back());
    return failed || times.back() > 200000 ? 1 : 0;
}
