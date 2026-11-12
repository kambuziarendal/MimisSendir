Pinned Signal bridge source patches

Slidgnal 0.5.1 / Slidge 0.5.2; base image:
codeberg.org/slidge/slidgnal:v0.5.1-amd64@sha256:ea4aaf58ad5eab65214ebfb43bfe4178b30baa90251c78f54b57e1cd79756982

Includes ordered startup events, isolated unavailable groups, bookmark names,
bounded correlation tracking, separate metadata/action lanes, bounded action
admission, and group avatar persistence after bookmark creation. The separate
native upload patch supplies the encrypted attachment length.

prepare.py validates exact upstream source hashes. Tests use synthetic fixtures;
no production credentials or state are included. Sending is never automatically
replayed after an unknown outcome. Action admission is bounded and deadline-limited.
A stuck native call retains its slot until it actually completes.

These are source patches, not a published bridge image or turnkey installer.
Upstream Slidge/Slidgnal and derived patches are AGPL-3.0-or-later. See LICENSE
and ../docs/public/BRIDGE.md for architecture, setup limitations and source links.
