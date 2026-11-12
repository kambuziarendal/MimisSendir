# Source and build provenance

Follow [COMPILING.md](../../COMPILING.md) on native Haiku x86_64. Public client tags preserve the corresponding beta source. They contain neutral service defaults, no account configuration, no private history and no provider API key. The original source licenses and contributor notices remain intact.

The release assets include the exact source archive, HPKG and SHA-256 checksums (older releases also include installer ZIPs). The source archive contains build scripts and resources, including generated icon/emoji inputs. The separately installed Haiku libraries are dependencies, not copies embedded in the HPKG.

The gloox 1.0.28 source archive and HaikuPorts revision-4 recipe/patchset are retained under `third_party/gloox/` for corresponding-source access. Its upstream archive SHA-256 is `591bd12c249ede0b50a1ef6b99ac0de8ef9c1ba4fd2e186f97a740215cc5966c`. Its source license is GPL-3.0-or-later with an OpenSSL exception despite older Haiku package metadata saying GPL v2. See LICENSES.md.

## Bridge patches

`bridge/` contains the guided installer, source preparation scripts and regression tests for pinned Slidgnal/Slidge versions. `Dockerfile.combined` builds Python and native fixes together from a public digest-pinned base and checksum-verified Go toolchain. No prebuilt bridge image is published with client releases; the installer builds and records a local immutable image ID. See [guided setup](BRIDGE.md). The Linux bridge is independently installed and operated, and still requires a prepared local Prosody server.

```sh
python3 -m unittest discover -s bridge/tests -p 'test_*.py'
python3 bridge/prepare.py /absolute/pinned-upstream /absolute/new-stage
```

Source preparation validates exact upstream hashes and refuses an existing output. Use disposable isolated fixtures, never production volumes. Integration tests require the matching pinned modules. The native upload recipe also requires the matching Go toolchain and libsignal inputs. Do not present these patch files as the complete corresponding source for a distributed bridge image; such a release needs its own complete source and dependency manifest.
