# WasIstLos

An unofficial WhatsApp desktop application for Linux.

[![Action Status](https://github.com/anupamsr/WasIstLos/workflows/Linter/badge.svg)](https://github.com/anupamsr/WasIstLos/actions/workflows/linter.yml)
[![Action Status](https://github.com/anupamsr/WasIstLos/workflows/Build/badge.svg)](https://github.com/anupamsr/WasIstLos/actions/workflows/build.yml)
[![Action Status](https://github.com/anupamsr/WasIstLos/workflows/Install/badge.svg)](https://github.com/anupamsr/WasIstLos/actions/workflows/install.yml)
[![Action Status](https://github.com/anupamsr/WasIstLos/workflows/Release/badge.svg)](https://github.com/anupamsr/WasIstLos/actions/workflows/release.yml)

![App Window](screenshot/app.png)


## About

WasIstLos is an unofficial WhatsApp desktop application written in C++ using GTK4 (gtkmm-4.0) and WebKitGTK 6.0. This fork features a complete GTK4 migration with native D-Bus system tray integration.


## Features

* All WhatsApp Web features
  * WhatsApp specific keyboard shortcuts work with *Alt* key instead of *Cmd*
* GTK4 native interface
* System tray integration via D-Bus StatusNotifierItem protocol
  * Dynamic attention icons for messages and connectivity warnings
  * Close to tray / start minimized to tray
* Zoom in/out controls
* Notification support with desktop integration
* Autostart with system
* Fullscreen mode
* Show/Hide headerbar by pressing *Alt+H*
* Localization support in 15+ languages
* Spell checking in system language (requires corresponding hunspell dictionary, e.g., `hunspell-en_us`)


## Dependencies

### Build Dependencies (Debian 13 Trixie / Ubuntu 24.10+)

```bash
sudo apt install build-essential cmake intltool pkgconf \
    libgtkmm-4.0-dev libwebkitgtk-6.0-dev
```

### Runtime Dependencies

* GTK4 (libgtkmm-4.0-0)
* WebKitGTK 6.0 (libwebkitgtk-6.0-4)
* GStreamer plugins (for media playback: gstreamer1.0-libav, gstreamer1.0-plugins-base, gstreamer1.0-plugins-good, gstreamer1.0-plugins-bad)
* hunspell dictionaries (optional, for spell checking)


## Build & Run

### Native Linux Build

```bash
# Create debug build directory
mkdir -p build/debug && cd build/debug

# Configure and build
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=/usr ../..
make -j4

# Run the application
./wasistlos

# Update translation template (when modifying translatable strings)
make update-template-translation
```

### Docker Build

For building on non-Linux systems or in isolated environments:

```bash
# Build the Docker image
docker build -t wasistlos:latest .

# Or use docker-compose
docker-compose build

# Extract the binary from the container
docker create --name temp-wasistlos wasistlos:latest
docker cp temp-wasistlos:/app/build/release/src/wasistlos ./wasistlos
docker rm temp-wasistlos
```

Note: The GUI requires X11 forwarding to run from Docker. Use Docker primarily for compilation.

### Local Installation

```bash
# From build directory (requires admin privileges)
make install

# Uninstall
xargs rm < install_manifest.txt
```

## Packaging

### Debian Package

```bash
# Update version in debian/changelog, then build
dpkg-buildpackage -uc -us -ui
```

### Snap Package

```bash
# Pass --use-lxd in virtual environments
snapcraft
```

### AppImage

```bash
# Install to AppDir first
make install DESTDIR=../../AppDir

# Build AppImage
appimage-builder --skip-test --recipe ./appimage/AppImageBuilder.yml
```


## Contributing

Please read [contributing](CONTRIBUTING.md).

## Credits

Originally forked from [xeco23/WasIstLos](https://github.com/xeco23/WasIstLos). This fork focuses on GTK4 migration and modern Linux desktop integration.
