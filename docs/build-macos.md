# Building qBittorrent on macOS

This guide documents a locally verified build workflow for the GUI application on macOS using Homebrew-provided dependencies.

## Prerequisites

- A recent macOS installation with Xcode Command Line Tools available
- [Homebrew](https://brew.sh/)

Install the build dependencies:

```shell
brew install cmake ninja pkgconf qt boost zlib libtorrent-rasterbar
```

The dependency versions are still governed by the project's supported ranges in [`INSTALL`](../INSTALL). If you report a build problem upstream, make sure your setup matches a supported configuration.

## Configure and build

From the repository root:

```shell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTESTING=ON -DVERBOSE_CONFIGURE=ON
cmake --build build -j8
```

## Run the test suite

```shell
cmake --build build --target check -j8
```

## Make the app bundle self-contained

The plain build can succeed while the generated `.app` bundle still fails to start because the required Qt plugins are not yet bundled inside it. Deploy the Qt frameworks and plugins into the application bundle with:

```shell
macdeployqt build/qbittorrent.app -no-strip
```

If `macdeployqt` is not on your `PATH`, invoke it via Homebrew's prefix:

```shell
/opt/homebrew/bin/macdeployqt build/qbittorrent.app -no-strip
```

The generated `qt.conf` inside the application bundle should contain:

```ini
[Paths]
Translations = translations
Plugins = PlugIns
```

## Launch the app

After running `macdeployqt`, launch the packaged application with either:

```shell
open build/qbittorrent.app
```

or:

```shell
build/qbittorrent.app/Contents/MacOS/qbittorrent
```

## Notes

- This guide covers local contributor builds only.
- It does not document release signing, notarization, or release artifact publishing.
- The repository also contains a macOS CI workflow in [`.github/workflows/ci_macos.yaml`](../.github/workflows/ci_macos.yaml), which is useful as a reference when investigating build failures.
