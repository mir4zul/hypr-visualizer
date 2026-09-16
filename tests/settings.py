"""Exercise the real settings CLI and a running frame helper in a temporary home."""
import json
import os
import selectors
import subprocess
import tempfile
import time
from pathlib import Path

repo = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='visualizer-settings-') as tmp:
    env = dict(os.environ, XDG_CONFIG_HOME=tmp)
    config = Path(tmp) / 'hypr-visualizer/config'
    def cli(*args, ok=True):
        result = subprocess.run(['python3', str(repo / 'settings.py'), *args], env=env, capture_output=True, text=True)
        assert (result.returncode == 0) == ok, result.stderr
    cli('--classic')
    process = subprocess.Popen([str(repo / 'build/hypr-visualizer'), '--levels'], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    selector = selectors.DefaultSelector();selector.register(process.stdout, selectors.EVENT_READ)
    def frame_matching(predicate):
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            if selector.select(0.2):
                frame = json.loads(process.stdout.readline())
                if predicate(frame): return frame
        raise AssertionError('helper did not apply settings')
    try:
        frame_matching(lambda f: f['style'] == 0 and f['glow'] == 0 and f['spacing'] == 30)
        cli('--set', 'theme=aurora', '--set', 'style=wave', '--set', 'glow=0.7', '--set', 'motion=natural', '--set', 'gap=4', '--set', 'bar_width=12')
        f = frame_matching(lambda f: f['style'] == 1 and f['glow'] == 0.7 and f['spacing'] == 16)
        assert f['colors'][0] == '#63e6be'
        before = config.read_bytes();cli('--set', 'opacity=nan', ok=False);assert config.read_bytes() == before
        config.write_text('opacity=nan\n')
        time.sleep(1.2)
        frame_matching(lambda f: f['style'] == 1 and f['glow'] == 0.7)
        cli('--classic')
        frame_matching(lambda f: f['style'] == 0 and f['glow'] == 0)
        from PIL import Image
        wallpaper = Path(tmp) / 'wall paper.png';Image.new('RGB', (24, 24), '#9c6ef5').save(wallpaper)
        cli('--wallpaper', str(wallpaper))
        frame_matching(lambda f: f['colors'][0] == '#9c6ef5' and f['colors'][-1] == '#9c6ef5')
        config.unlink()
        frame_matching(lambda f: f['colors'][0] == '#ffdf7e')
    finally:
        process.terminate();process.wait(timeout=5);selector.close()
print('PASS: CLI validation, live reload, invalid config recovery, Classic reset, wallpaper palette and deleted-config fallback')
