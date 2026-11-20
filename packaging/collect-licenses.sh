#!/bin/bash
# Writes the licenses of the libraries built into Peekstep to one text file.
#   collect-licenses.sh <vcpkg installed dir for the triplet> <out file> [extra "name=license file" ...]
set -euo pipefail

installed=$1 out=$2
shift 2

{
    echo "Peekstep includes the following third-party software, each under its own license."
    echo
    echo "OpenCASCADE is under the GNU LGPL 2.1 with an additional exception, and Peekstep"
    echo "links it statically. Peekstep's complete source and build files are at"
    echo "https://github.com/hin/peekstep, and the exact OpenCASCADE version (from"
    echo "https://github.com/Open-Cascade-SAS/OCCT) is pinned there through vcpkg, so you can"
    echo "rebuild Peekstep against a modified OpenCASCADE."
    for dir in "$installed"/share/*/; do
        name=$(basename "$dir")
        case "$name" in
            vcpkg-*|glslang|spirv-tools|spirv-headers) continue ;; # build tools only, not in the binary
        esac
        [ -f "$dir/copyright" ] || continue
        printf '\n\n==============================================================================\n%s\n==============================================================================\n\n' "$name"
        cat "$dir/copyright"
    done
    for extra in "$@"; do
        printf '\n\n==============================================================================\n%s\n==============================================================================\n\n' "${extra%%=*}"
        cat "${extra#*=}"
    done
} > "$out"
echo "Wrote $out"
