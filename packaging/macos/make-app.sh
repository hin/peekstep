#!/bin/bash
# Builds Peekstep.app around a built peekstep binary, with the Vulkan loader
# and MoltenVK inside, so it runs without the Vulkan SDK.
#
#   make-app.sh <peekstep binary> <vcpkg installed dir for the triplet> <MoltenVK dir> <version> <out dir>
#
# <MoltenVK dir> is the "MoltenVK" folder from the MoltenVK-macos.tar release.
set -euo pipefail

exe=$1 vcpkg=$2 mvk=$3 version=$4 out=$5
here=$(cd "$(dirname "$0")" && pwd)
app="$out/Peekstep.app"

rm -rf "$app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Frameworks" "$app/Contents/Resources/vulkan/icd.d"
cp "$exe" "$app/Contents/MacOS/peekstep"
cp "$here/Peekstep.icns" "$app/Contents/Resources/Peekstep.icns"

# The Vulkan loader, under the name the binary links to (@rpath/libvulkan.1.x.y.dylib).
loader=$(otool -L "$exe" | awk '/@rpath\/libvulkan/ {print $1}' | sed 's|@rpath/||')
[ -n "$loader" ] || { echo "peekstep does not link the Vulkan loader through @rpath" >&2; exit 1; }
cp -L "$vcpkg/lib/$loader" "$app/Contents/Frameworks/$loader"

# MoltenVK. The loader finds drivers in Contents/Resources/vulkan/icd.d of the app bundle.
# The release has a universal binary; keep only the Apple Silicon half.
lipo "$mvk/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib" -thin arm64 -output "$app/Contents/Frameworks/libMoltenVK.dylib"
cat > "$app/Contents/Resources/vulkan/icd.d/MoltenVK_icd.json" <<JSON
{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "../../../Frameworks/libMoltenVK.dylib",
        "api_version": "$(sed -n 's/.*"api_version" *: *"\([^"]*\)".*/\1/p' "$mvk/MoltenVK/dynamic/dylib/macOS/MoltenVK_icd.json")",
        "is_portability_driver": true
    }
}
JSON

# Look for libraries in Contents/Frameworks; drop rpaths that point into the build machine.
for rp in $(otool -l "$app/Contents/MacOS/peekstep" | awk '/LC_RPATH/ {getline; getline; print $2}'); do
    install_name_tool -delete_rpath "$rp" "$app/Contents/MacOS/peekstep"
done
install_name_tool -add_rpath @executable_path/../Frameworks "$app/Contents/MacOS/peekstep"

# Oldest macOS the binary runs on, from its build version.
minos=$(otool -l "$exe" | awk '/LC_BUILD_VERSION/ {f=1} f && $1=="minos" {print $2; exit}')
sed -e "s/@VERSION@/$version/g" -e "s/@MINOS@/${minos:-11.0}/g" "$here/Info.plist.in" > "$app/Contents/Info.plist"
plutil -lint "$app/Contents/Info.plist"

# Changing load commands breaks the linker's signature; Apple Silicon only runs
# signed code, so sign everything ad hoc (this is not an Apple Developer ID).
codesign --force --sign - "$app/Contents/Frameworks/"*.dylib
codesign --force --sign - "$app"
codesign --verify --deep --strict "$app"
echo "Built $app (macOS $minos or newer)"
