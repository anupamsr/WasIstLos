# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

WasIstLos is an unofficial WhatsApp desktop application for Linux written in C++ using GTK4 (gtkmm-4.0) and WebKitGTK 6.0. It wraps WhatsApp Web in a native GTK application with features like system tray integration, zoom controls, and localization support.

## Build System

This project uses CMake (minimum version 3.16) with C++20 standard. The build system is structured across multiple CMakeLists.txt files:
- Root `CMakeLists.txt` defines project metadata and version (currently 1.8.0)
- `src/CMakeLists.txt` manages source compilation, library linking, and generates `Config.hpp` from `Config.hpp.in`
- `po/CMakeLists.txt` handles translation compilation via gettext
- `resource/CMakeLists.txt` bundles UI files and icons using GResource

### Build Dependencies

#### Debian 13 (Trixie) / Ubuntu 24.10+

```bash
sudo apt install build-essential cmake intltool pkgconf \
    libgtkmm-4.0-dev libwebkitgtk-6.0-dev
```

#### Runtime Dependencies

The application requires:
- GTK4 (libgtkmm-4.0) - includes GIO D-Bus support for system tray
- WebKitGTK 6.0 (libwebkitgtk-6.0)
- GStreamer plugins (for media playback in WhatsApp Web)

### Development Build Commands

#### Native Linux Build

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

#### Docker Build

The Dockerfile uses `debian:trixie` as the base and includes all GTK4 dependencies. Docker is primarily useful for building on macOS or for CI/CD:

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

Note: Running the GUI application from Docker requires X11 forwarding setup. For macOS, native Linux build is not possible - use Docker for compilation only.

### Local Installation

```bash
# From build directory (requires admin privileges)
make install

# Uninstall
xargs rm < install_manifest.txt
```

### Packaging

```bash
# Debian package
dpkg-buildpackage -uc -us -ui

# Snap (pass --use-lxd in virtual environments)
snapcraft

# AppImage (install to AppDir first)
make install DESTDIR=../../AppDir
appimage-builder --skip-test --recipe ./appimage/AppImageBuilder.yml
```

## Code Formatting and Linting

**Always format code before committing.** The project uses clang-format version 15 with Allman brace style, 4-space indentation, and 160 character line limit.

```bash
# Format all C++ files in src/
clang-format -i src/**/*.{hpp,cpp}

# Check formatting (CI runs this)
clang-format --dry-run --Werror src/**/*.{hpp,cpp}
```

Clang-tidy is configured in `.clang-tidy` with bugprone, cert, misc, modernize, performance, and readability checks. Many specific checks are disabled - refer to `.clang-tidy` for the exact configuration.

## Architecture

### Namespace Structure

All code lives under the `wil` namespace with two main sub-namespaces:
- `wil::ui` - UI components (Application, MainWindow, WebView, TrayIcon, dialogs)
- `wil::util` - Utilities (Settings, Helper)

### Application Flow

1. `main.cpp` - Entry point that:
   - Sets up locale and gettext for internationalization
   - Creates singleton `wil::ui::Application` instance
   - Redirects output to logger via `wil::util::redirectOutputToLogger()`
   - Registers signal handlers (SIGINT, SIGTERM, SIGPIPE)
   - Calls `app.run()` to start GTK main loop

2. `ui/Application` - GTK Application wrapper (singleton):
   - Manages `MainWindow` lifecycle
   - Handles startup, activation, and file opening events
   - Implements keep-alive mechanism for background operation

3. `ui/MainWindow` - Main application window:
   - Contains `WebView` and `TrayIcon` instances
   - Manages preferences and phone number dialogs
   - Uses GTK4 event controllers (EventControllerKey, EventControllerScroll)
   - Handles keyboard shortcuts and window state events
   - Implements zoom, fullscreen, and headerbar toggle

4. `ui/WebView` - Core WhatsApp Web integration:
   - Wraps WebKitGTK 6.0 WebView widget
   - Hardcoded to load `https://web.whatsapp.com`
   - Uses custom User-Agent string for Chrome 120 on Linux
   - Manages zoom level, font size, custom CSS injection
   - Emits signals for load status changes and notifications
   - Implements phone number opening via WhatsApp's `wa.me` URLs

### UI Construction

UI layouts are defined in GtkBuilder XML files under `resource/ui/`:
- `MainWindow.ui` - Main window layout with headerbar and menu
- `PreferencesWindow.ui` - Settings dialog
- `PhoneNumberDialog.ui` - Phone number input dialog
- `ShortcutsWindow.ui` - Keyboard shortcuts help

These are compiled into the binary via GResource (`resource/gresource.xml`). UI components load their layout in constructors using `Gtk::Builder::create_from_resource()`.

### Settings Management

`util/Settings` is a singleton that manages user preferences stored in `~/.config/wasistlos/settings.ini`. Uses a custom `SettingMap` for type-safe key-value storage with template methods `getValue<T>()` and `setValue<T>()`. Special handling for autostart setting which writes to XDG autostart directory.

### System Tray Integration

`ui/TrayIcon` implements system tray functionality via **direct D-Bus StatusNotifierItem protocol**:
- Pure GTK4 implementation using `Gio::DBus::Connection` (no external tray libraries)
- Registers on `org.kde.StatusNotifierItem` D-Bus interface
- Uses `Gio::Menu` and `Gio::SimpleActionGroup` for the popup menu
- Menu and actions exported over D-Bus via `export_menu_model()` and `export_action_group()`
- Native protocol for KDE Plasma, XFCE, and other freedesktop.org-compliant panels
- Three states: Passive (hidden), Active (idle), NeedsAttention (alerts)
- Dynamic attention icons: switches between message icon (`whatsapp-msg`) and warning icon (`whatsapp-warning`)
- Icon lookup uses `gtk_icon_theme_has_icon()` with fallback chains for theme compatibility
- Emits D-Bus signals (`NewStatus`, `NewIcon`, `NewAttentionIcon`) for state changes

### Localization

- Translation strings use gettext macros: `_("string")` for translatable text
- Source strings extracted from C++ files and Glade UI files (see `po/POTFILES.in`)
- Template file: `po/wasistlos.pot`
- Supported languages: bn, cs, de, es, fr, hu, it, ka, nl, pl, pt_BR, ru, si, tr, uk, zh_Hans
- Translations managed via POEditor: https://poeditor.com/join/project/jMGkxVn3vN
- Use `make update-template-translation` to regenerate POT file after adding new translatable strings
- Language detection happens in `WebView` via `std::locale{""}.name()`

## Dependencies

Required libraries (checked via pkg-config):
- gtkmm-4.0 - GTK4 C++ bindings (includes giomm for D-Bus support)
- webkitgtk-6.0 - GTK4-native web rendering engine

Note: System tray uses D-Bus StatusNotifierItem protocol via GTK4's built-in GIO D-Bus bindings - no external tray libraries needed. Notification sounds are handled by WhatsApp Web itself via WebKit's audio stack, not by the application. Spell checking is handled internally by WebKitGTK.

## Key Files

- `src/Config.hpp.in` - Template for build-time configuration macros (app name, version, paths)
- `.clang-format` - Code style configuration (Allman braces, 160 char limit, 4-space indent)
- `.clang-tidy` - Static analysis rules
- `resource/gresource.xml` - Defines UI files and images bundled into binary

## Testing and CI

The project has GitHub Actions workflows in `.github/workflows/`:
- `linter.yml` - Runs clang-format check on push/PR
- `build.yml` - Compiles the project
- `install.yml` - Tests installation
- `release.yml` - Handles releases

No unit tests are present - the project relies on manual testing and CI build validation.
