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
- **BaaS + Cloud**: Firebase (Authentication, Firestore, Cloud Storage)
- **Compiler**: g++
- **Networking**: QtNetwork (REST APIs)

## Getting Started

### Option 1: Download a build (easiest)

Grab the latest packaged release for your OS from the [Releases page](https://github.com/rue08/CPP-Desktop-IDE/releases). Nothing else to install first:

- **macOS**: open the `.dmg` and drag the app into Applications.
- **Windows**: unzip and run `IDE.exe`. The download includes everything needed to compile and run C++ files — no separate compiler install required.

That's it — no extra config needed either way, the cloud backend ships preconfigured out of the box.

### Option 2: Build from source

Already have a C++ compiler installed (e.g. an existing MinGW/g++ on Windows)? This is the simpler path for you — no need for the larger bundled download, the build just uses what's already on your machine.

**Prerequisites**
- [Qt](https://www.qt.io/download-qt-installer) (Qt 6 recommended, Qt 5 also supported) with the **Widgets** and **Network** modules
- CMake 3.16+
- A C++17 compiler (`g++` or Clang) on `PATH` — on Windows, see [`windows/README.md`](windows/README.md) for bundling one into your own build instead
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

3. Launch the built `IDE` binary.

## Using the IDE

1. **Sign up / log in** — click the login icon in the toolbar and create an account with an email and password. This is what ties your files to you across machines.
2. **Create or open a file** — `Ctrl+N` for a new file, `Ctrl+O` to open an existing one, or "Open Folder..." to work across a whole project.
3. **Write and save** — `Ctrl+S` saves locally, same as any editor.
4. **Run it** — hit the ▶️ **Run** button to compile and execute the current file with `g++`; output opens in a terminal window.
5. **Push to the cloud** — with the file open, click the ☁️ **Upload** button to save it to your account.
6. **Pull it down elsewhere** — open **The Vault** from the toolbar to browse the files you've uploaded, and click one to bring it down to whatever machine you're on.

## Project Status
The main aim is to build a full-featured project, and it's under active development.

### Coming next
1. Move from Firebase to a Node.js backend.
2. More robust file handling, including uploading whole folders (currently limited to individual code files).
3. Continued UI/UX improvements.

## Author
Mehul Sharma\
Gmail: mehssi2004@gmail.com

If you have any suggestions feel free to reach out to me via mail, and if you liked the project, make sure to give a star ⭐️.
