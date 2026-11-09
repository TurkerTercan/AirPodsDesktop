# AirPodsDesktop Linux Port Plan

## Status

_Updated 2026-11-09. The phases below are the original plan; this section is the live state._

### Complete

- [x] **Phase 1.1 — CMake platform support.** Windows-only gate split into `WIN32` / Linux
      (`APD_OS_LINUX`), Windows-only flags and libraries guarded, Linux platform sources collected
      automatically, Linux Qt deployment via `qt_generate_deploy_app_script`.
- [x] **Phase 1.2 — Toolchain.** Qt 6.8.3 (aqtinstall), Boost 1.83 headers, BlueZ/D-Bus headers,
      OpenGL dev headers.
- [x] **Phase 2 — Bluetooth.** `Bluetooth_linux.{h,cpp}` implements `Device`, `DeviceManager` and
      `AdvertisementWatcher` on BlueZ via QtDBus (`SetDiscoveryFilter` with `Transport=le` and
      `DuplicateData=true`, `PropertiesChanged`/`InterfacesAdded` parsing, Apple vendor id 76).
      `GlobalMedia_linux` drives MPRIS, `AutoStart_linux` writes an XDG autostart entry.
- [x] **Phase 3 — Tray and GUI.** Tray icon and quick-connect menu; the theme follows the desktop
      colour scheme through the freedesktop portal (`c76c4ff`).
- [x] **Phase 4 — Media control.** MPRIS pause/resume on ear detection. The bounded MPRIS calls and
      the pause/resume bookkeeping are covered by `GlobalMediaTests`, and advertisement merging by
      `AirPodsDomainTests` (`f1b370f`).
- [x] **Phase 6 — Auto-start.** XDG autostart entry; no systemd user service is needed.
- [x] **Quick connect.** BlueZ backend (`Device1.Connect` plus `PropertiesChanged` observation,
      `3fbf715`) replaced the original null backend.
- [x] **Packaging.** Relocatable bundle, DEB and TGZ through CPack (`bb7aa72`); `qt.conf`, `$ORIGIN`
      runpaths, translations and the desktop entry are part of the bundle.
- [x] **CI.** Linux and Windows jobs build, test and package on pushes and pull requests
      (`0df3737`, `5a8aee4`, `1c15088`).
- [x] **Build and startup verified.** `Build-linux/Binary/AirPodsDesktop` links and runs headlessly;
      startup log is clean apart from the updater (no network, no Linux release assets).
- [x] **Intermittent startup segfault fixed.** `ApdApplication` took `argc` by value, so the reference
      `QCoreApplicationPrivate` keeps to it dangled once the constructor returned. Qt's XCB
      integration later called `QCoreApplication::arguments()` (via `wmClass()`), walked a garbage
      argument count, and crashed. `argc` is now taken by reference and binds to `main()`'s copy.
- [x] **Translations.** Sources refreshed and Turkish completed (`30035a9`).

### Remaining

- [ ] **Hardware verification.** Pairing, binding, battery display and quick connect were exercised
      with AirPods Pro 3 on 2026-11-09. Three findings are still open:
      - Ear detection and MPRIS pause/resume work when a proximity packet arrives, but the tested
        BARROT USB dongle delivers them only every 10–90 seconds, so a bud leaving or entering the
        ear can take up to a minute to register. The built-in Intel adapter was reported prompt
        before the dongle was installed; that has not been re-verified.
      - The case battery is never displayed. Docked pods drop the classic link, and the current code
        stops the advertisement watcher and rejects advertisements while the link is down — exactly
        when the case broadcasts. Log capture confirms docked-pod advertisements (both pods in case,
        case battery present) reach the host only if the watcher keeps running.
      - Pause/resume uses the MPRIS `PlayPause` toggle. The AirPods pause the player themselves when
        a bud is removed, so the toggle can cancel that pause and keep the music playing. Explicit
        `Play`/`Pause` calls are the intended fix.
- [ ] **Linux release assets.** No Linux release is published, so the updater remains a no-op on
      Linux.
- [ ] **GNOME tray.** GNOME needs the AppIndicator extension for the tray icon to appear.
- [ ] **Wayland.** The test suite runs under Xvfb; a Wayland session is not verified.
- [ ] **Low-latency mode and codecs.** The silent-stream implementation runs on Linux, but its
      effect and aptX/LDAC selection through PipeWire are unmeasured.
- [ ] **Taskbar battery dock.** Windows-only by design; Linux keeps the no-op `TaskbarStatus`.

## Executive Summary
AirPodsDesktop uses Qt with platform-specific subsystems for Bluetooth, media control, auto-start, and audio management. The codebase has well-structured abstract interfaces, making it suitable for a Linux port. Estimated effort: **2-3 months** for a complete port with full feature parity, or **3-4 weeks** for a minimal viable port (Bluetooth + battery display only).

---

## Phase 1: Build System (Week 1)

### 1.1 Update CMakeLists.txt
- **Remove**: Hard Windows-only checks (lines 51-62)
- **Add**: Platform detection and conditional compilation
- **Add**: Linux-specific find_package calls for BlueZ, D-Bus, GLib, ALSA/PipeWire

```cmake
# Platform detection
if(WIN32)
    set(APD_PLATFORM "windows")
elseif(UNIX AND NOT APPLE)
    set(APD_PLATFORM "linux")
endif()

# Conditional dependencies
if(APD_PLATFORM STREQUAL "linux")
    find_package(Bluez REQUIRED)
    find_package(DBus REQUIRED)
    find_package(Glib REQUIRED)
    find_package(ALSA REQUIRED)
endif()
```

### 1.2 Platform-Specific Source Files
```cmake
# Windows sources
if(APD_PLATFORM STREQUAL "windows")
    target_sources(apd_core PRIVATE
        Source/Core/Bluetooth_win.cpp
        Source/Core/AutoStart_win.cpp
        Source/Core/GlobalMedia_win.cpp
        Source/Core/QuickConnect_win.cpp
    )
endif()

# Linux sources (new files to create)
if(APD_PLATFORM STREQUAL "linux")
    target_sources(apd_core PRIVATE
        Source/Core/Bluetooth_linux.cpp
        Source/Core/AutoStart_linux.cpp
        Source/Core/GlobalMedia_linux.cpp
        Source/Core/QuickConnect_linux.cpp
    )
endif()
```

### 1.3 Linux Dependencies
```bash
# Ubuntu 24.04
sudo apt install \
    libbluetooth-dev \
    libdbus-1-dev \
    libglib2.0-dev \
    libasound2-dev \
    libpipewire-0.3-dev \
    libpulse-dev \
    libx11-dev \
    libwayland-dev
```

---

## Phase 2: Bluetooth Subsystem (Weeks 2-3)

### 2.1 Replace WinRT Bluetooth with BlueZ D-Bus API

**Current**: `Bluetooth_win.cpp` uses WinRT `Windows.Devices.Bluetooth.Advertisement` for BLE scanning.

**Linux Implementation**: Use BlueZ's D-Bus API via `gdbus` or `sd-bus`.

```cpp
// Example: Start BLE advertising scan via D-Bus
g_dbus_connection_emit_signal(conn, NULL, "/org/bluez/hci0",
    "org.bluez.LEAdvertisingManager1", "RegisterAdvertisingApplication",
    parameters, NULL);
```

### 2.2 Parse Apple AirPods BLE Advertisements
Apple uses manufacturer data with company ID 0x004C (76 decimal). The payload format is well-documented.

**Key Limitation:** BlueZ D-Bus API does NOT support filtering by manufacturer data at the D-Bus level. You must filter in application code by inspecting each device's `ManufacturerData` property.

**Advertisement Format (Paired Mode, 27 bytes total):**
```
Byte 0-2:  0x07 0x19 0x01 (protocol header)
Byte 3-4:  Model id, little-endian (e.g., 0x2014 = AirPods Pro 2, 0x2027 = AirPods Pro 3
           as the bytes 27 20)
Byte 5:    Status bits; bit1 = broadcasting bud in ear, bit2 = both pods in case,
           bit3 = other bud in ear, bit5 = 1 when the left bud broadcasts
Byte 6:    Broadcasting bud battery (low nibble) + other bud battery (high nibble)
Byte 7:    Case battery (low nibble), then charging flags in the high nibble:
           bit4 = broadcasting bud, bit5 = other bud, bit6 = case
Byte 8:    Lid: bit3 = closed, bits 0-2 = lid switch count
Byte 9-26: Per-packet hash/encrypted payload (identifying; do not log)
```

`Source/Core/AppleCP.h` is the source of truth; the C below mirrors its layout.

**Battery values use 10% steps (0-10), not exact percentages. Values above 10 (e.g. 0xF) =
unavailable.**

```c
// C example: Parse AirPods battery from manufacturer data
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int model;
    int case_battery;      // -1 = unavailable
    int left_battery;      // -1 = unavailable
    int right_battery;     // -1 = unavailable
    bool case_charging;
    bool lid_open;
} AirPodsBattery;

static int percent(int nibble) {
    if (nibble > 10) return -1;  // 0xF = unavailable
    return nibble * 10;
}

bool parse_airpods_battery(const uint8_t *data, int len, AirPodsBattery *out) {
    if (len < 27 || data[0] != 0x07 || data[1] != 0x19 || data[2] != 0x01) {
        return false;  // Not AirPods format
    }

    const bool from_left = (data[5] & 0x20) != 0;
    const uint8_t curr = data[6] & 0x0F;        // the broadcasting bud
    const uint8_t anot = (data[6] >> 4) & 0x0F; // the other bud

    out->model = data[3] | (data[4] << 8);
    out->case_battery = percent(data[7] & 0x0F);
    out->left_battery = percent(from_left ? curr : anot);
    out->right_battery = percent(from_left ? anot : curr);
    out->case_charging = (data[7] & 0x40) != 0;
    out->lid_open = (data[8] & 0x08) == 0;

    return true;
}
```

### 2.3 Implement Device Manager Interface
Implement `DeviceManagerAbstract` interface:
- Device discovery via BlueZ
- Device connection state monitoring
- Battery level reporting

---

## Phase 3: System Tray and GUI (Week 4)

### 3.1 System Tray Integration
Qt's `QSystemTrayIcon` works on Linux via DBus StatusNotifierItem.

```cpp
// Check if tray is available
if (QSystemTrayIcon::isSystemTrayAvailable()) {
    trayIcon->show();
} else {
    // Fallback: keep window visible
    showWindow();
}
```

### 3.2 GNOME Desktop Integration
For GNOME, use AppIndicator/KStatusNotifierItem extension:
- Check if `org.gnome.Shell.Extensions.AppIndicator` is installed
- Provide fallback behavior if not

### 3.3 Wayland Support
Qt handles Wayland natively. Ensure proper environment variables:
- `WAYLAND_DISPLAY`
- `XDG_CURRENT_DESKTOP`

---

## Phase 4: Media Control (Weeks 5-6)

### 4.1 MPRIS D-Bus Integration
Linux uses MPRIS (Media Player Remote Interfacing Specification) for media control.

```cpp
// Query active MPRIS players
QDBusInterface player("org.mpris.MediaPlayer2.playerctl",
    "/org/mpris/MediaPlayer2",
    "org.mpris.MediaPlayer2.Player");

// Control playback. Use the explicit methods, not "PlayPause": the AirPods pause the player
// themselves when a bud is removed, and a toggle would turn the music back on.
player.call("Pause");
```

### 4.2 Media Key Handling
- Use libmpd or libpulse for audio session detection
- Handle media key events via X11 or Wayland protocols

---

## Phase 5: Audio Latency Optimization (Week 7)

### 5.1 PipeWire/ALSA Integration
For low-latency audio, use ALSA or PipeWire directly:

```cpp
// ALSA low-latency setup
snd_pcm_t *handle;
snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
snd_pcm_set_params(handle, ...);  // Configure for low latency
```

### 5.2 Bluetooth Codec Support
- Verify A2DP codec support on Linux
- Test with aptX, LDAC, SBC codecs
- Ensure low-latency codec selection works

---

## Phase 6: Auto-Start and System Integration (Week 8)

### 6.1 XDG Autostart
Write autostart `.desktop` file:

```cpp
// Create autostart entry
QDir dir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation));
dir.mkpath("autostart");
QFile file(dir.filePath("airpods-desktop.desktop"));
file.open(QIODevice::WriteOnly);
file.write("[Desktop Entry]\nType=Application\nExec=airpods-desktop\n");
```

### 6.2 Systemd User Service
Optional: Provide systemd user service for background operation.

---

## Phase 7: Testing and Validation (Week 9+)

### 7.1 Test Matrix
- **OS**: Ubuntu 24.04 LTS (GNOME default)
- **Bluetooth**: Test with various Bluetooth adapters
- **Audio**: Test with PulseAudio, PipeWire, ALSA
- **Wayland/X11**: Test both display servers

### 7.2 Feature Parity Checklist
- [x] AirPods battery level display (buds yes; case battery blocked by the docked-pod finding in
      [Status](#remaining))
- [x] Connection status indicator
- [x] Media playback control (ear detection through MPRIS)
- [x] Quick connect/disconnect (BlueZ backend)
- [x] Low-latency audio mode (silent-stream implementation; effect on Linux unmeasured)
- [x] Auto-start on boot (XDG autostart)
- [x] Tray icon with menu (GNOME needs the AppIndicator extension)
- [x] Settings persistence

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| BlueZ API changes | Low | High | Pin BlueZ version, test on multiple distros |
| MPRIS compatibility | Low | Medium | Test with multiple media players |
| Wayland limitations | Medium | Medium | Provide X11 fallback |
| Bluetooth codec issues | Medium | High | Thorough codec testing |
| GNOME integration | Low | Low | Document workarounds |

---

## Dependencies Summary

| Dependency | Purpose | Linux Equivalent |
|------------|---------|------------------|
| WinRT Bluetooth | BLE scanning | BlueZ D-Bus API |
| Windows Registry | Auto-start | XDG autostart |
| Windows MMDevice | Audio control | PulseAudio/PipeWire |
| Win32 API | Window management | X11/Wayland |
| Windows Media Control | Media control | MPRIS D-Bus |
| KSPROPERTY | Bluetooth audio | BlueZ + ALSA |

---

## Success Criteria

1. **Builds on Ubuntu 24.04** without Windows dependencies
2. **Detects AirPods** via BLE scanning
3. **Displays battery level** in system tray
4. **Controls media playback** on the active player
5. **Supports both X11 and Wayland** display servers
6. **Passes comprehensive testing** with various Bluetooth adapters and audio configurations

---

## Estimated Timeline

| Phase | Duration | Cumulative |
|-------|----------|------------|
| Build System | 1 week | 1 week |
| Bluetooth Subsystem | 2 weeks | 3 weeks |
| System Tray & GUI | 1 week | 4 weeks |
| Media Control | 2 weeks | 6 weeks |
| Audio Latency | 1 week | 7 weeks |
| System Integration | 1 week | 8 weeks |
| Testing & Validation | 1+ week | 9+ weeks |

**Total: 9-12 weeks for production-ready port**