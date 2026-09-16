#!/bin/sh
set -eu
if [ "$(id -u)" -eq 0 ]; then
    echo 'Run as your desktop user, not root. sudo is used only for missing dependencies.' >&2
    exit 1
fi
for tool in curl tar; do
    command -v "$tool" >/dev/null || { echo "Install $tool first." >&2; exit 1; }
done
ready=true
for tool in cc make pkg-config wayland-scanner; do
    command -v "$tool" >/dev/null || ready=false
done
if [ "$ready" = true ]; then
    pkg-config --exists wayland-client cairo libpulse wayland-protocols || ready=false
fi
if [ "$ready" = false ]; then
    command -v sudo >/dev/null || { echo 'Install the dependencies from README.md first (sudo not found).' >&2; exit 1; }
    if command -v pacman >/dev/null; then
        sudo pacman -S --needed base-devel pkgconf wayland wayland-protocols cairo libpulse python git
    elif command -v apt-get >/dev/null; then
        sudo apt-get update
        sudo apt-get install -y build-essential pkg-config libwayland-dev wayland-protocols libcairo2-dev libpulse-dev python3 git
    else
        echo 'Install build dependencies from README.md, then run this installer again.' >&2
        exit 1
    fi
fi
release_dir=$(mktemp -d)
trap 'rm -rf "$release_dir"' EXIT HUP INT TERM
curl -fsSL --retry 3 https://github.com/mir4zul/hypr-visualizer/archive/refs/heads/main.tar.gz -o "$release_dir/source.tar.gz"
tar -xzf "$release_dir/source.tar.gz" -C "$release_dir"
"$release_dir/hypr-visualizer-main/install.sh" "$@"
