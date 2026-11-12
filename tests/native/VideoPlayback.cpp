// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "ui/VideoPlayback.h"
#include <Application.h>
#include <OS.h>
#include <cstdio>
#include <functional>
#include <cstdlib>

static int checks = 0;
static void Check(bool value, const char* message)
{
    ++checks;
    if (!value) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}

struct Probe {
    std::shared_ptr<BBitmap> image;
    uint64 generation = 0;
    VideoPlayback::Snapshot state;
    bool Until(VideoPlayback& player, std::function<bool()> predicate, bigtime_t timeout = 10000000)
    {
        const auto limit = system_time() + timeout;
        do {
            state = player.Read(image, generation);
            if (predicate()) return true;
            if (!state.error.empty()) { fprintf(stderr, "%s\n", state.error.c_str()); return false; }
            snooze(20000);
        } while (system_time() < limit);
        fprintf(stderr, "timeout position=%lld duration=%lld frames=%llu audio=%llu ready=%d paused=%d ended=%d finished=%d\n",
            (long long)state.position, (long long)state.duration,
            (unsigned long long)state.frames, (unsigned long long)state.audioFrames,
            state.ready,state.paused,state.ended,state.finished);
        return false;
    }
};

int main(int argc, char** argv)
{
    BApplication app("application/x-vnd.Kambuzi-InlineVideoTests");
    if (argc != 3) { fprintf(stderr, "usage: VideoTests audio-video.mp4 silent-video.mp4\n"); return 2; }
    std::atomic_bool cancel{false};
    auto poster = VideoPlayback::LoadPoster(argv[1], cancel);
    Check(poster && poster->InitCheck() == B_OK, "audio-video poster without playback");
    Check(poster->Bounds().Width() < 480 && poster->Bounds().Height() < 270, "poster dimensions bounded");
    Check(bool(VideoPlayback::LoadPoster(argv[2], cancel)), "silent-video poster");
    cancel = true;
    Check(!VideoPlayback::LoadPoster(argv[1], cancel), "cancelled poster is not decoded");
    cancel = false;
    Check(!VideoPlayback::LoadPoster("/missing-poster.mp4", cancel), "invalid poster fails safely");
    auto lease = std::make_shared<int>(1);
    std::weak_ptr<int> weak = lease;
    VideoPlayback player(argv[1], lease);
    lease.reset();
    Probe p;
    Check(p.Until(player, [&] { return p.state.position >= 500000 && p.state.frames > 2; }), "video advances");
    Check(p.state.frames <= 15, "15 fps fixture does not decode at packet-tick speed");
    Check(p.image && p.image->InitCheck() == B_OK, "preview bitmap");
    Check(p.image->Bounds().Width() < 480 && p.image->Bounds().Height() < 270, "bounded preview");
    Check(p.state.audio && p.state.audioFrames > 0, "decoded PCM reaches sound callback");
    Check(!weak.expired(), "download retained while decoding");
    auto backgroundPoster = VideoPlayback::LoadPoster(argv[2], cancel);
    Check(bool(backgroundPoster), "poster can load alongside active video");
    Check(!player.Read(p.image, p.generation).stopped, "poster loading does not supersede player");
    player.Pause(true);
    snooze(100000);
    auto paused = player.Read(p.image, p.generation);
    snooze(300000);
    auto stable = player.Read(p.image, p.generation);
    Check(stable.paused && paused.position == stable.position && paused.frames == stable.frames
        && paused.audioFrames == stable.audioFrames, "pause freezes video and PCM");
    player.Seek(5000000);
    Check(p.Until(player, [&] { return p.state.position >= 5000000 && p.state.frames > stable.frames; }), "seek while paused decodes target");
    Check(p.state.paused, "seek preserves pause");
    const auto beforeEndSeek = p.state.frames;
    player.Seek(p.state.duration + 10000000);
    Check(p.Until(player, [&] { return p.state.frames > beforeEndSeek && p.state.position == p.state.duration - 1; }),
        "seek beyond end shows the last frame while paused");
    player.Seek(5000000);
    Check(p.Until(player, [&] { return p.state.position == 5000000; }), "return from end while paused");
    player.Mute(true); player.Pause(false);
    const uint64 audioBefore = p.state.audioFrames;
    Check(p.Until(player, [&] { return p.state.position >= 5500000 && p.state.audioFrames > audioBefore; }), "muted playback still advances");
    Check(p.state.muted, "mute retained");
    player.Seek(0); player.Mute(false);
    Check(p.Until(player, [&] { return p.state.position < 500000; }), "backward seek resets clock");
    Check(p.Until(player, [&] { return p.state.position > 600000; }), "resume after backward seek");
    player.Seek(p.state.duration - 500000);
    Check(p.Until(player, [&] { return p.state.ended; }), "end of video pauses");
    player.Seek(0); player.Pause(false);
    Check(p.Until(player, [&] { return !p.state.ended && p.state.position < 1000000; }), "replay");
    // Starting another player must stop the previous conversation's audio.
    VideoPlayback silent(argv[2], nullptr);
    Check(p.Until(player, [&] { return p.state.finished; }), "superseded player shuts down");
    Check(weak.expired(), "download lease released after decoder cleanup");
    Probe q;
    Check(q.Until(silent, [&] { return q.state.position >= 500000 && q.state.frames > 2; }), "silent video advances on wall clock");
    Check(!q.state.audio, "silent clip has no audio output");
    silent.Stop();
    Check(q.Until(silent, [&] { return q.state.finished; }), "explicit close finishes worker");
    VideoPlayback invalid("/does-not-exist-video.mp4", nullptr);
    Probe bad;
    Check(bad.Until(invalid, [&] { return bad.state.finished && !bad.state.error.empty(); }), "invalid file returns visible error");
    printf("%d inline video checks passed\n", checks);
    return 0;
}
