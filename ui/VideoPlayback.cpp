// Copyright 2026 Kambuzi. Distributed under the MIT license.
#include "VideoPlayback.h"
#include <Entry.h>
#include <MediaFile.h>
#include <MediaTrack.h>
#include <SoundPlayer.h>
#include <OS.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
std::atomic<unsigned> sDecoders{0};
constexpr size_t kMaxAudioBuffer = 128 * 1024;
constexpr bigtime_t kMaxDuration = 24LL * 60 * 60 * 1000000;
void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
}

struct VideoPlayback::State {
    std::string path;
    std::shared_ptr<void> fileLifetime;
    std::atomic_bool stop{false}, paused{false}, muted{false}, seeking{true}, finished{false};
    std::atomic<bigtime_t> seek{-1}, position{0}, duration{0};
    std::mutex pixelsLock;
    Snapshot snapshot;
    std::unique_ptr<BBitmap> preview;
    uint64 generation = 0;

    // Fixed PCM ring. Only memcpy and counters in the sound callback. Decoder
    // and media-server calls are never made while holding this lock.
    std::mutex audioLock;
    std::vector<uint8> pcm;
    size_t read = 0, used = 0, frameBytes = 0;
    double rate = 0;
    bigtime_t audioBase = 0;
    uint64 consumed = 0;
    std::atomic<uint64> audioFrames{0};

    static void Audio(void* cookie, void* buffer, size_t size,
        const media_raw_audio_format& format)
    {
        (void)format;
        auto& self = *static_cast<State*>(cookie);
        memset(buffer, 0, size);
        if (self.stop || self.paused || self.seeking || self.seek >= 0) return;
        std::unique_lock<std::mutex> lock(self.audioLock, std::try_to_lock);
        if (!lock.owns_lock() || !self.frameBytes) return;
        size_t count = std::min(size, self.used);
        count -= count % self.frameBytes;
        const size_t first = std::min(count, self.pcm.size() - self.read);
        if (!self.muted) {
            memcpy(buffer, self.pcm.data() + self.read, first);
            memcpy(static_cast<uint8*>(buffer) + first, self.pcm.data(), count - first);
        }
        self.read = (self.read + count) % self.pcm.size();
        self.used -= count;
        self.consumed += count / self.frameBytes;
        self.audioFrames += count / self.frameBytes;
    }
};

VideoPlayback::VideoPlayback(const std::string& path, std::shared_ptr<void> lifetime)
    : fState(std::make_shared<State>())
{
    // One active video across all conversations. An old decoder can finish
    // shutting down in the background; at most two workers may coexist.
    static std::mutex activeLock;
    static std::weak_ptr<State> active;
    std::lock_guard<std::mutex> lock(activeLock);
    if (auto old = active.lock()) old->stop = true;
    active = fState;
    fState->path = path;
    fState->fileLifetime = std::move(lifetime);
    if (sDecoders.fetch_add(1) >= 2) {
        --sDecoders;
        fState->snapshot.error = "Previous video is closing. Try Play again shortly.";
        fState->stop = true;
        fState->finished = true;
        return;
    }
    try { std::thread(Run, fState).detach(); }
    catch (const std::system_error&) {
        --sDecoders;
        fState->snapshot.error = "Could not start video playback.";
        fState->stop = true;
        fState->finished = true;
    }
}

VideoPlayback::~VideoPlayback() { Stop(); }
void VideoPlayback::Stop() { fState->stop = true; }
void VideoPlayback::Pause(bool paused) { fState->paused = paused; }
void VideoPlayback::Mute(bool muted) { fState->muted = muted; }
void VideoPlayback::Seek(bigtime_t position)
{
    fState->seeking = true;
    fState->seek = std::clamp(position, bigtime_t(0),
        std::max(bigtime_t(0), fState->duration.load() - 1));
}

VideoPlayback::Snapshot VideoPlayback::Read(std::shared_ptr<BBitmap>& preview, uint64& generation)
{
    auto& s = *fState;
    std::lock_guard<std::mutex> lock(s.pixelsLock);
    if (s.preview && s.generation != generation) {
        if (!preview || preview->Bounds() != s.preview->Bounds())
            preview = std::make_shared<BBitmap>(s.preview.get());
        else memcpy(preview->Bits(), s.preview->Bits(), s.preview->BitsLength());
        if (preview->InitCheck() != B_OK) preview.reset();
        generation = s.generation;
    }
    Snapshot result = s.snapshot;
    result.position = s.position;
    result.duration = s.duration;
    result.paused = s.paused;
    result.muted = s.muted;
    result.stopped = s.stop;
    result.finished = s.finished;
    result.audioFrames = s.audioFrames;
    return result;
}

void VideoPlayback::Run(std::shared_ptr<State> s)
{
    struct Lease {
        State& state;
        ~Lease() { state.fileLifetime.reset(); state.finished = true; --sDecoders; }
    } lease{*s};
    // Sound player must be destroyed before its callback state or media file.
    std::unique_ptr<BSoundPlayer> sound;
    try {
        entry_ref ref;
        Require(get_ref_for_path(s->path.c_str(), &ref) == B_OK, "Downloaded video is unavailable.");
        BMediaFile file(&ref);
        Require(file.InitCheck() == B_OK, "Haiku could not open this video. Use Save or Open in MediaPlayer.");
        BMediaTrack* video = nullptr;
        BMediaTrack* audio = nullptr;
        Require(file.CountTracks() > 0 && file.CountTracks() <= 32, "Unsupported video track count.");
        for (int32 i = 0; i < file.CountTracks(); ++i) {
            auto* track = file.TrackAt(i);
            if (!track) continue;
            media_format format = {};
            if (track->InitCheck() == B_OK && track->EncodedFormat(&format) == B_OK) {
                if (!video && (format.type == B_MEDIA_ENCODED_VIDEO || format.type == B_MEDIA_RAW_VIDEO)) video = track;
                else if (!audio && (format.type == B_MEDIA_ENCODED_AUDIO || format.type == B_MEDIA_RAW_AUDIO)) audio = track;
            }
            if (track != video && track != audio) file.ReleaseTrack(track);
        }
        Require(video, "This file has no playable video track.");
        media_format vf = {};
        vf.type = B_MEDIA_RAW_VIDEO;
        vf.u.raw_video = media_raw_video_format::wildcard;
        vf.u.raw_video.display.format = B_RGB32;
        Require(video->DecodedFormat(&vf) == B_OK && vf.type == B_MEDIA_RAW_VIDEO,
            "Haiku has no decoder for this video. Use Save or Open in MediaPlayer.");
        const auto display = vf.u.raw_video.display;
        const double frameRate = vf.u.raw_video.field_rate;
        Require(std::isfinite(frameRate) && frameRate >= 1 && frameRate <= 120,
            "Unsupported video frame rate. Use MediaPlayer.");
        Require((display.format == B_RGB32 || display.format == B_RGBA32)
            && display.line_width > 0 && display.line_width <= 4096
            && display.line_count > 0 && display.line_count <= 2160
            && display.bytes_per_row >= display.line_width * 4
            && display.bytes_per_row <= 4096 * 4
            && display.pixel_offset == 0 && display.line_offset == 0,
            "Video is too large or uses an unsupported pixel format. Use MediaPlayer.");
        s->duration = video->Duration();
        Require(s->duration > 0 && s->duration <= kMaxDuration, "Unsupported video duration.");
        std::vector<uint8> frame(size_t(display.bytes_per_row) * display.line_count);
        const double scale = std::min({1.0, 480.0 / display.line_width, 270.0 / display.line_count});
        const int width = std::max(1, int(display.line_width * scale));
        const int height = std::max(1, int(display.line_count * scale));
        {
            std::lock_guard<std::mutex> lock(s->pixelsLock);
            s->preview = std::make_unique<BBitmap>(BRect(0, 0, width - 1, height - 1), B_RGB32);
            Require(s->preview->InitCheck() == B_OK, "Could not allocate video preview.");
            memset(s->preview->Bits(), 0, s->preview->BitsLength());
        }

        media_format af = {};
        std::vector<uint8> audioBuffer;
        if (audio && !s->stop) {
            af.type = B_MEDIA_RAW_AUDIO;
            af.u.raw_audio = media_raw_audio_format::wildcard;
            af.u.raw_audio.format = media_raw_audio_format::B_AUDIO_FLOAT;
            af.u.raw_audio.byte_order = B_MEDIA_HOST_ENDIAN;
            Require(audio->DecodedFormat(&af) == B_OK && af.type == B_MEDIA_RAW_AUDIO,
                "Haiku could not decode this video's audio. Use MediaPlayer.");
            const auto& a = af.u.raw_audio;
            Require(a.format == media_raw_audio_format::B_AUDIO_FLOAT
                && a.byte_order == B_MEDIA_HOST_ENDIAN && std::isfinite(a.frame_rate)
                && a.frame_rate >= 8000 && a.frame_rate <= 192000
                && a.channel_count >= 1 && a.channel_count <= 8
                && a.buffer_size > 0 && a.buffer_size <= kMaxAudioBuffer,
                "Unsupported audio format. Use MediaPlayer.");
            s->frameBytes = a.channel_count * sizeof(float);
            s->rate = a.frame_rate;
            // One second of PCM plus a complete decoder block, capped by the
            // validated channel/rate bounds. No complete-video predecoding.
            s->pcm.resize(size_t(a.frame_rate) * s->frameBytes + a.buffer_size);
            audioBuffer.resize(a.buffer_size);
            sound = std::make_unique<BSoundPlayer>(&a, "MímisSendir video", State::Audio, nullptr, s.get());
            Require(sound->InitCheck() == B_OK, "Audio output is unavailable. Use MediaPlayer or try again.");
            const auto output = sound->Format();
            Require(output.format == a.format && output.channel_count == a.channel_count
                && output.byte_order == a.byte_order && std::abs(output.frame_rate - a.frame_rate) < 1,
                "Audio output negotiated an unsupported format. Use MediaPlayer.");
        }
        if (s->stop) return;
        bool audioEnd = !audio, videoEnd = false, pending = false, first = true;
        bool previousPaused = false, ended = false;
        bigtime_t frameTime = 0, discardBefore = 0, base = 0, epoch = system_time();
        bigtime_t audioTailTime = 0, audioTailPosition = 0;
        const bigtime_t latency = sound ? std::clamp(sound->Latency(), bigtime_t(0), bigtime_t(200000)) : 0;
        {
            std::lock_guard<std::mutex> lock(s->pixelsLock);
            s->snapshot.ready = true;
            s->snapshot.audio = bool(sound);
        }
        if (sound) {
            Require(sound->Start() == B_OK, "Could not start audio output.");
            sound->SetHasData(true);
        }
        s->seeking = false;
        while (!s->stop) {
            const bigtime_t seek = s->seek.exchange(-1);
            if (seek >= 0) {
                bigtime_t target = seek;
                Require(video->SeekToTime(&target, B_MEDIA_SEEK_CLOSEST_BACKWARD) == B_OK,
                    "This video could not be seeked. Close and replay it.");
                if (audio) {
                    target = seek;
                    Require(audio->SeekToTime(&target, B_MEDIA_SEEK_CLOSEST_BACKWARD) == B_OK,
                        "This video's audio could not be seeked. Close and replay it.");
                }
                {
                    std::lock_guard<std::mutex> lock(s->audioLock);
                    s->read = s->used = 0; s->consumed = 0; s->audioBase = seek;
                }
                base = discardBefore = seek; epoch = system_time(); s->position = seek;
                first = true; videoEnd = false; audioEnd = !audio; pending = false; ended = false;
                audioTailTime = 0;
                { std::lock_guard<std::mutex> lock(s->pixelsLock); s->snapshot.ended = false; }
                s->seeking = false;
            }
            const bool paused = s->paused;
            if (paused != previousPaused) {
                base = s->position; epoch = system_time();
                if (audioTailTime) { audioTailTime = epoch; audioTailPosition = base; }
                previousPaused = paused;
            }
            // Fill PCM before waiting for a video timestamp, so a slow video
            // frame does not starve the audio callback. Seeking drops samples
            // preceding the requested time, not just the nearest keyframe.
            if (audio && !audioEnd) {
                bool room;
                { std::lock_guard<std::mutex> lock(s->audioLock); room = s->used + audioBuffer.size() <= s->pcm.size(); }
                if (room) {
                    int64 count = audioBuffer.size() / s->frameBytes;
                    media_header header = {};
                    const auto result = audio->ReadFrames(audioBuffer.data(), &count, &header);
                    Require(result == B_OK || result == B_LAST_BUFFER_ERROR,
                        "Audio decoding failed. Close and retry, or use MediaPlayer.");
                    Require(count >= 0 && size_t(count) <= audioBuffer.size() / s->frameBytes,
                        "Invalid decoded audio size.");
                    if (result != B_OK || count == 0) audioEnd = true;
                    else {
                        // Some Haiku FFmpeg decoders return packet time-base ticks in
                        // header.start_time instead of microseconds. PCM frame indices
                        // are exact and remain valid after SeekToTime.
                        const bigtime_t sampleTime = bigtime_t((audio->CurrentFrame() - count) * 1000000.0 / s->rate);
                        const int64 skip = std::clamp<int64>(int64(std::ceil((discardBefore - sampleTime) * s->rate / 1000000.0)), 0, count);
                        const size_t bytes = (count - skip) * s->frameBytes;
                        std::lock_guard<std::mutex> lock(s->audioLock);
                        const size_t write = (s->read + s->used) % s->pcm.size();
                        const size_t firstBytes = std::min(bytes, s->pcm.size() - write);
                        memcpy(s->pcm.data() + write, audioBuffer.data() + skip * s->frameBytes, firstBytes);
                        memcpy(s->pcm.data(), audioBuffer.data() + skip * s->frameBytes + firstBytes, bytes - firstBytes);
                        s->used += bytes;
                    }
                }
            }
            if (!paused && !ended) {
                bigtime_t clock = base + system_time() - epoch;
                if (sound) {
                    std::lock_guard<std::mutex> lock(s->audioLock);
                    clock = s->audioBase + bigtime_t(s->consumed * 1000000.0 / s->rate);
                    clock = std::max(s->audioBase, clock - latency);
                    if (audioEnd && !s->used) {
                        if (!audioTailTime) { audioTailTime = system_time(); audioTailPosition = clock; }
                        clock = audioTailPosition + system_time() - audioTailTime;
                    }
                }
                s->position = std::min(clock, s->duration.load());
            }
            if (!videoEnd && (!paused || first)) {
                if (!pending) {
                    int64 count = 1;
                    media_header header = {};
                    const auto result = video->ReadFrames(frame.data(), &count, &header);
                    Require(result == B_OK || result == B_LAST_BUFFER_ERROR,
                        "Video decoding failed. Close and retry, or use MediaPlayer.");
                    if (result != B_OK || count != 1) videoEnd = true;
                    else {
                        // Use the negotiated frame clock, not decoder packet ticks
                        // (e.g. 1024 ticks for a 66,667 us MP4 video frame).
                        frameTime = bigtime_t((video->CurrentFrame() - 1) * 1000000.0 / frameRate);
                        pending = true;
                    }
                }
                if (pending && (frameTime < discardBefore || frameTime <= s->position || first)) {
                    if (frameTime + 1000000.0 / frameRate > discardBefore) {
                        std::lock_guard<std::mutex> lock(s->pixelsLock);
                        auto* bits = static_cast<uint8*>(s->preview->Bits());
                        for (int y = 0; y < height; ++y) {
                            const auto* src = frame.data() + (uint64(y) * display.line_count / height) * display.bytes_per_row;
                            auto* dst = bits + y * s->preview->BytesPerRow();
                            for (int x = 0; x < width; ++x) {
                                memcpy(dst + x * 4, src + (uint64(x) * display.line_width / width) * 4, 4);
                                dst[x * 4 + 3] = 255;
                            }
                        }
                        ++s->generation; ++s->snapshot.frames; first = false;
                    }
                    pending = false;
                }
            }
            if (videoEnd && s->position >= s->duration) {
                ended = true; s->paused = true;
                std::lock_guard<std::mutex> lock(s->pixelsLock); s->snapshot.ended = true;
            }
            // Keyframe seeking may need to discard several seconds of frames.
            // Yield briefly while catching up, not one playback interval/frame.
            snooze(first && !videoEnd ? 1000 : 5000);
        }
        // Stop callbacks before the BMediaFile (and its tracks) leave scope.
        if (sound) { sound->Stop(); sound.reset(); }
    } catch (const std::exception& error) {
        s->seeking = true;
        if (sound) { sound->Stop(); sound.reset(); }
        std::lock_guard<std::mutex> lock(s->pixelsLock);
        s->snapshot.error = error.what();
    }
    s->stop = true;
    s->fileLifetime.reset();
}
