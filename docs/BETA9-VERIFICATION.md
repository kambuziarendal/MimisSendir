# Beta 9 acceptance

Native Haiku R1 beta6 hrev59866+79 x86_64, isolated overlay and synthetic TLS/XMPP/media fixtures. No production bridge or real conversation media was used.

- Reproduced beta 8 showing standalone image and MP4 URLs when upload settings were absent.
- The candidate discovers the account server's advertised same-domain XEP-0363 service and caches it against the bare account JID, including sign-ins with a resource. Explicit upload settings remain authoritative.
- Old URL-only local history renders an image preview and video card without rewriting the history store. New OOB image captions are retained.
- 52 native metadata/discovery, 329 native text/layout, 26 native image-quality and 72 native history checks pass.
- MP4 H.264/AAC test clip downloads only on Play, matches the source SHA-256, and opens MediaPlayer. Two observed frames advance from 1.267 to 3.933 seconds after the client exits. Physical audio output was not listened to.
- Test VM initially lacked an audio device. Its AudioMixer crashed; the report was retained. Rebooting that isolated VM with an AC97 device resolved it, and actual playback was repeated. This was not a production change.
- Installed upgrade from beta8-2 to beta9-1 retains the profile. Installed executable SHA-256 matches the native build: `34f24f1e006c90dbc35eed50b4bb492a5b91f1ed14331181a0f47bf9ceca018b`.
- Package installation uses offline `pkgman install -R -y` in the restricted fixture. The initial refreshing command was stopped before activation; beta 8's installed hash was verified unchanged before retry.
- Installed candidate: missing video gives a visible HTTP 404 error without launching a player. Retry starts a new download; Cancel stops it. Replacing the synthetic fixture and retrying successfully opens the exact MP4 in MediaPlayer.
- Public neutral defaults, separate Renga identity and KeyStore settings are retained. Video codec support depends on Haiku media add-ons. Inline video, clips above 50 MiB, physical-device/live Signal interoperability and native HaikuDepot delivery are not claimed tested.

Recovery: reinstall the retained beta 8 package without restoring stale conversation data. Discovery adds only account-scoped settings; no destructive history/schema migration is performed.
