#!/bin/sh
set -eu
cd "$(dirname "$0")"
if [ "$(id -u)" -eq 0 ]; then
    echo 'Run ./install.sh as your desktop user, not root.' >&2; exit 1
fi
lockscreen=auto
start=true
for arg in "$@"; do
    case "$arg" in
        --no-lockscreen) lockscreen=no ;;
        --no-start) start=false ;;
        --help) echo 'Usage: ./install.sh [--no-lockscreen] [--no-start]'; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done
for tool in cc make pkg-config wayland-scanner; do
    command -v "$tool" >/dev/null || { echo "Missing $tool. See README.md for dependencies." >&2; exit 1; }
done
pkg-config --exists wayland-client cairo libpulse || {
    echo 'Install build dependencies listed in README.md first.' >&2; exit 1;
}
config_dir="${XDG_CONFIG_HOME:-$HOME/.config}/hypr"
if [ -f "$config_dir/hyprland.lua" ]; then
    config="$config_dir/hyprland.lua"
    entry='hl.on("hyprland.start", function() hl.exec_cmd("\"" .. os.getenv("HOME") .. "/.local/bin/hypr-visualizer\"") end)'
    marker='-- hypr-visualizer desktop audio bars'
elif [ -f "$config_dir/hyprland.conf" ]; then
    config="$config_dir/hyprland.conf"
    entry='exec-once = "$HOME/.local/bin/hypr-visualizer"'
    marker='# hypr-visualizer desktop audio bars'
else
    echo "No Hyprland config found in $config_dir. Run this installer as your desktop user." >&2
    exit 1
fi
make
make install PREFIX="$HOME/.local"
if [ "$lockscreen" = auto ] && [ -f "${XDG_CONFIG_HOME:-$HOME/.config}/quickshell/dms/Modules/Lock/LockScreenContent.qml" ]; then
    if command -v python3 >/dev/null; then
        python3 lockscreen/install.py || echo 'Desktop installed; DMS integration skipped. See the message above.' >&2
    else
        echo 'Desktop installed; install Python 3 and rerun to enable DMS lockscreen support.' >&2
    fi
fi
install -Dm755 uninstall.sh "${XDG_DATA_HOME:-$HOME/.local/share}/hypr-visualizer/uninstall.sh"
if ! grep -Fq '/.local/bin/hypr-visualizer' "$config"; then
    cp -p "$config" "$config.hypr-visualizer-backup-$(date +%s)"
    printf '\n%s\n%s\n' "$marker" "$entry" >> "$config"
fi
if [ "$start" = true ] && [ -n "${HYPRLAND_INSTANCE_SIGNATURE:-}" ] && [ -n "${WAYLAND_DISPLAY:-}" ]; then
    if command -v systemctl >/dev/null; then
        systemctl --user stop hypr-visualizer.service 2>/dev/null || :
    fi
    pkill -u "$(id -u)" -x hypr-visualizer 2>/dev/null || :
    # Give the old instance time to release its session lock.
    sleep 1
    log_dir="${XDG_STATE_HOME:-$HOME/.local/state}/hypr-visualizer"
    mkdir -p "$log_dir"
    if command -v systemd-run >/dev/null && systemctl --user show-environment >/dev/null 2>&1; then
        systemd-run --user --collect --unit=hypr-visualizer \
            --property=Restart=on-failure --property=RestartSec=2 \
            --setenv="WAYLAND_DISPLAY=$WAYLAND_DISPLAY" \
            --setenv="HYPRLAND_INSTANCE_SIGNATURE=$HYPRLAND_INSTANCE_SIGNATURE" \
            --setenv="XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR" \
            --property="StandardOutput=append:$log_dir/visualizer.log" \
            --property="StandardError=append:$log_dir/visualizer.log" \
            "$HOME/.local/bin/hypr-visualizer"
        sleep 2
        systemctl --user is-active --quiet hypr-visualizer.service || {
            echo "Startup failed. See $log_dir/visualizer.log" >&2; exit 1;
        }
    else
        nohup "$HOME/.local/bin/hypr-visualizer" > "$log_dir/visualizer.log" 2>&1 < /dev/null &
        pid=$!
        sleep 1
        if ! kill -0 "$pid" 2>/dev/null; then
            echo "Installed, but startup failed. See $log_dir/visualizer.log" >&2
            exit 1
        fi
    fi
    echo 'Installed and running. Autostart enabled for your next Hyprland login.'
else
    echo 'Installed. Starts automatically at your next Hyprland login.'
fi
