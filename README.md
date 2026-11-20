# Peekstep

A small, fast-starting STEP file viewer for Linux, macOS and Windows. It is
meant for taking a quick look at a `.step` file, for example a KiCad board
export, straight from the file browser or the command line.

- Opens the window immediately and loads the file in the background.
- Shows the colors from the file, plus a list of colors and materials.
- Rotate, pan, zoom and standard views. Nothing else (yet).

> **AI-developed software.** Peekstep was written almost entirely by an AI
> coding assistant (Claude, by Anthropic), directed and reviewed by a human.
> It works for its author, but the code has not had the kind of line-by-line
> human review a hand-written project gets. Use it at your own risk, as the
> license says.

Built with C++20, [OpenCASCADE](https://dev.opencascade.org/) for reading
STEP, Vulkan for drawing, and GLFW + Dear ImGui for the window and UI.

## Download

Get the latest version from the
[Releases page](https://github.com/hin/peekstep/releases).

- **Windows (x64):** `peekstep-<version>-windows-x64.exe` is the program
  itself; download it and run it. Or use `peekstep-<version>-windows-x64-setup.exe`,
  which installs Peekstep for your user (no admin rights) with a Start menu
  entry, an uninstaller and an optional checkbox to open `.step`/`.stp` files
  with Peekstep.
- **macOS (Apple Silicon, macOS 13 or newer):** unzip `Peekstep-<version>-macos-arm64.zip` and
  move Peekstep to Applications. The app is not signed by Apple, so the first
  time you open it, right-click it and choose Open (or allow it under System
  Settings, Privacy & Security). Peekstep shows up under "Open With" for STEP
  files. To make it the double-click app, use Get Info on a `.step` file,
  choose Peekstep under "Open with" and click Change All.
- **Linux (x64):** unpack `peekstep-<version>-linux-x64.tar.gz` and run
  `peekstep`. Or run `./install.sh` from that folder: it installs Peekstep to
  `~/.local/bin` with a menu entry, and asks whether it should become the
  default app for STEP files (`./install.sh --uninstall` removes it again).
  Built on Ubuntu 22.04, so it runs on distributions with glibc 2.35 or newer.
  It needs a Vulkan driver, which most desktop installs have.

## Usage

```
peekstep [file.step]
peekstep --info file.step   # print parts, colors and timings, no window
peekstep --vulkan-info      # print the Vulkan drivers and GPUs found (for troubleshooting)
```

You can also drop a file on the window.

| Input | Action |
| --- | --- |
| Left drag | Rotate |
| Right or middle drag, Shift + left drag | Pan |
| Wheel | Zoom |
| F | Fit the model in the window |
| 0 | Iso view |
| 1 / 3 / 7 | Front / right / top (with Ctrl: back / left / bottom) |
| Tab | Hide or show the panel |
| F1 | About Peekstep |
| Esc | Quit |

Set `PEEKSTEP_VALIDATION=1` to turn on the Vulkan validation layer.

## Building

You need CMake 3.21+, Ninja, a C++20 compiler and
[vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` pointing at it.
vcpkg builds all dependencies statically, so the result is a single
executable. The first build compiles OpenCASCADE and takes a while; later
builds reuse it.

Your vcpkg checkout must be at least as new as the `builtin-baseline` in
`vcpkg.json`. Errors like "no version database entry for ..." mean it is too
old: run `git pull` in the vcpkg folder, then its bootstrap script.

```
cmake --preset <linux|mingw|macos>
cmake --build --preset <linux|mingw|macos>
```

The binary ends up in `build/<preset>/`.

### Linux

Install the X11 and autotools packages vcpkg needs for GLFW, for example on
Ubuntu:

```
sudo apt install ninja-build autoconf autoconf-archive automake libtool pkg-config bison \
    libxinerama-dev libxcursor-dev libxrandr-dev libxi-dev xorg-dev libgl1-mesa-dev
```

The executable still uses the system's C library, X11 and Vulkan loader
(`libvulkan.so.1`, part of the graphics driver packages).

### Windows (MSYS2 UCRT64)

From an MSYS2 UCRT64 shell:

```
pacman -S --needed git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pkgconf
export VCPKG_ROOT=/c/path/to/vcpkg
cmake --preset mingw
cmake --build --preset mingw
```

The only runtime dependency is `vulkan-1.dll`, which comes with the graphics
driver.

### macOS

```
brew install ninja
cmake --preset macos
cmake --build --preset macos
```

Vulkan runs on top of MoltenVK. To run the plain binary from the build
folder, MoltenVK must be installed (for example from the LunarG Vulkan SDK).
The release app bundles the Vulkan loader and MoltenVK; see
`packaging/macos/make-app.sh`.

## Making a release

1. Set the new version in `project(peekstep VERSION ...)` in `CMakeLists.txt`
   (and `version-string` in `vcpkg.json`), and merge that to `main`.
2. Tag it and push the tag: `git tag v0.2.0 && git push origin v0.2.0`.
3. CI builds the packages for all platforms and creates a draft release with
   them. Check the notes on the Releases page and publish it.

CI refuses to build a release when the tag does not match the version in
`CMakeLists.txt`. Packaging files are in `packaging/`.

## License

Peekstep is under the BSD 2-Clause license, see [LICENSE](LICENSE).

Binaries also contain OpenCASCADE (LGPL 2.1 with an exception), GLFW (zlib),
Dear ImGui (MIT), the Vulkan loader (Apache 2.0), on macOS MoltenVK (Apache
2.0), and their dependencies, under their own licenses. Each release package
includes these licenses in `THIRD-PARTY-LICENSES.txt`. OpenCASCADE is linked
statically; to rebuild Peekstep against a modified OpenCASCADE, build it from
this repository as described above, where the OpenCASCADE version is pinned
through vcpkg.
