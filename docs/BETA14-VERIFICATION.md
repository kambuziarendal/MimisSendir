# Beta 14: delivery/read receipt redraw

The reported trigger is a message changing to Delivered. RefreshMessageFooter
previously invalidated the entire conversation for every matching update.
It now changes only the footer row, ignores unchanged values, and expands the
invalidated region only when a group read label changes the paragraph height.
Offscreen receipts do not repaint visible messages. Bottom anchoring is retained.

A separate native pixel regression reproduced selection drawing-state leakage:
subtract/invert mode survived into the next draw. Draw now isolates that state
and begins background/content painting in copy mode.

Native Haiku R1 beta6 x86_64, isolated QEMU overlay and synthetic XMPP peers:
- 353 attachment/timeline checks pass, including localized receipt damage,
  duplicate and offscreen receipts, wrapped reader labels/scroll range, identical
  selected-text pixels, and the existing idle/caret and floating-button tests.
- 72 history checks and delivery correlation/reordered-receipt checks pass.
- The new selected-text pixel test failed against beta 13 before the fix.
- Final installed HPKG matches the clean native build, executable SHA-256
  `98a104816fe98b3796d1304dff45e9d5692cd642c5ae650fb39a9bb82d9b385a`.
- Five UI-sent messages receive delivery and read markers in a different order.
  Across 240 captured frames, at most 435 of 203116 conversation pixels change,
  confined to the marks. Text, bubbles and viewport remain stable.
- Beta 13 to beta 14 upgrade preserves all 80 synthetic profile files.
- Installed dark/compact light selection inspected; 100 settled selected-text
  frames are identical. Actual beta 14 to beta 13 to beta 14 package roundtrip
  preserves all 86 profile files after the new messages and read receipts.
- Final HPKG SHA-256:
  `7a5a75855b00c9bb2035fa5bd63a9c775d28467601062dd5db6ac65a015a1e93`.

The full black frame reported on physical hardware was not captured in the VM's
baseline receipt recording. This change removes the confirmed full-conversation
receipt repaint and the separately reproduced drawing-state defect; physical
hardware confirmation remains outstanding.

Recovery: quit the app and reinstall retained beta 13. No profile migration or
bridge change; keep current settings/history, never restore stale messages.

The initial inherited overlay had an invalid package activation index and was
rejected. The final build, all native checks and package installation were
repeated on a standalone image copied from a cleanly stopped, working guest.
No real account or production bridge was involved.
