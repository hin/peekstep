# Like vcpkg's arm64-osx-release, but for an older macOS than the build
# machine's, so release builds run on more Macs. Keep in sync with
# CMAKE_OSX_DEPLOYMENT_TARGET in the "macos" preset.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)
set(VCPKG_OSX_DEPLOYMENT_TARGET 13.0)
set(VCPKG_BUILD_TYPE release)
