# Public beta verification, 12 November 2026

All three release versions were built from separate source snapshots with neutral service defaults, installed on Haiku R1/beta6 hrev59866+79 x86_64 in a VM, and identified by their installed executable hashes. Shared dependencies are listed in BUILD-ENVIRONMENT.txt. No provider key is included.

| Public package | History checks | Protocol checks | Native text/layout checks |
| --- | ---: | ---: | ---: |
| beta3-6 | 26 | 41 | Not repeated for this packaging pass |
| beta4-5 | 52 | 47 | Not repeated for this packaging pass |
| beta5-3 | 55 | 63 | 149 |

The latest exported source matched 418 native build inputs. Public beta 5 was exercised with synthetic direct and group conversations, group artwork, a sent message and receipt, light/dark palettes, and a narrow-window jump-to-latest action preserving the unsent draft. Existing history survived the public beta 3 → 4 → 5 installation sequence. The two README screenshots are active-window captures of installed beta 5 with fictional data.

The bridge utilities passed 19 offline unit tests. The release-manifest helper passed four tests; all three manifests passed the current Spritz schema. These are separate from a live Signal transport test or a Spritz catalog installation, neither of which was performed for public packaging.

During preparation, a stale-timestamp incremental beta 4 build was detected by its unchanged executable hash and withheld. The changed source inputs were explicitly invalidated and rebuilt before acceptance. Beta 5 temporary build files were moved to the scratch volume after a low-space compilation failure. The failed outputs were not published.

Each release carries HPKG, installer ZIP, matching source archive and SHA256SUMS. Anonymous GitHub downloads are checked against the local hashes. Security reports can be submitted through the repository's private vulnerability reporting. Beta 5 is now published in Spritz Depot; its public page and resolver are verified. Native Spritz installation is not yet tested. No hosted bridge service is included.
