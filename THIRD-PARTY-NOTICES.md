# Third-party notices

- **Renga / Jabber for BeOS**: original authors and MIT terms in [LICENSE](LICENSE). This fork changes UI, history, media, protocol handling, configuration and packaging.
- **gloox 1.0.28**: Jakob Schröter, GPL-3.0-or-later with OpenSSL linking exception. Exact [license](third_party/gloox/LICENSE), upstream source archive and HaikuPorts recipe/patchset are included in `third_party/gloox/`. The client binary uses Haiku's shared gloox 1.0.28-4 package. Source archive SHA-256: `591bd12c249ede0b50a1ef6b99ac0de8ef9c1ba4fd2e186f97a740215cc5966c`. Haiku package metadata says GPL v2; the actual upstream source contains GPL v3. The combined executable is distributed under GPL-3.0-or-later, retaining original MIT notices and the upstream OpenSSL exception.
- **Noto Emoji / Noto Color Emoji**: Google and contributors, SIL OFL-1.1. Original font and rendered emoji attribution/licenses in `resources/fonts/` and `resources/emoji/`.
- **Unicode data**: Unicode, Inc., license in `resources/emoji/LICENSE.txt`.
- **Liberation fonts used in local artwork**: license in `resources/fonts/Liberation-LICENSE.txt`.
- **GIPHY attribution mark**: provider artwork, see `resources/giphy/README.txt`. No API credential is included; provider terms apply if enabled.
- **Haiku system libraries and separately installed shared dependencies**: curl (curl license), OpenSSL (Apache-2.0 for version 3), libcss and libwapcaplet (MIT), libiconv (LGPL), giflib (MIT), libwebp (BSD-3-Clause), Haiku libraries and runtime. These are not redistributed inside our HPKG; retain their original package notices. Exact package versions are recorded with the release.
- **Slidge/Slidgnal bridge patches**: AGPL-3.0-or-later, see `bridge/LICENSE` and upstream projects linked in the operator guide. The client installer contains no bridge binary. Native bridge images have separate source/compliance requirements and are not distributed here.

The MIT license at the repository root describes original client source, not every file or combined executable. No third-party sticker collections or real account images are included.
