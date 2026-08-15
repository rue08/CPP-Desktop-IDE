# C++ Desktop IDE

## Overview
A lightweight, self-contained **C++ IDE** built to solve a simple problem: your code should be as easy to pick up from anywhere as your photos or documents already are.

Cloud backup has made personal files effortless to access and sync across machines. Raw source code, in a real development environment, has never had that same convenience. This project closes that gap — sign up, and your files are available from any machine you install the IDE on.

## Features
- A custom C++ editor built on Qt, with syntax highlighting.
- One-click compile & run, driving the `g++` toolchain directly from the editor.
- A cloud-backed file system — save a file locally, upload it, and pull it down again from any other machine you're signed into.
- Asynchronous upload/download with real-time status feedback.
- Local file and folder management alongside your cloud files, side by side.

## Tech Stack
- **Language**: C++
- **Framework**: Qt (Widgets + Network)
- **Auth**: Firebase Authentication
- **File storage**: self-hosted [Node.js + PostgreSQL backend](server/) (`server/`) — Firebase
  ID tokens in, files out; no Firestore/Cloud Storage involved
- **Compiler**: g++
- **Networking**: QtNetwork (REST APIs)

## Getting Started

There's no packaged release yet — the project is still being polished before a first public
release, so building from source is currently the only way to run it.

Sign-up/login works out of the box once built. Cloud file storage additionally needs a running
copy of the [`server/`](server/) backend — see **Cloud Backend** below.

### Build from source

The build uses whatever C++ compiler is already on your machine (e.g. an existing MinGW/g++ on
Windows) — see [`windows/README.md`](windows/README.md) if you'd rather bundle a compiler in
alongside the build instead of relying on `PATH`.

**Prerequisites**
- [Qt](https://www.qt.io/download-qt-installer) (Qt 6 recommended, Qt 5 also supported) with the **Widgets** and **Network** modules
- CMake 3.16+
- A C++17 compiler (`g++` or Clang) on `PATH`
- [Qt Creator](https://www.qt.io/product/development-tools) — optional, but the simplest way to get Qt and build in one step

**Build**

1. Clone the repo:
   ```bash
   git clone https://github.com/rue08/CPP-Desktop-IDE.git
   cd CPP-Desktop-IDE
   ```

2. Build it, either way:

   **Using Qt Creator** (easiest)
   - File → Open Project → select `CMakeLists.txt`
   - Let it configure the kit, then hit **Run** ▶️ in the bottom-left corner.

   **From the command line**
   ```bash
   cmake -B build -DCMAKE_PREFIX_PATH=<path-to-your-Qt-install>   # e.g. ~/Qt/6.10.1/macos
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

3. Launch the built `VaultWright` binary.

## Cloud Backend

File storage (upload/list/download) talks to the [`server/`](server/) Node.js + PostgreSQL
backend, not Firebase — see [`server/README.md`](server/README.md) to run it (a `docker compose
up --build` away). Firebase Authentication is unaffected; sign-up/login/refresh works
independently of this.

Because the backend's URL isn't baked into the app (it can change — e.g. an ngrok free-tier
tunnel gets a new URL on every restart), point the app at your instance via the ⚙️ **Settings**
button in the toolbar before using cloud features. It's saved and reused on future launches.

## Using the IDE

1. **Point the app at your backend** — click ⚙️ **Settings** in the toolbar and enter the
   backend's URL (see **Cloud Backend** above). Only needed once, or whenever the URL changes.
2. **Sign up / log in** — click the login icon in the toolbar and create an account with an email and password. This is what ties your files to you across machines.
3. **Create or open a file** — `Ctrl+N` for a new file, `Ctrl+O` to open an existing one, or "Open Folder..." to work across a whole project.
4. **Write and save** — `Ctrl+S` saves locally, same as any editor.
5. **Run it** — hit the ▶️ **Run** button to compile and execute the current file with `g++`; output opens in a terminal window.
6. **Push to the cloud** — with the file open, click the ☁️ **Upload** button to save it to your account.
7. **Pull it down elsewhere** — open **The Vault** from the toolbar to browse the files you've uploaded, and click one to bring it down to whatever machine you're on.

## Project Status
The main aim is to build a full-featured project, and it's under active development.

### Coming next
1. More robust file handling, including uploading whole folders (currently limited to individual code files).
2. Continued UI/UX improvements.

## Author
Mehul Sharma\
Gmail: mehssi2004@gmail.com

If you have any suggestions feel free to reach out to me via mail, and if you liked the project, make sure to give a star ⭐️.
