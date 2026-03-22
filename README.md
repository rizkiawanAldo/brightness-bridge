# ☀ Brightness Bridge

A tiny (~59KB) background service that lets your laptop's **brightness keys** (Fn+☀) control **external monitor brightness** via [ClickMonitorDDC](https://clickmonitorddc.com/).

## The Problem

On laptops with external monitors, the **Fn + Brightness keys** only adjust the built-in display. External monitors use a different protocol (**DDC/CI** over the display cable) that the OS brightness keys don't talk to. [ClickMonitorDDC](https://clickmonitorddc.com/) can control external monitors via DDC/CI, but it can't listen for brightness keys — they're special **HID Consumer Control** events (not regular keyboard keys), so ClickMonitorDDC's built-in hotkey scanner can't detect them.

**Brightness Bridge** solves this by listening for those low-level HID brightness events and forwarding them to ClickMonitorDDC, making your brightness keys work for external monitors too.

## How It Works

```
┌──────────────────┐     CreateProcess      ┌─────────────────────┐
│ brightness_bridge │ ── "b +10" / "b -10" ──▶ │  ClickMonitorDDC    │
│   (.exe, ~59KB)   │                       │  (adjusts DDC/CI)   │
│                   │                       │                     │
│ Listens: Raw HID  │                       │  → Monitor brightness│
│ 0x006F / 0x0070   │                       │    changes           │
└──────────────────┘                        └─────────────────────┘
        ▲
   Fn + Brightness keys
```

## Setup

### 1. Place in the same folder as ClickMonitorDDC

Copy `brightness_bridge.exe` into the same folder as your `ClickMonitorDDC*.exe`
(e.g. `C:\myScript\`). It auto-detects any `ClickMonitorDDC*.exe` in the same
directory, so it works regardless of the version number.

### 2. Run

Double-click `brightness_bridge.exe`. A small icon appears in the system tray.

- **Right-click** the tray icon → **Exit** to stop
- Uses ~1-2 MB of RAM
- No ClickMonitorDDC hotkey setup needed — it calls the exe directly

### 3. Start on Boot (optional)

`Win+R` → type `shell:startup` → place a shortcut to `brightness_bridge.exe` there.

## Configuration

Edit these values at the top of `brightness_bridge.c` and recompile:

```c
#define CLICKMONITOR_EXE  L"ClickMonitorDDC_7_2.exe"  // exe name
#define BRIGHTNESS_STEP   10                           // % per press
```

## Building from Source

Requires [w64devkit](https://github.com/skeeto/w64devkit) or any MinGW gcc.

```
build.bat
```

## Files

| File | Description |
|---|---|
| `brightness_bridge.exe` | Compiled program (~59KB, no dependencies) |
| `brightness_bridge.c` | Source code (~200 lines) |
| `build.bat` | Build script |
| `catch_brightness.py` | Diagnostic tool — shows all keyboard/HID events (optional) |
