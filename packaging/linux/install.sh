#!/bin/sh
# Installs Peekstep for the current user (no root needed):
#   ~/.local/bin/peekstep, a menu entry with icon, and the STEP file type.
# Optionally makes Peekstep the default app for STEP files.
#
#   ./install.sh               install, ask about the default app
#   ./install.sh --default     install and make it the default for STEP files
#   ./install.sh --no-default  install, leave the default app alone
#   ./install.sh --uninstall   remove everything this script installed
set -eu

here=$(cd "$(dirname "$0")" && pwd)
data="${XDG_DATA_HOME:-$HOME/.local/share}"
bin="$HOME/.local/bin"
apps="$data/applications"
mime="$data/mime"
icons="$data/icons/hicolor"

refresh() {
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q "$apps" || true
    command -v update-mime-database >/dev/null 2>&1 && update-mime-database "$mime" || true
    command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t "$icons" || true
}

mode=ask
case "${1:-}" in
    --default) mode=yes ;;
    --no-default) mode=no ;;
    --uninstall)
        rm -f "$bin/peekstep" "$apps/peekstep.desktop" "$mime/packages/peekstep-step.xml"
        rm -f "$icons"/*/apps/peekstep.png "$icons"/scalable/apps/peekstep.svg
        refresh
        echo "Peekstep removed."
        exit 0 ;;
    "") ;;
    *) sed -n '2,9p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac

mkdir -p "$bin" "$apps" "$mime/packages"
install -m 755 "$here/peekstep" "$bin/peekstep"
# Absolute path, so the menu entry works even if ~/.local/bin is not on PATH.
sed "s|^Exec=peekstep|Exec=\"$bin/peekstep\"|" "$here/peekstep.desktop" > "$apps/peekstep.desktop"
install -m 644 "$here/peekstep-step.xml" "$mime/packages/peekstep-step.xml"
(cd "$here/icons/hicolor" && for f in */apps/peekstep.*; do
    mkdir -p "$icons/$(dirname "$f")"
    install -m 644 "$f" "$icons/$f"
done)
refresh
echo "Installed $bin/peekstep and a menu entry."

if [ "$mode" = ask ]; then
    if [ -t 0 ]; then
        printf "Open STEP files with Peekstep by default (double-click in the file manager)? [y/N] "
        read -r answer || answer=
        case "$answer" in [yY]*) mode=yes ;; *) mode=no ;; esac
    else
        mode=no
    fi
fi
if [ "$mode" = yes ]; then
    if command -v xdg-mime >/dev/null 2>&1; then
        xdg-mime default peekstep.desktop model/step
        echo "Peekstep is now the default app for STEP files."
    else
        echo "xdg-mime not found; choose Peekstep under \"Open With\" in your file manager instead."
    fi
else
    echo "Peekstep is listed under \"Open With\" for STEP files; the default app is unchanged."
fi

case ":$PATH:" in
    *":$bin:"*) ;;
    *) echo "Note: $bin is not on your PATH, so 'peekstep' won't work in a terminal until you add it." ;;
esac
