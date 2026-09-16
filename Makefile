CC ?= cc
PKGS = wayland-client cairo libpulse
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
CPPFLAGS += $(shell pkg-config --cflags $(PKGS)) -Ibuild
LDLIBS += $(shell pkg-config --libs $(PKGS)) -lm -pthread
PREFIX ?= $(HOME)/.local
WAYLAND_PROTOCOLS_DIR ?= $(shell pkg-config --variable=pkgdatadir wayland-protocols)

all: build/hypr-visualizer
build:
	mkdir -p build
build/layer-shell.h: protocol/wlr-layer-shell-unstable-v1.xml | build
	wayland-scanner client-header $< $@
build/layer-shell.c: protocol/wlr-layer-shell-unstable-v1.xml | build
	wayland-scanner private-code $< $@
build/xdg-shell.c: $(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml | build
	wayland-scanner private-code $< $@
build/hypr-visualizer: main.c theme.h config.h version.h build/layer-shell.h build/layer-shell.c build/xdg-shell.c
	$(CC) $(CPPFLAGS) $(CFLAGS) main.c build/layer-shell.c build/xdg-shell.c -o $@ $(LDLIBS)
install: all
	install -Dm755 build/hypr-visualizer "$(DESTDIR)$(PREFIX)/bin/hypr-visualizer"
	install -Dm755 settings.py "$(DESTDIR)$(PREFIX)/bin/hypr-visualizer-settings"
clean:
	rm -rf build
.PHONY: all install clean

build/test-audio: tests/audio.c main.c theme.h config.h build/layer-shell.h build/layer-shell.c build/xdg-shell.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/audio.c build/layer-shell.c build/xdg-shell.c -o $@ $(LDLIBS)
test: build/test-audio
	./build/test-audio
.PHONY: test

build/test-settings: tests/settings.c main.c theme.h config.h build/layer-shell.h build/layer-shell.c build/xdg-shell.c
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/settings.c build/layer-shell.c build/xdg-shell.c -o $@ $(LDLIBS)
test: test-settings
test-settings: build/test-settings
	./build/test-settings
.PHONY: test-settings
