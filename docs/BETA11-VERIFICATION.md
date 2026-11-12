# Beta 11 acceptance

Native Haiku R1 beta6 hrev59866+79 x86_64, isolated QEMU guest and synthetic TLS/XMPP/media only. No production bridge changes or real messages.

- Visible videos prepare a static preview without playback. A centered play button starts the clip; clicking the image pauses it and restores the button. No control strip below the image. Right-click retains playback, MediaPlayer and save actions.
- 30 native video checks, 332 native attachment/layout checks, 52 metadata checks, 26 image-quality checks, 72 history checks and delivery-correlation checks passed.
- Installed beta10-2 to beta11-1 upgrade preserved all 344 profile files byte-for-byte. Installed/build executable SHA256: `80be556ae86775b01d3567b38226c3b49eba99a808380e92df740a5b7a53c716`.
- Installed UI: both previews appeared automatically without clicks. Switching playback retained the other preview in both directions. Paused image regions remained pixel-identical; play overlays returned. Right-click and save dialog, conversation-switch pause, restart without autoplay, 680x620 and 1000x760 light/dark windows checked.
- QEMU AC97 output capture: peak 2211 during each clip's playback, zero samples above zero during settled preview/pause intervals. This verifies virtual audio, not physical-device listening. Two 60-sample native UI probes had zero failures; maximum reply time 1.319 ms.
- In this layered test VM, two small previews were still loading at 8 seconds and ready by 15 seconds without input. No frozen UI was found. Physical hardware, every codec, live Signal and native HaikuDepot installation are not claimed tested.
- Initial app build failed because guest `/boot` had only 4.2 MiB free. Preserved and SHA256-verified 31 bundled Haiku source archives on the larger guest build volume; original source path retained by symlink. `/boot` then had 331.8 MiB free and the build succeeded. App source, profiles and installed dependencies were not removed.

HPKG `0.1.0~beta11-1` SHA256: `7493f326511beab930679f9c5b7f388f2b2b7f45861fc26972ef641e59388838`.

Recovery: reinstall retained beta10-2, without restoring old history. No data/schema migration. Test evidence retained locally under `reports/mimissendir-video-posters-20261112`.
