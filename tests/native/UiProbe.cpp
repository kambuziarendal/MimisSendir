// Measures native UI message-loop latency. Synthetic application fixture only.
#include <Application.h>
#include <Message.h>
#include <Messenger.h>
#include <OS.h>
#include <algorithm>
#include <atomic>
#include <string>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr bigtime_t kSendTimeout = 1000000;
constexpr char kComposer[] = "Message Input";

status_t SendQuery(const BMessenger& target, BMessage& query, BMessage& reply)
{
    return target.SendMessage(&query, &reply, kSendTimeout, kSendTimeout);
}

status_t ReadDraft(const BMessenger& window, int32 limit, std::string& draft)
{
    BMessage query(B_GET_PROPERTY), reply;
    query.AddSpecifier("Text", int32(0), limit);
    query.AddSpecifier("View", kComposer);
    const status_t status = SendQuery(window, query, reply);
    if (status != B_OK)
        return status;
    const char* text = nullptr;
    const status_t result = reply.FindString("result", &text);
    if (result == B_OK)
        draft = text;
    return result;
}

BMessenger FindComposerWindow(const BMessenger& app)
{
    BMessage query(B_GET_PROPERTY), reply;
    query.AddSpecifier("Windows");
    if (SendQuery(app, query, reply) != B_OK)
        return {};
    BMessenger window;
    for (int32 i = 0; reply.FindMessenger("result", i, &window) == B_OK; ++i) {
        BMessage textLength(B_COUNT_PROPERTIES), answer;
        textLength.AddSpecifier("Text");
        textLength.AddSpecifier("View", kComposer);
        int32 count = 0;
        if (SendQuery(window, textLength, answer) == B_OK
            && answer.FindInt32("result", &count) == B_OK) {
            return window;
        }
    }
    return {};
}

status_t SendKey(const BMessenger& window, char key)
{
    const char bytes[]{key, '\0'};
    BMessage event(B_KEY_DOWN);
    event.AddInt64("when", system_time());
    event.AddInt8("byte", key);
    event.AddInt32("raw_char", key);
    event.AddInt32("modifiers", 0);
    event.AddString("bytes", bytes);
    event.AddSpecifier("View", kComposer);
    return window.SendMessage(&event, static_cast<BHandler*>(nullptr), kSendTimeout);
}

void Report(const char* label, std::vector<bigtime_t>& times)
{
    if (times.empty())
        return;
    std::sort(times.begin(), times.end());
    printf("%s samples=%zu p50_us=%lld p95_us=%lld max_us=%lld\n", label,
        times.size(), (long long)times[times.size() / 2],
        (long long)times[(times.size() - 1) * 95 / 100], (long long)times.back());
}

int MeasureTyping(const BMessenger& app, int count, int intervalMs)
{
    if (count < 1 || count > 2000 || intervalMs < 1 || intervalMs > 1000)
        return 2;
    const BMessenger window = FindComposerWindow(app);
    if (!window.IsValid()) {
        fprintf(stderr, "No open chat composer found. Open a synthetic conversation.\n");
        return 2;
    }

    std::string draft;
    if (ReadDraft(window, count + 1, draft) != B_OK || !draft.empty()) {
        fprintf(stderr, "The synthetic composer must be readable and empty.\n");
        return 2;
    }

    const std::string pattern = "the quick brown fox jumps over the lazy dog ";
    std::string expected;
    for (int i = 0; i < count; ++i)
        expected += pattern[i % pattern.size()];

    std::vector<bigtime_t> submitted(count);
    std::vector<bigtime_t> observed(count);
    std::vector<bigtime_t> roundTrips;
    std::atomic<int> failedKeys{0};
    const bigtime_t interval = intervalMs * 1000;
    const bigtime_t start = system_time() + 100000;
    printf("READY typing keys=%d interval_ms=%d\n", count, intervalMs);
    fflush(stdout);

    // Generate input independently of the synchronous draft queries. A slow
    // window must not reduce the offered key rate and hide a queue backlog.
    std::thread producer([&] {
        for (int i = 0; i < count; ++i) {
            snooze_until(start + i * interval, B_SYSTEM_TIMEBASE);
            submitted[i] = system_time();
            if (SendKey(window, expected[i]) != B_OK)
                ++failedKeys;
        }
    });

    const bigtime_t finish = start + (count - 1) * interval;
    const bigtime_t deadline = finish + 10000000;
    size_t visible = 0;
    int failedQueries = 0;
    bool mismatch = false;
    while (system_time() < deadline && visible < expected.size() && !mismatch) {
        const bigtime_t queried = system_time();
        const status_t status = ReadDraft(window, count + 1, draft);
        const bigtime_t now = system_time();
        if (status == B_OK) {
            if (queried >= start && queried <= finish)
                roundTrips.push_back(now - queried);
            mismatch = draft.size() < visible || expected.compare(0, draft.size(), draft) != 0;
            if (!mismatch) {
                while (visible < draft.size())
                    observed[visible++] = now;
            }
        } else
            ++failedQueries;
        snooze(5000);
    }
    producer.join();

    std::vector<bigtime_t> inputLatency;
    std::vector<bigtime_t> scheduling;
    for (size_t i = 0; i < visible; ++i) {
        inputLatency.push_back(observed[i] - submitted[i]);
        scheduling.push_back(submitted[i] - (start + i * interval));
    }
    Report("Composer key-to-observed upper bound", inputLatency);
    Report("Composer looper query during typing", roundTrips);
    Report("Producer enqueue scheduling lag", scheduling);
    printf("Composer characters=%zu/%d key_failures=%d query_failures=%d exact=%s\n",
        visible, count, failedKeys.load(), failedQueries, !mismatch && draft == expected ? "yes" : "no");
    return mismatch || draft != expected || failedKeys != 0 || failedQueries != 0 ? 1 : 0;
}

} // namespace

int main(int argc, char** argv)
{
    BApplication app("application/x-vnd.Kambuzi-Renga-UiProbe");
    BMessenger target("application/x-vnd.Kambuzi-MimisSendir");
    if (!target.IsValid()) return 2;
    if (argc > 1 && std::string(argv[1]) == "--typing") {
        if (argc > 4) return 2;
        return MeasureTyping(target, argc > 2 ? std::atoi(argv[2]) : 200,
            argc > 3 ? std::atoi(argv[3]) : 20);
    }
    int count = argc > 1 ? std::atoi(argv[1]) : 100;
    if (count < 1 || count > 10000) return 2;
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
