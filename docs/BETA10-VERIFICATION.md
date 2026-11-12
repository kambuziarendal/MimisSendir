# Beta 10 acceptance

Native Haiku R1 beta6 hrev59866+79 x86_64, isolated overlay backed by the stopped beta 9 fixture. Synthetic local TLS/XMPP and H.264/AAC MP4 only. No production bridge or private media was used.

- Video now plays in the message bubble. Play/pause, replay, backward/forward seeking, mute and close use a bounded local decoder and PCM ring; one active player across conversations. Download remains opt-in, maximum 50 MiB. MediaPlayer remains an explicit attachment-menu fallback.
- 23 native player checks pass: frames advance at the negotiated rate, audio PCM reaches the sound callback, pause freezes both, seek while paused, seek past the end displays the last frame, backward seek, replay, mute, silent video, invalid path, superseding a player, asynchronous close and download-file lifetime.
- 52 metadata/discovery, 329 text/layout, 72 history and 26 image-quality checks pass. The image-quality check was rerun from the required source working directory after an initial missing-fixture error.
- The first player test exposed MP4 decoder packet ticks in `media_header.start_time`, not microseconds. Video frame indices/negotiated frame rate and exact audio sample indices now drive timing and seeking. The failing log was retained.
- Installed UI: old image previews and outgoing delivery checks remain; no video GET before Play. Timed video advances in the bubble, pauses on a stable frame and resumes. Seeking while paused shows the target/last frame. Leaving and returning to a conversation leaves playback paused. Controls remain accessible at 680×620 and 1000×720 after scrolling to the message.
- QEMU AC97 output was captured with `wavcapture`: nonzero PCM during playback (peak 2211), zero settled samples while paused. The final installed app was also closed during playback: its process exited and the final half-second of captured audio was silent. This proves virtual audio output, not listening on a physical device. Native UI-loop probe: 60 samples, zero failures, p95 31.7 ms, maximum 100.8 ms.
- Upgrade beta9-1 to the first beta10 candidate, then beta10-2 after a guest reboot. Existing profile/history retained, no autoplay on restart. Final installed/build executable SHA-256: `424a39d712d503b600399ee476b115d61beab984fd0fa025d12848313fdd483f`.
- Final HPKG SHA-256: `f1017d77f49e31e940962f81510c26b4381a99462e2aac98a1f37a78859bfc02`.
- Limits: installed Haiku codecs; inline frames up to 4096×2160 and 1 to 120 fps, duration up to 24 hours. Preview is at most 480×270 and the UI refreshes at 20 Hz. Physical-device playback, every codec/variable-rate file and native HaikuDepot delivery are not claimed tested.

Recovery: reinstall retained beta 9 without restoring stale history. No history/schema migration. Test-only inactive beta 9 package state was retained on the guest work disk; production files and services were unchanged.
