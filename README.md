# C++ Desktop IDE

## Overview
A lightweight, self-contained **C++ IDE** built to solve a simple problem: your code should be as easy to pick up from anywhere as your photos or documents already are.

Cloud backup has made personal files effortless to access and sync across machines. Raw source code, in a real development environment, has never had that same convenience. This project closes that gap — sign up, and your files are available from any machine you install the IDE on.

## Features
- An embedded [Monaco Editor](https://microsoft.github.io/monaco-editor/) (VS Code's editor core) for every workspace tab — real find/replace, multi-cursor editing, code folding, a minimap, and language-aware syntax highlighting.
- One-click compile & run, driving the `g++` toolchain directly from the editor.
- A cloud-backed file system — save a file locally, upload it, and pull it down again from any other machine you're signed into.
- Stay signed in across restarts — logging in once is enough; the app silently restores your session on the next launch instead of asking again.
- Asynchronous upload/download with real-time status feedback, including batch upload and batch delete for multiple files at once (multi-select in either file tree, or use the File menu's Upload/Delete File to/from Cloud actions).
- Local file and folder management alongside your cloud files, side by side.
- Light/dark theme, either following the OS or set explicitly via **View > Theme**.

## Tech Stack
- **Language**: C++
- **Framework**: Qt (Widgets + Network + WebEngine + WebChannel)
- **Editor**: [Monaco Editor](https://microsoft.github.io/monaco-editor/), embedded via `QWebEngineView`
- **Auth**: Firebase Authentication
- **File storage**: [Node.js + PostgreSQL backend](server/) (`server/`) — Firebase ID tokens in,
  files out; no Firestore/Cloud Storage involved. One instance is hosted centrally for you, see
  **Cloud Backend** below — you never need to run this yourself.
- **Compiler**: g++
- **Networking**: QtNetwork (REST APIs)

## Getting Started

There's no packaged release yet — the project is still being polished before a first public
release, so building from source is currently the only way to run it.

Sign-up/login works out of the box once built. Cloud file storage talks to a central backend
that's already running for you — nothing to set up on your end, just point the app at it (see
**Cloud Backend** below).

### Build from source

The build uses whatever C++ compiler is already on your machine (e.g. an existing MinGW/g++ on
Windows) — see [`windows/README.md`](windows/README.md) if you'd rather bundle a compiler in
alongside the build instead of relying on `PATH`.

**Prerequisites**
- [Qt](https://www.qt.io/download-qt-installer) (Qt 6 recommended, Qt 5 also supported) with the
  **Widgets**, **Network**, **WebEngine**, **WebChannel**, and **Positioning** modules — none of
  the last three are installed by default, so add them explicitly in the Qt Maintenance Tool /
  online installer's component list. Two gotchas that aren't obvious from the installer UI:
  - **WebEngine isn't built for every Qt patch release** — under Extensions → Qt WebEngine, only
    specific patch versions are offered (e.g. 6.10.3, not 6.10.1). If your installed Qt patch
    version isn't one of them, install one that is (same minor version, e.g. 6.10.x) rather than
    trying to force WebEngine onto a version it wasn't built for.
  - **Qt Positioning is a WebEngine dependency**, not an optional extra — it's under the base Qt
    version's own "Additional Libraries" list (not under Extensions), and `Qt6WebEngineCore`
    won't configure without it.
- CMake 3.16+
- A C++17 compiler (`g++` or Clang) on `PATH`
- Node.js + `npm` on `PATH` — only needed once, to vendor the editor (see step 2 below)
- [Qt Creator](https://www.qt.io/product/development-tools) — optional, but the simplest way to get Qt and build in one step

**Build**

1. Clone the repo:
   ```bash
   git clone https://github.com/rue08/CPP-Desktop-IDE.git
   cd CPP-Desktop-IDE
   ```

2. Vendor the embedded editor (one-time; see [`third_party/README.md`](third_party/README.md)):
   ```bash
   third_party/fetch-monaco.sh
   ```

3. Build it, either way:

   **Using Qt Creator** (easiest)
   - File → Open Project → select `CMakeLists.txt`
   - Let it configure the kit, then hit **Run** ▶️ in the bottom-left corner.

   **From the command line**
   ```bash
   cmake -B build -DCMAKE_PREFIX_PATH=<path-to-your-Qt-install>   # e.g. ~/Qt/6.10.3/macos
   cmake --build build
   ```
   On Windows, pass the generator explicitly and run this from the Qt-provided MinGW command
   prompt (Start Menu → "Qt \<version\> (MinGW 64-bit)"), not a plain `cmd.exe` — otherwise
   CMake defaults to looking for MSVC's `nmake`, which won't be on `PATH`, and configuration
   fails before it ever reaches your compiler:
   ```bat
   cmake -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\6.11.1\mingw_64"
   cmake --build build
   ```

4. Launch the built `VaultWright` binary.

## Cloud Backend

File storage (upload/list/download) talks to a [`server/`](server/) Node.js + PostgreSQL
backend, not Firebase — but as a user, **you don't need to run this yourself.** One instance is
hosted centrally; you just need its current URL, entered via the ⚙️ **Settings** button in the
toolbar. Firebase Authentication is unaffected either way; sign-up/login/refresh works
independently of this.

The backend's URL isn't baked into the app because it can change (currently tunneled via ngrok's
free tier, which assigns a new URL on every restart) — Settings has a **Fetch Latest from
GitHub** button that pulls the current one automatically; it's saved and reused on future
launches.

**Availability:** The shared backend isn't running around the clock. It's hosted on a personal
laptop rather than dedicated server infrastructure, reached through a free-tier ngrok tunnel —
and free ngrok tunnels are a temporary, single-session URL, not a permanent
one, so this setup isn't meant to run unattended 24/7. The project's also still in a pre-release
polish phase, not yet a guaranteed-uptime service. Cloud sync (Upload/Vault) is generally
available **3pm–12am IST**; outside that window, everything else — local editing, saving,
compiling, running — works exactly the same, only cloud features will be unreachable until the
backend's back up.

[`server/README.md`](server/README.md) documents how the shared backend itself is built and run
— implementation detail for the one instance VaultWright is built around, not a self-hosting
guide. The project isn't set up to support parallel, community-run instances, and isn't taking
outside contributions at this stage.

## Using the IDE

1. **Point the app at the backend** — click ⚙️ **Settings** in the toolbar and enter the
   current backend URL (see **Cloud Backend** above). Only needed once, or whenever the URL changes.
2. **Sign up / log in** — click the login icon in the toolbar and create an account with an email and password. This is what ties your files to you across machines; once you're signed in, the app keeps you signed in on future launches too, so this is normally a one-time step per machine.
3. **Create or open a file** — `Ctrl+N` for a new file, `Ctrl+O` to open an existing one, or "Open Folder..." to work across a whole project.
4. **Write and save** — `Ctrl+S` saves locally, same as any editor.
5. **Run it** — hit the ▶️ **Run** button to compile and execute the current file with `g++`; output opens in a terminal window.
6. **Push to the cloud** — with the file open, click the ☁️ **Upload** button (or **File > Upload File to Cloud**) to save it to your account. Select multiple files in the local files tree first to upload them all in one batch.
7. **Pull it down elsewhere** — open **The Vault** from the toolbar to browse the files you've uploaded, and click one to bring it down to whatever machine you're on.
8. **Delete a cloud file** — select it (or several) in The Vault and use **File > Delete File from Cloud**; you'll be asked to confirm first.

## Project Status
The main aim is to build a full-featured project, and it's under active development.

### Coming next
1. More robust file handling, including uploading whole folders (currently limited to individual code files).
2. Continued UI/UX improvements.
3. An integrated terminal.
4. Booklet of Profile options.

## Author
Mehul Sharma\
Gmail: mehssi2004@gmail.com

If you have any suggestions feel free to reach out to me via mail, and if you liked the project, make sure to give a star ⭐️.
