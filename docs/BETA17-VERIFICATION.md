# Beta 17: typing during conversation updates

The regression was reproduced in a Haiku R1/beta6 x86_64 VM using synthetic XMPP accounts. With 400 messages already in a direct chat, 48 incoming messages arrived over 3.76 seconds while a separate input producer entered 160 characters at 25 ms intervals. The probe independently read the draft without sending it.

In two beta 16 runs, the composer timed out on 14 one-second reads per run. The full draft appeared later: this was a responsiveness issue, not demonstrated loss of keystrokes. With the beta 17 fix, all 160 characters appeared in order with zero read timeouts. In the final build-tree run, 95% were observed within 27 ms, all within 33 ms. These are upper bounds on time to observation, not physical keyboard or display latency.

Separate runs covered four interleaved delivery receipts and the initial retrieval of 240 archive records plus a correction. The oldest message was corrected and an overlapping live message remained visible after the archive. Native history (92), conversation (171), timeline/attachment (380) and delivery regression checks passed. Physical hardware and a live Signal account were not used.

The beta 16 to beta 17 package upgrade preserved all 450 files in the synthetic profile byte for byte. The installed client and sticker-link helper matched the native build. The release contains no bridge change. To revert the client, quit it and reinstall the previous beta 16 HPKG; keep the current profile.

- Author HPKG SHA-256: `e203c2bac997429ed78cab06ca8ffffcc9ab0ac36a04db06a1b7f00f60a134be`
- Installed client SHA-256: `f1024268551f3205a1a0bab2a7a70859d989feca9fa24ba58c9a6a8eb99b5815`

## Published packages

The [GitHub prerelease](https://github.com/kambuziarendal/MimisSendir/releases/tag/v0.1.0-beta.17) and [Spritz catalog](https://spritzdepot.desktoponfire.com/app/no.kambuzi.mimissendir) both identify beta 17. The GitHub assets were downloaded anonymously and checked against the local files. The Spritz direct download matches the author HPKG. The installed author package repeated the typing test with all 160 characters intact, zero timeouts, p95 25.6 ms and maximum 32.9 ms.

The public HaikuDepot index lists the repackaged HPKG with SHA-256 `a2e4e128c6a608b3fbcceecf6ad86fa5abef8236757b796af362e73619fa339d`. Spritz changes the vendor field; all 50 other payload files match the author package. The repository copy was installed over beta 16 and launched in Haiku. Its native executable and sticker-link helper match the tested build. No production bridge was changed.
