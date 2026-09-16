#!/usr/bin/env python3
import os
import shutil
from pathlib import Path
from datetime import datetime

home = Path.home()
config = Path(os.environ.get('XDG_CONFIG_HOME', home / '.config'))
target = config / 'quickshell/dms/Modules/Lock/LockScreenContent.qml'
if not target.is_file():
    raise SystemExit(0)
text = target.read_text()
marker = 'id: hyprVisualizerBars'
anchor = '    SystemClock {\n'
if marker not in text and text.count(anchor) != 1:
    raise SystemExit('DMS lockscreen layout changed; integration was not applied.')
destination = Path(os.environ.get('XDG_DATA_HOME', home / '.local/share')) / 'hypr-visualizer/lockscreen'
destination.mkdir(parents=True, exist_ok=True)
for name in ('Bars.qml', 'Levels.qml', 'qmldir'):
    shutil.copy2(Path(__file__).parent / name, destination / name)
if marker not in text:
    import json
    source = json.dumps((destination / 'Bars.qml').as_uri())
    block = f'''    Loader {{
        id: hyprVisualizerBars
        anchors.fill: parent
        active: root.visible && (Window.window?.visible ?? false)
        source: {source}
    }}

'''
    backup = target.with_name(target.name + '.hypr-visualizer-backup-' + datetime.now().strftime('%Y%m%d%H%M%S'))
    shutil.copy2(target, backup)
    target.write_text(text.replace(anchor, block + anchor, 1))
print('DMS lockscreen visualizer installed.')
