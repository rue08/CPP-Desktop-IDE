# Bundling MinGW-w64 for Windows

**Already have a C++ compiler on `PATH`** (an existing MinGW/g++ install)?
You don't need anything on this page — just build from source per the main
[README](../README.md#option-2-build-from-source) and it'll be picked up
automatically. This doc is only relevant if you want your build to be a
self-contained package that needs no compiler installed at all, whether
that's a one-off release you're packaging for others or just wanting your
own local build to work the same way.

The Windows build of VaultWright looks for a compiler at `mingw64/bin/g++.exe`
next to `VaultWright.exe` before falling back to whatever `g++` is on `PATH`
(see `Terminal::runFile()` in `terminal.cpp`). Bundling one here means end
users can hit Run with no separate compiler install.

## One-time setup (per machine building a release)

1. Download a portable MinGW-w64 build — [WinLibs](https://winlibs.com/) is
   the standard choice (no installer, just a folder of binaries). Grab a
   release build, x86_64.
2. Unzip it, and place its contents so you end up with:
   ```
   windows/mingw64/bin/g++.exe
   windows/mingw64/bin/*.dll   (its runtime dependencies)
   ...
   ```
3. That's it. `windows/mingw64/` is gitignored — it's never committed, just
   placed locally before building.

## What happens at build time

- `CMakeLists.txt` checks whether `windows/mingw64/` exists. If it does, a
  post-build step copies it next to the built `VaultWright.exe`, and it's
  included in `cmake --install` output too.
- If it doesn't exist, the build still works — `Terminal::runFile()` falls
  back to searching `PATH` for `g++`, which is enough for local development
  on a machine that already has MinGW installed some other way. Only a
  *packaged release* meant for end users needs this folder actually
  populated, so Run works for someone with nothing installed.

## Size note

A full MinGW-w64 toolchain is roughly 150–400MB depending on the build.
That's added to the Windows download once this is in place — expected, and
the whole point of bundling it.
