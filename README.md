# Peekstep

A small, fast-starting STEP file viewer for Linux, macOS and Windows. It is
meant for taking a quick look at a `.step` file, for example a KiCad board
export, straight from the file browser or the command line.

- Opens the window immediately and loads the file in the background.
- Shows the colors from the file, plus a list of colors and materials.
- Rotate, pan, zoom and standard views. Nothing else (yet).

Built with C++20, [OpenCASCADE](https://dev.opencascade.org/) for reading
STEP, Vulkan for drawing, and GLFW + Dear ImGui for the window and UI.

## Usage

```
peekstep [file.step]
peekstep --info file.step   # print parts, colors and timings, no window
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
| Esc | Quit |

Set `PEEKSTEP_VALIDATION=1` to turn on the Vulkan validation layer.

## Building

You need CMake 3.21+, Ninja, a C++20 compiler and
[vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` pointing at it.
vcpkg builds all dependencies statically, so the result is a single
executable. The first build compiles OpenCASCADE and takes a while; later
builds reuse it.

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
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
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

Vulkan runs on top of MoltenVK. For now the Vulkan loader and MoltenVK must be
installed separately (for example from the LunarG Vulkan SDK).

## License

Peekstep is under the BSD 2-Clause license, see [LICENSE](LICENSE).

Binaries also contain OpenCASCADE (LGPL 2.1 with an exception), GLFW (zlib),
Dear ImGui (MIT) and their dependencies, under their own licenses.
