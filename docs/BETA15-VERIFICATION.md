# Beta 15: atomic conversation painting

Arne clarified that the remaining flash occurs when a message is sent, not only
when a Delivered receipt arrives. Beta 14's receipt-only fix was insufficient.
The native send path invalidates the conversation while adding the new message.
With the original opaque view, app_server can erase that region before the
window looper reaches Draw. Direct background and text painting can also expose
an unfinished frame.

The timeline now suppresses automatic background erasure and paints each damaged
region into a reusable viewport-sized bitmap before one completed blit. The
surface is independent of total history height, recreated on resize, capped at
16 million pixels, and released with the view. Allocation failure falls back to
direct drawing. Selection/caret use the same target; receipt damage stays local.

Native Haiku R1 beta6 x86_64, standalone test disk, synthetic accounts only:
- A beta 14 source build with timestamp-only Draw/send/status instrumentation
  reproduced two consecutive frames with 90.04% bare document background after
  Enter. No artificial delay was injected. No FrameResized event accompanied it.
- A separate uninstrumented installed-beta-14 sample did not capture the flash.
  The defect is timing-sensitive; empty samples are not treated as proof of a fix.
- 368 native attachment/timeline checks pass, including full-versus-partial pixel
  equality at three scroll offsets in both palettes, background preservation,
  resizing, selection, caret and existing localized receipt regressions.
- 72 history checks and delivery correlation/reordered-receipt checks pass.
- Installed beta 15 matches the native build, executable SHA-256
  `ccffa178cf546f879fba20c4e6b1cdd2247c29c7fb4175d8f2e1be5946548c66`.
- Upgrade from beta 14 preserves all 90 profile files byte for byte.
- Five UI-sent messages reach the synthetic peer. Across 450 frames during
  sending, no frame exceeds the blank-field threshold; the previous empty
  state is absent. A further 100 compact light-palette send frames pass too.
- Ten reordered Delivered/Read receipts across 240 frames change at most 435
  of 203116 conversation pixels, confined to footer marks.
- Dark/light selection and compact resize are visually inspected.
- Actual beta 15 to beta 14 to beta 15 package roundtrip preserves all 96
  profile files, including the new sent messages and read receipts. Both installed
  executable hashes match their retained package/build identities.

Final HPKG SHA-256:
`008ff3de353f25b1e39e39717408516e6963244252da2d98452759c04c7bdc01`.

Physical hardware confirmation is still outstanding. No production bridge or
profile schema changes. Recovery: quit the app and reinstall retained beta 14,
keeping current history/settings; never restore stale messages.
