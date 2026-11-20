# Working on Peekstep

Peekstep is a small, fast-starting STEP viewer (C++20, OpenCASCADE, GLFW,
Dear ImGui, Vulkan) for Linux, macOS (Apple Silicon) and Windows (MSYS2
UCRT64). Dependencies come from vcpkg and are linked statically. See
README.md for building and releasing.

## Ask, don't assume

When something is unclear or there are several reasonable choices, ask the
maintainer instead of picking one. Show options, with previews where it
helps, before committing to a design.

## CI rebuilds are expensive

CI builds OpenCASCADE from source whenever the vcpkg binary cache misses. That
takes about 45 minutes on Windows and runs on every platform. GitHub only
shares Actions caches from `main`. A cache saved by a pull request is only
visible to that same pull request.

Rules:

- **Ask first** before changing anything that invalidates the vcpkg cache,
  and say that it causes a full rebuild. That means:
  - `vcpkg.json` dependencies, features, `builtin-baseline` or
    `vcpkg-configuration`
  - `cmake/triplets/*`
  - `VCPKG_COMMIT` in `.github/workflows/build.yml`
  - the triplet or toolchain variables in `CMakePresets.json`
  - the runner `os:` in the CI matrix
  - MSYS2 compiler packages
  - the cache key in the workflow
  
  Bumping only `version-string` in `vcpkg.json` for a release is fine, because
  the packages are restored through the cache's restore key.
- Put such a change in **its own small pull request**, not mixed with app
  changes, and batch dependency changes together.
- After such a change is merged, **don't push other pull requests until
  `main`'s build has finished** and saved its cache.
- Don't push while another CI run that will produce a cache is still
  building. Wait for it to finish.
- Note in a pull request whether it is cache-neutral.

## Icons

The app icon's source is `packaging/icon/peekstep.svg`. After changing it, run
`python3 packaging/icon/make-icons.py` (needs `cairosvg` and `pillow`) and
commit the regenerated files.
