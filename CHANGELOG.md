# Changelog

## v0.2.0 — Windows C Core + GUI (unreleased)

### Added — C Core (Phase 1)
- **`zzz list`** command: enumerates network adapters (name, description, MAC, IP; TSV format)
- **`zzz run <device> [--config <path>]`** command: run with explicit device and optional config path
- **stdin commands**: `quit` (graceful exit) and `reconnect` (restart auth)
- **ANSI color control**: auto-detects TTY; respects `NO_COLOR` env var; disables colors for GUI consumers
- **Windows signal handling**: `SetConsoleCtrlHandler()` for Ctrl+C / close / shutdown events
- Meson cross-files: `cross/x86_64-windows-gnu.toml`, `cross/aarch64-windows-gnu.toml`
- CI matrix entries for `x86_64-windows-gnu` and `aarch64-windows-gnu`

### Changed
- **`device.c`**: platform abstraction — Windows uses `GetAdaptersAddresses()` + `pcap_findalldevs()` GUID matching for MAC/IP; Linux keeps `ioctl()` path
- **`crypto.c`**: conditional `<winsock2.h>` on Windows, `<arpa/inet.h>` on Linux for `htonl`/`htons`
- **`aes-md5.c`**: `htobe32` → `htonl` on Windows via `<winsock2.h>`
- **`auth.c`**: `u_char` → `const unsigned char` for cross-platform type safety
- **`main.c`**: rewritten with command dispatch, platform-specific signal handling, stdin poll loop
- **`log.c`**: `isatty`/`fileno` detection; `<unistd.h>` on Linux, `<io.h>` with macros on Windows
- **`meson.build`**: Windows fallback links `wpcap.lib` / `ws2_32.lib` / `iphlpapi.lib`
- **`include/packet/packet.h`**: `__attribute__((packed))` → `#pragma pack` macro for MSVC compat
- **`pcap_open_live`** snap length: `BUFSIZ` → `SNAP_LEN 65535` (BUFSIZ=512 on Windows truncates frames)

### Fixed
- **`aes-md5.c`**: `lookup_dict()` default case now returns instead of falling through to `memcpy` with uninitialized pointer (UB/crash)
- **`meson.build`**: address sanitizer flag typo (`-Db_sanitize=address` → `-fsanitize=address`) plus missing linker args
- **`device.c`**: `inet_ntop` buffer too small (`IP_ADDR_SIZE=15`, needs 16); now uses `INET_ADDRSTRLEN` temp buffer
- **CI**: Npcap SDK library path now selects `Lib/ARM64` for aarch64 targets instead of hardcoded `Lib/x64`

### Added — Wails + Vue 3 Desktop App (Phase 2)
- **Wails v2** project (`desktop/`): Go backend + Vue 3 frontend in WebView2
- **Go backend** (`app.go`): bridges to zzz.exe — GetDevices, Connect, Disconnect, config I/O, auto-start registry
- **Vue 3 frontend** (`App.vue`): frameless window with DWM Acrylic backdrop, glassmorphism cards, status dot animation, collapsible log, Tailwind CSS
- **System tray**: minimize to tray on close, tray click restores window

### Added — Inno Setup Installer (Phase 3)
- **Inno Setup script** (`installer/setup.iss`): Npcap pre-install check, admin privileges, Start Menu + Desktop shortcuts, Programs & Features registration, config preservation on uninstall
- **`just package-win`**: builds CLI + GUI + installer in one command, produces `build/zzz-setup.exe` (~5.5 MB)

### Fixed (post-Phase 2 testing)
- **`device.c`**: `FriendlyName` from `GetAdaptersAddresses()` for Chinese adapter display names