# Build on Haiku x86_64

Use Haiku R1/beta6 hrev59866+79 or later, CMake >=3.20 and a C++20 compiler. Install development packages providing:

`devel:libgloox`, `devel:libcrypto`, `devel:libcss`, `devel:libwapcaplet`, `devel:libiconv`, `devel:libcurl` (curl >=7.85), `devel:libgif`, `devel:libwebp` (including demux), plus the Haiku development libraries.

```sh
cmake -S . -B build-haiku -DCMAKE_BUILD_TYPE=Debug -DRENGA_BUILD_NATIVE_TESTS=ON
cmake --build build-haiku --target Renga RengaStickerLink -j2
```

The `Renga` target produces `build-haiku/mimissendir`; the internal name preserves upstream compatibility. Debug is the validated build mode. The executable uses shared system libraries, not bundled copies.

Run relevant native test targets listed in CMakeLists.txt. They require Haiku; Linux parser tests do not establish UI correctness.

```sh
cmake --build build-haiku --target RengaHistoryTests RengaAttachmentTests -j2
./build-haiku/RengaHistoryTests
./build-haiku/RengaAttachmentTests
sh packaging/package-native-binary.sh "$PWD/build-haiku/mimissendir" "$PWD/package-output"
```

The packaging helper rejects existing outputs and never embeds provider keys. Install and test the resulting HPKG, not merely the build-tree executable. Dependencies are resolved by Haiku's package manager.

Each public release tag includes its exact source and package version. Historical beta tags intentionally retain their corresponding feature set. See [source and dependency details](docs/public/BUILD.md).
