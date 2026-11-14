# Beta 12: idle conversation redraw

Native Haiku R1 beta6 x86_64, our isolated QEMU/KVM VM and synthetic XMPP peers.

Root cause: selecting/clicking read-only conversation text started TextDocumentView's
caret timer. The editor was disabled, so no caret bounds were drawn. The timer
therefore invalidated the entire conversation every 500 ms, indefinitely, even
when no messages arrived. Select All could start the same timer. The minute date
poll also invalidated the timeline without checking whether the date changed.

The caret timer now runs only for editable, focused, active text with an empty
selection. Disabling editing/selection, losing focus or deactivating the window
cancels it and invalidates only a previously visible caret. Read-only selection
still updates once. The date poll redraws only on a local-calendar-day change;
archive synchronization is unchanged.

Verification:
- New native regression fails on beta11: readonly timeline stays idle after
  clicking text. Four new checks pass after the fix: idle click, Select All,
  editable caret still blinks, disabling editing cancels the timer.
- 336 native attachment/text checks, 10 date checks, 72 history checks and delivery
  correlation/reordered receipt/retained text checks pass. Native app builds and
  links with -Werror.
- Installed beta11 to beta12 upgrade preserves all 78 synthetic profile files
  byte-for-byte. Installed/build executable SHA256:
  `8d69e7917bf2f139b52ea02ce649d19ba722181fc8ff97b0c36cb0ec7bf38a2c`.
- Installed beta12: click in a 76-message conversation, then settle. 100 consecutive
  captured frames have identical conversation/header/list pixels. The preceding
  180-frame capture includes initial conversation layout, then 160 identical
  frames. Native UI loop: 30 samples, zero failures, maximum response 951 us.
- Real native UI sends a synthetic message to the intended peer, receives its
  receipt and a new incoming message. Minute history poll remains usable.
- Dark-theme installed view inspected. No private account, Signal traffic or
  bridge deployment was involved.

Additional media check: the first run timed out because the recreated VM had no
virtual sound card. After a clean shutdown, AC97 was added and
`/dev/audio/hmulti/auich` verified. The same unchanged candidate then passed all
30 inline-video checks, including silent and audio-bearing clips, preview,
play/pause and cleanup. The failed environment run is retained in the local
report rather than counted as a pass. No media/codec/playback code changed.

Limitations: physical hardware/GPU-specific flashing and physical audio output
are not directly tested. Beta11's separate media acceptance remains documented
in BETA11-VERIFICATION.md.

Recovery: reinstall retained beta11 HPKG; keep current settings/history. No data
migration, database restoration or bridge change is needed.
