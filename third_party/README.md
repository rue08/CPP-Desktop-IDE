# Vendoring Monaco Editor

Every workspace tab is backed by an embedded [Monaco Editor](https://microsoft.github.io/monaco-editor/)
(the editor core from VS Code) — see `MonacoEditor` in `../monacoeditor.h`/`.cpp`, hosted in a
`QWebEngineView` and loading `../monaco-host/index.html`.

`third_party/monaco/` (the actual Monaco JS/CSS build, ~24MB) is **gitignored** — too large for
git, same reasoning as `windows/mingw64/` (see `../windows/README.md` for that one). It's fetched
locally instead of committed.

## One-time setup (per machine building from source)

```bash
third_party/fetch-monaco.sh
```

This downloads a pinned version of the `monaco-editor` npm package (requires `npm` on `PATH`),
and unpacks just its AMD build (`min/vs/`) to `third_party/monaco/vs/`. Re-run it any time to
re-fetch, e.g. after bumping `MONACO_VERSION` in the script.

## What happens at build time

`CMakeLists.txt` copies two things next to the built binary, into a `monaco/` folder:
- `monaco-host/` (tracked in git — the small bridge page + QWebChannel wiring)
- `third_party/monaco/vs/` (this vendored build, into `monaco/vs/`)

Unlike the optional MinGW bundle, this one isn't optional — `third_party/monaco/vs/` missing is a
hard CMake configure error, since there's no fallback editor once `MonacoEditor` is what every tab
uses. Run the fetch script above first.

## Why vendor instead of loading Monaco from a CDN

`MonacoEditor` loads `monaco-host/index.html` from disk (`QUrl::fromLocalFile`), never over the
network — the app needs to work fully offline, and shouldn't take a runtime dependency on a CDN
staying up just to open a file.
