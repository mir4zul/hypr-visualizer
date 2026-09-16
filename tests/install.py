"""Exercise install/update/uninstall against disposable user configs."""
import os
import subprocess
import tempfile
from pathlib import Path

repo = Path(__file__).resolve().parents[1]
for syntax in ('lua', 'conf'):
    with tempfile.TemporaryDirectory(prefix='hypr visualizer test ') as tmp:
        home = Path(tmp)
        config = home / 'config'
        hypr = config / 'hypr'
        hypr.mkdir(parents=True)
        target = hypr / ('hyprland.' + syntax)
        original = '-- keep my settings\n' if syntax == 'lua' else '# keep my settings\n'
        target.write_text(original)
        lock = config / 'quickshell/dms/Modules/Lock/LockScreenContent.qml'
        lock.parent.mkdir(parents=True)
        lock_original = 'Item {\n    SystemClock {\n        id: clock\n    }\n}\n'
        lock.write_text(lock_original)
        mocks = home / 'mocks'
        mocks.mkdir()
        for name in ('pkill', 'systemctl'):
            tool = mocks / name
            tool.write_text('#!/bin/sh\nexit 0\n')
            tool.chmod(0o755)
        env = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(config),
                   XDG_DATA_HOME=str(home / 'data'), HYPRLAND_INSTANCE_SIGNATURE='',
                   PATH=str(mocks) + ':' + os.environ['PATH'])
        for _ in range(2):
            subprocess.run(['./install.sh', '--no-start'], cwd=repo, env=env, check=True, stdout=subprocess.DEVNULL)
        assert target.read_text().count('/.local/bin/hypr-visualizer') == 1
        assert lock.read_text().count('id: hyprVisualizerBars') == 1
        assert (home / '.local/bin/hypr-visualizer').is_file()
        assert (home / 'data/hypr-visualizer/lockscreen/Bars.qml').is_file()
        subprocess.run(['./uninstall.sh'], cwd=repo, env=env, check=True, stdout=subprocess.DEVNULL)
        assert target.read_text().strip() == original.strip()
        assert lock.read_text() == lock_original
        assert not (home / '.local/bin/hypr-visualizer').exists()
        assert not (home / 'data/hypr-visualizer/lockscreen/Bars.qml').exists()
        print(f'PASS: {syntax} install, repeat install, DMS integration, uninstall; paths with spaces')
