#!/bin/sh
set -eu
if [ "$(id -u)" -eq 0 ]; then
    echo 'Run as your desktop user, not root.' >&2; exit 1
fi
command -v python3 >/dev/null || { echo 'Python 3 is required to remove config entries safely.' >&2; exit 1; }
if command -v systemctl >/dev/null; then
    systemctl --user stop hypr-visualizer.service 2>/dev/null || :
fi
pkill -u "$(id -u)" -x hypr-visualizer 2>/dev/null || :
python3 - <<'PY'
import os, re, shutil
from pathlib import Path
from datetime import datetime
home = Path.home()
config = Path(os.environ.get('XDG_CONFIG_HOME', home / '.config'))
for target in (config / 'hypr/hyprland.conf', config / 'hypr/hyprland.lua', config / 'quickshell/dms/Modules/Lock/LockScreenContent.qml'):
    if not target.is_file():
        continue
    original = target.read_text()
    if target.suffix == '.qml':
        changed = re.sub(r'    Loader \{\n        id: hyprVisualizerBars\n.*?\n    \}\n\n', '', original, count=1, flags=re.S)
    else:
        changed = ''.join(line for line in original.splitlines(keepends=True)
                          if not (('/.local/bin/hypr-visualizer' in line and (line.lstrip().startswith('exec-once') or line.lstrip().startswith('hl.on(')))
                                  or line.strip() in ('# hypr-visualizer desktop audio bars', '-- hypr-visualizer desktop audio bars')))
    if changed != original:
        shutil.copy2(target, str(target) + '.hypr-visualizer-uninstall-backup-' + datetime.now().strftime('%Y%m%d%H%M%S'))
        target.write_text(changed)
(home / '.local/bin/hypr-visualizer').unlink(missing_ok=True)
(config / 'autostart/hypr-visualizer.desktop').unlink(missing_ok=True)
data = Path(os.environ.get('XDG_DATA_HOME', home / '.local/share')) / 'hypr-visualizer'
for name in ('Bars.qml', 'Levels.qml', 'qmldir'):
    (data / 'lockscreen' / name).unlink(missing_ok=True)
(data / 'uninstall.sh').unlink(missing_ok=True)
PY
echo 'Uninstalled. Existing config backups and diagnostic logs were kept.'
