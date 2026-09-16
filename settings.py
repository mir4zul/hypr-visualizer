#!/usr/bin/env python3
"""Small optional GTK settings app. The visualizer itself remains C/Wayland."""
import argparse
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

CONFIG = Path(os.environ.get('XDG_CONFIG_HOME', Path.home() / '.config')) / 'hypr-visualizer/config'
APP_VERSION = '0.5.0'
CLASSIC_COLORS = '#ffdf7e,#e4d783,#aaba87,#719f99,#267e96,#086b94,#385e8a,#65517f,#9e3969,#cc285b,#ec6065,#f09b75,#ffcf7b,#ffdf7e,#e4d783,#aaba87,#719f99,#267e96'
PRESETS = {
    'classic': CLASSIC_COLORS,
    'aurora': '#63e6be,#74c0fc,#b197fc,#f783ac',
    'sunset': '#ffcf70,#ff9068,#ed648e,#8d78dc',
    'ocean': '#91f2d0,#37c6d0,#4288e8,#a49bff',
}
DEFAULTS = dict(theme='classic', style='bars', motion='classic', lockscreen='on', bar_width='21.6', gap='8.4', height='0.48', opacity='0.60', glow='0', attack='0.055', release='0.24')
RANGES = dict(bar_width=(1, 200), gap=(0, 200), height=(0.02, 1), opacity=(0, 1), glow=(0, 1), attack=(0.02, 1), release=(0.05, 2))
CHOICES = dict(theme=[*PRESETS, 'custom', 'wallpaper'], style=['bars', 'wave', 'lines', 'pill'], motion=['classic', 'natural'], lockscreen=['on', 'off'])


def read_config():
    values = DEFAULTS.copy()
    if CONFIG.exists():
        for line in CONFIG.read_text().splitlines():
            line = line.strip()
            if line and not line.startswith('#'):
                key, value = line.split('=', 1)
                values[key.strip()] = value.strip()
    return values


def validate(values):
    import math
    for key, value in values.items():
        if key in CHOICES:
            if value not in CHOICES[key]:
                raise ValueError(f'Invalid {key}: {value}')
        elif key in RANGES:
            lo, hi = RANGES[key]
            number = float(value)
            if not math.isfinite(number) or not lo <= number <= hi:
                raise ValueError(f'{key} must be between {lo} and {hi}')
        elif key == 'colors':
            colors = value.split(',')
            if not 2 <= len(colors) <= 32 or any(not re.fullmatch(r'#[0-9a-fA-F]{6}', c.strip()) for c in colors):
                raise ValueError('Use 2–32 comma-separated colors, for example #74c0fc,#b197fc')
        else:
            raise ValueError(f'Unknown setting: {key}')


def save_config(values):
    validate(values)
    CONFIG.parent.mkdir(parents=True, exist_ok=True)
    # Stable order: theme first, explicit palette last. Replace atomically so the
    # desktop and lockscreen never consume half a settings update.
    keys = [key for key in DEFAULTS if key in values] + (['colors'] if 'colors' in values else [])
    text = '# hypr-visualizer — reloads automatically within one second\n' + ''.join(f'{key}={values[key]}\n' for key in keys)
    fd, name = tempfile.mkstemp(prefix='.config-', dir=CONFIG.parent)
    try:
        with os.fdopen(fd, 'w') as out:
            out.write(text)
        os.replace(name, CONFIG)
    finally:
        Path(name).unlink(missing_ok=True)


def wallpaper_colors(path):
    from PIL import Image, ImageOps
    with Image.open(path) as source:
        image = ImageOps.exif_transpose(source).convert('RGB')
        image.thumbnail((160, 160))
        reduced = image.quantize(colors=8)
        palette = reduced.getpalette()
        ranked = sorted(reduced.getcolors(), reverse=True)
        colors = []
        for _, index in ranked:
            rgb = palette[index * 3:index * 3 + 3]
            if max(rgb) < 35:
                continue
            colors.append('#' + ''.join(f'{v:02x}' for v in rgb))
        if not colors:
            rgb = image.resize((1, 1)).getpixel((0, 0))
            colors = ['#' + ''.join(f'{max(48, v):02x}' for v in rgb)]
        if len(colors) == 1:
            colors *= 2
        return ','.join(colors[:6])


def gui():
    try:
        import gi
        gi.require_version('Gtk', '4.0')
        from gi.repository import Gtk, GLib, Gdk
    except (ImportError, ValueError) as error:
        raise SystemExit('Settings window needs GTK 4 and PyGObject. You can still use --set or edit ' + str(CONFIG)) from error

    # Hyprland tiles new application windows by default. The installed Lua
    # window rule makes this preferences panel floating.
    try:
        app_class = 'io.github.hyprvisualizer.Settings'
        pass
    except (OSError, subprocess.SubprocessError):
        pass

    class App(Gtk.Application):
        def __init__(self):
            super().__init__(application_id='io.github.hyprvisualizer.Settings')

        def do_activate(self):
            window = self.props.active_window
            if window:
                window.present()
                return
            window = Gtk.ApplicationWindow(application=self, title='Visualizer')
            # Scale the panel to the monitor it opens on. Keep comfortable
            # bounds so it remains usable on both laptop and ultrawide screens.
            monitor_width, monitor_height = 1920, 1080
            display = Gdk.Display.get_default()
            if display and display.get_monitors().get_n_items():
                monitor = display.get_monitors().get_item(0)
                geometry = monitor.get_geometry()
                monitor_width, monitor_height = geometry.width, geometry.height
            panel_width = max(480, min(760, int(monitor_width * 0.34)))
            panel_height = max(620, min(900, int(monitor_height * 0.76)))
            try:
                rule = ("hl.window_rule({ name = 'hypr-visualizer-settings-runtime', "
                        "match = { class = '^io.github.hyprvisualizer.Settings$' }, "
                        f"float = true, size = '{panel_width} {panel_height}' }})")
                subprocess.run(['hyprctl', 'eval', rule], stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL, timeout=1)
            except (OSError, subprocess.SubprocessError):
                pass
            window.set_default_size(panel_width, panel_height)
            outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8, margin_top=16, margin_bottom=16, margin_start=18, margin_end=18)
            scroll = Gtk.ScrolledWindow(vexpand=True)
            window.set_child(scroll);scroll.set_child(outer)
            title = Gtk.Label(label='Visualizer', xalign=0)
            title.add_css_class('title-1');outer.append(title)
            subtitle = Gtk.Label(label='Tune the look of your music. Changes apply instantly.', wrap=True, xalign=0)
            subtitle.add_css_class('dim-label');outer.append(subtitle)
            self.widgets = {}
            self.values = read_config()
            for key, options in CHOICES.items():
                row = Gtk.Box(spacing=8)
                row.add_css_class('settings-row')
                row.append(Gtk.Label(label=key.title(), xalign=0, hexpand=True))
                widget = Gtk.DropDown.new_from_strings([v.title() for v in options])
                widget.set_selected(options.index(self.values.get(key, DEFAULTS[key])))
                row.append(widget);outer.append(row);self.widgets[key] = widget
            labels = dict(bar_width='Bar width (logical pixels)', gap='Gap (logical pixels)', height='Maximum height', opacity='Opacity', glow='Bass glow', attack='Rise time (seconds)', release='Fall time (seconds)')
            for key, (lo, hi) in RANGES.items():
                label = Gtk.Label(label=labels[key], xalign=0)
                label.add_css_class('dim-label');outer.append(label)
                widget = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, lo, hi, 1 if key in ('bar_width', 'gap') else 0.01)
                widget.set_digits(1 if key in ('bar_width', 'gap') else 3)
                widget.set_value(float(self.values[key]));outer.append(widget);self.widgets[key] = widget
            colors_label = Gtk.Label(label='Gradient colors', xalign=0)
            colors_label.add_css_class('dim-label');outer.append(colors_label)
            self.colors = Gtk.Entry(text=self.values.get('colors', PRESETS.get(self.values['theme'], CLASSIC_COLORS)))
            outer.append(self.colors)
            wallpaper = Gtk.Button(label='Pick wallpaper colors…');wallpaper.connect('clicked', self.choose_wallpaper);outer.append(wallpaper)
            buttons = Gtk.Box(spacing=8)
            reset = Gtk.Button(label='Restore Classic');reset.connect('clicked', self.restore);buttons.append(reset)
            about = Gtk.Button(label='About');about.connect('clicked', self.show_about);buttons.append(about)
            outer.append(buttons)
            self.status = Gtk.Label(label='Saved automatically', wrap=True, xalign=0)
            self.status.add_css_class('dim-label')
            outer.append(self.status)
            self.updating = False
            self.widgets['theme'].connect('notify::selected', self.theme_changed)
            for key in CHOICES:
                self.widgets[key].connect('notify::selected', self.save_changes)
            for key in RANGES:
                self.widgets[key].connect('value-changed', self.save_changes)
            self.colors.connect('changed', self.save_changes)
            provider = Gtk.CssProvider()
            provider.load_from_data(b'''
                .settings-row { padding: 3px 0; }
                entry { min-height: 30px; }
                button { min-height: 30px; }
            ''')
            Gtk.StyleContext.add_provider_for_display(window.get_display(), provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
            window.present()
            GLib.timeout_add(120, self.compact_window, panel_width, panel_height)

        def compact_window(self, panel_width, panel_height):
            """Float and resize this window after Hyprland maps it."""
            try:
                result = subprocess.run(['hyprctl', 'clients', '-j'], capture_output=True,
                                        text=True, timeout=1, check=True)
                clients = json.loads(result.stdout)
                client = next((item for item in clients if item.get('pid') == os.getpid()), None)
                if not client:
                    return True
                address = client.get('address')
                if not client.get('floating'):
                    subprocess.run(['hyprctl', 'dispatch', 'togglefloating', f'address:{address}'],
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=1)
                    return True
                # Lua-mode Hyprland uses its dispatcher API instead of the
                # legacy `dispatch resizeactive` command.
                subprocess.run(['hyprctl', 'eval',
                                f"hl.dsp.window.resize({{ x = {panel_width}, y = {panel_height}, relative = false }})"],
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=1)
            except (OSError, subprocess.SubprocessError, ValueError, TypeError):
                pass
            return False

        def theme_changed(self, widget, _):
            theme = CHOICES['theme'][widget.get_selected()]
            if theme in PRESETS:
                previous = self.updating
                self.updating = True
                self.colors.set_text(PRESETS[theme])
                self.updating = previous

        def restore(self, _):
            self.updating = True
            for key, options in CHOICES.items():
                self.widgets[key].set_selected(options.index(DEFAULTS[key]))
            for key in RANGES:
                self.widgets[key].set_value(float(DEFAULTS[key]))
            self.colors.set_text(CLASSIC_COLORS)
            self.updating = False
            self.save_changes()

        def save_changes(self, *_):
            if self.updating:
                return
            values = {key: options[self.widgets[key].get_selected()] for key, options in CHOICES.items()}
            values.update({key: format(self.widgets[key].get_value(), '.4g') for key in RANGES})
            values['colors'] = self.colors.get_text().strip()
            try:
                save_config(values)
                self.status.set_text('Saved automatically. Your visualizer is updating.')
            except (ValueError, OSError) as error:
                self.status.set_text(str(error))

        def choose_wallpaper(self, _):
            chooser = Gtk.FileChooserNative.new('Choose wallpaper', self.props.active_window, Gtk.FileChooserAction.OPEN, 'Use colors', 'Cancel')
            images = Gtk.FileFilter();images.set_name('Images');images.add_pixbuf_formats();chooser.add_filter(images)
            def chosen(dialog, response):
                if response == Gtk.ResponseType.ACCEPT:
                    try:
                        colors = wallpaper_colors(dialog.get_file().get_path())
                        self.updating = True
                        self.widgets['theme'].set_selected(CHOICES['theme'].index('wallpaper'))
                        self.colors.set_text(colors)
                        self.updating = False
                        self.save_changes()
                    except (ImportError, ValueError, OSError) as error:
                        self.status.set_text('Cannot read image (Pillow is required): ' + str(error))
                dialog.destroy()
                self.chooser = None
            chooser.connect('response', chosen);self.chooser = chooser;chooser.show()

        def show_about(self, _):
            about = Gtk.AboutDialog(transient_for=self.props.active_window, modal=True)
            about.set_application_name('Hypr Visualizer')
            about.set_title('About Hypr Visualizer')
            about.set_version(APP_VERSION)
            about.set_comments('A native Hyprland audio visualizer with adaptive bars, live themes and DMS lockscreen support.')
            about.set_website('https://github.com/mir4zul/hypr-visualizer')
            about.set_website_label('Open project on GitHub')
            about.set_license_type(Gtk.License.MIT_X11)
            about.set_authors(['Mirajul Islam'])
            about.present()

    App().run([])


def main():
    parser = argparse.ArgumentParser(description='Visualizer settings: run without arguments for the settings window.')
    parser.add_argument('--set', action='append', default=[], metavar='KEY=VALUE')
    parser.add_argument('--classic', action='store_true', help='restore the original look')
    parser.add_argument('--wallpaper', type=Path, help='extract a gradient from an image')
    parser.add_argument('--show', action='store_true')
    args = parser.parse_args()
    try:
        if args.classic or args.set or args.wallpaper:
            values = DEFAULTS.copy() if args.classic else read_config()
            for item in args.set:
                key, value = item.split('=', 1)
                if key == 'theme' and value in PRESETS:
                    values.pop('colors', None)
                values[key] = value
            if args.wallpaper:
                values.update(theme='wallpaper', colors=wallpaper_colors(args.wallpaper))
            save_config(values)
            print('Applied:', CONFIG)
        elif not args.show:
            gui()
        if args.show:
            for key, value in read_config().items():
                print(f'{key}={value}')
    except (ValueError, OSError, ImportError) as error:
        parser.exit(2, f'{error}\n')


if __name__ == '__main__':
    main()
