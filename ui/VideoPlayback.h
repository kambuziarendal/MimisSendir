// Copyright 2026 Kambuzi. Distributed under the MIT license.
#pragma once
#include <Bitmap.h>
#include <atomic>
#include <memory>
#include <string>

// Local-file playback only. The decoder, media server and cleanup never run on
// a window looper. Snapshot copies only the small preview, not decoded frames.
class VideoPlayback {
public:
    struct Snapshot {
        bigtime_t position = 0, duration = 0;
        bool ready = false, paused = false, ended = false, muted = false;
        bool audio = false, stopped = false, finished = false;
        uint64 frames = 0, audioFrames = 0;
        std::string error;
    };
    VideoPlayback(const std::string& path, std::shared_ptr<void> fileLifetime);
    ~VideoPlayback();
    VideoPlayback(const VideoPlayback&) = delete;
    VideoPlayback& operator=(const VideoPlayback&) = delete;
    // Worker-only poster decoding: no audio output or active-player ownership.
    static std::shared_ptr<BBitmap> LoadPoster(const std::string& path, const std::atomic_bool& cancelled);
    void Pause(bool paused);
    void Seek(bigtime_t position);
    void Mute(bool muted);
    void Stop();
    Snapshot Read(std::shared_ptr<BBitmap>& preview, uint64& generation);
private:
    struct State;
    std::shared_ptr<State> fState;
    static void Run(std::shared_ptr<State> state);
};
