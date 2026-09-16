# hypr-visualizer

A small native audio visualizer for **Hyprland**. Rounded pastel bars react to your music across every monitor, with bass in the center, mirrored motion, and a soft fade into your wallpaper.

![Mirrored pastel audio bars with a soft bottom fade](docs/preview.svg)

*Illustrative preview. The app draws only the bars; your wallpaper stays in place. Actual heights follow your audio.*

- **No settings window or runtime config.** Change the source theme and rebuild.
- **All monitors**, including monitors connected after startup.
- **42 bars**, smooth attack and release, 60% opacity, and a fading base.
- **Output audio only**, including PipeWire-Pulse and Bluetooth; follows the default output.
- **Click-through desktop layer.** Windows stay above the bars.
- **Native C + Wayland + Cairo.** No browser, GTK, Qt, or CAVA needed for the desktop app.
- **Optional DMS lockscreen integration**, using the same theme and audio analysis.

## Quick install

Run inside your Hyprland session as your normal user. You need `curl` and `tar` first.

```sh
curl -fsSL https://raw.githubusercontent.com/mir4zul/hypr-visualizer/main/quick-install.sh | sh
```

The script installs missing build dependencies on **Arch-based** or **Debian/Ubuntu-based** systems using `sudo`, downloads the source, builds locally, installs into `~/.local/bin`, adds autostart, and starts the visualizer. Hyprland itself and a working audio server must already be installed. Other distributions can use the manual instructions below.

To download and inspect the script before running it:

```sh
curl -fsSLO https://raw.githubusercontent.com/mir4zul/hypr-visualizer/main/quick-install.sh
less quick-install.sh
sh quick-install.sh
```

## Manual install

Install build dependencies for your distribution.

**Arch Linux / EndeavourOS / Manjaro**

```sh
sudo pacman -S --needed base-devel git pkgconf wayland wayland-protocols cairo libpulse python
```

**Debian / Ubuntu** — in an existing Hyprland environment:

```sh
sudo apt-get update
sudo apt-get install -y git build-essential pkg-config libwayland-dev wayland-protocols libcairo2-dev libpulse-dev python3
```

Then clone and install:

```sh
git clone https://github.com/mir4zul/hypr-visualizer.git
cd hypr-visualizer
./install.sh
```

On other distributions, install a C compiler, Make, pkg-config, wayland-scanner, Wayland headers/protocols, Cairo headers, and libpulse headers. Python 3 is used for DMS integration and uninstalling.

### Build only

```sh
make
make test
./build/hypr-visualizer
```

`make install PREFIX="$HOME/.local"` installs only the binary. It does not add autostart or lockscreen integration. Run the binary inside Hyprland.

## What the installer changes

- Installs `~/.local/bin/hypr-visualizer` and an uninstall script.
- Backs up your Hyprland config before adding one autostart line.
- Supports `hyprland.lua` and `hyprland.conf` in `$XDG_CONFIG_HOME/hypr` (default `~/.config/hypr`). Lua takes priority if both exist. Custom config locations need manual autostart setup.
- Starts immediately in an active Hyprland session. Uses a transient systemd user service when available; otherwise launches a background process.
- Reinstalling updates the binary without duplicating autostart.
- Adds DMS lockscreen bars if a compatible local DMS source config is found.

Skip optional lockscreen integration or immediate startup:

```sh
./install.sh --no-lockscreen
./install.sh --no-start
```

## Lockscreen

The integration supports **DankMaterialShell (DMS)** with its source config at `~/.config/quickshell/dms`. The installer backs up `Modules/Lock/LockScreenContent.qml`, adds a visualizer loader behind the controls, and installs the QML components in your XDG data directory.

Lockscreen views share one additional audio-only helper while visible. Silence makes the bars disappear. A powered-off monitor cannot display the visualizer. Other screen lockers are not supported by this integration.

DMS updates may replace the patched lockscreen file. Re-run `./install.sh` afterward. If DMS disables hot reload, reload DMS while your session is unlocked. An incompatible DMS layout is skipped without preventing desktop installation.

## Appearance

Edit [`theme.h`](theme.h), then run `./install.sh` again. The lockscreen receives the theme from the compiled binary too.

| Constant | Default | Effect |
| --- | --- | --- |
| `BAR_COUNT` | `42` | Total visible bars |
| `BAR_HEIGHT_RATIO` | `0.48` | Maximum height relative to the monitor |
| `BAR_WIDTH_RATIO` | `0.72` | Bar width within each slot; the rest is gap |
| `OPACITY` | `0.60` | Main bar opacity |
| `BASE_ALPHA` | `0.22` | Bottom opacity multiplier |
| `ATTACK_SECONDS` | `0.055` | How quickly bars rise |
| `RELEASE_SECONDS` | `0.24` | How slowly bars fall |
| `FPS` | `30` | Rendering target |

`palette` controls the left-to-right colors. Bass and treble gains adjust the visual balance without changing speaker volume. Keep bar count at least 2, FPS positive, and ratios/opacity between 0 and 1.

## Update and uninstall

For a cloned checkout:

```sh
git pull --ff-only
./install.sh
```

Or rerun the quick installer. Commit or save your `theme.h` edits before updating; the quick installer uses the default upstream theme.

Uninstall:

```sh
sh "${XDG_DATA_HOME:-$HOME/.local/share}/hypr-visualizer/uninstall.sh"
```

The uninstaller stops the app, removes its autostart and DMS loader, and deletes installed app files. Config backups and logs remain available. Your source checkout is kept.

## Troubleshooting and limitations

- **Nothing visible:** play audio and expose the desktop. Bars are behind application windows and fade away during silence.
- **No audio response:** check that playback uses your default output and PipeWire-Pulse or PulseAudio is running.
- **Logs:** `${XDG_STATE_HOME:-$HOME/.local/state}/hypr-visualizer/visualizer.log`.
- **Stop:** `systemctl --user stop hypr-visualizer.service` for the installer-started service, or `pkill -x hypr-visualizer` for a direct launch.
- Tested on an Arch Linux Hyprland session with two 1080p monitors and Bluetooth audio. Other distro install paths are provided but have not all been tested on a desktop.
- Two shared-memory pixel buffers are used per monitor; rendering cost grows with resolution. Fractional scaling currently uses compositor scaling.
- The spectrum uses Hann-windowed Goertzel measurements with visual weighting. It is decorative, not a calibrated audio analyzer or the CAVA engine.

## Development

```sh
make
make test
python3 tests/install.py
```

Tests cover silence, tones, attack/release, mirrored frequency placement, and isolated install/update/uninstall cycles for both config formats. Installer tests use temporary homes and do not change your live session.

## License

[MIT](LICENSE). The vendored [wlr-layer-shell protocol](https://gitlab.freedesktop.org/wlroots/wlr-protocols) retains its upstream license.
