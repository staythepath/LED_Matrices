# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 firmware for controlling WS2812B LED matrix panels (up to 8x 16x16 panels). Built with PlatformIO + Arduino framework. Features a web UI, telnet CLI, and physical OLED+rotary encoder interface.

## Build Commands

```bash
# Build firmware
pio run

# Upload firmware via USB (COM14)
pio run --target upload

# Upload SPIFFS filesystem (data/ directory)
pio run --target uploadfs

# Monitor serial output (115200 baud)
pio device monitor

# Build + upload + monitor
pio run --target upload && pio device monitor

# Clean build
pio run --target clean
```

The device is also reachable at `esp32-led.local` via mDNS. OTA firmware update is available through the web UI at `/update.html`.

## Architecture

### Core Loop

`main.cpp` initializes all managers and runs the main loop. The system runs on ESP32 dual cores: Core 0 handles WiFi/system tasks, Core 1 runs the main loop (LED updates, web server, telnet).

### Manager Pattern

The codebase uses a manager pattern where each subsystem is a singleton-style class:

- **LEDManager** (`src/LEDManager.cpp/h`) — Central hub. Owns the animation lifecycle, palette system, brightness, and all animation parameters. All state mutations are protected by a FreeRTOS recursive mutex (`_stateMutex`). Use `LEDMANAGER_LOCK_OR_RETURN` macro or `LockGuard` for thread-safe access.
- **WebServerManager** (`src/WebServerManager.cpp/h`) — ESPAsyncWebServer on port 80. Serves SPIFFS static files and exposes `/api/*` REST endpoints. All API routes use GET with query parameters (e.g., `/api/brightness?val=128`).
- **TelnetManager** (`src/TelnetManager.cpp/h`) — CLI on port 23 for remote debugging and control.
- **WiFiManager** (`src/WiFiManager.cpp/h`) — WiFi connection + ArduinoOTA setup. Network config persisted to `/net.cfg` on SPIFFS.
- **LogManager** (`src/LogManager.cpp/h`) — Thread-safe logging with SPIFFS persistence.
- **Menu/RotaryEncoder/LCDManager** — Physical OLED (U8g2 SSD1309) + rotary encoder interface.

### Animation System

All animations inherit from `BaseAnimation` (`src/animations/BaseAnimation.h`) which requires `begin()` and `update()` pure virtual methods. Animations write directly to the global `CRGB leds[MAX_LEDS]` array; `FastLED.show()` is called by LEDManager after each update.

Current animations: Blink, Traffic (particle vehicles), RainbowWave, Firework, GameOfLife, LangtonsAnt, SierpinskiCarpet.

### Adding a New Animation

1. Create `src/animations/MyAnimation.h` and `.cpp` inheriting from `BaseAnimation`
2. Implement `begin()` (init/reset) and `update()` (per-frame logic)
3. In `LEDManager.cpp`:
   - Add `#include "animations/MyAnimation.h"`
   - Add name to `_animationNames` vector in constructor
   - Add `case N:` in `setAnimation()` switch to instantiate it
   - Add parameter setters that `dynamic_cast` to `MyAnimation*`
   - Wire parameters in `configureCurrentAnimation()`
4. In `WebServerManager.cpp`: add `/api/` routes for any new parameters
5. In `data/control.html` and `data/script.js`: add UI controls with `data-anim="N"` attribute for conditional visibility

### Frontend (data/ directory)

Served from SPIFFS. Key files:
- `control.html` — Main animation control panel
- `script.js` — API request queue (serialized with 250ms delay to avoid flooding ESP32), slider debouncing (200ms), retry with exponential backoff
- `css/styles.css` — Dark theme (cyan accent `#38bdf8`)

Animation-specific controls use `data-anim` attributes to show/hide based on the selected animation index.

## Hardware Configuration (src/config.h)

- LED data pin: GPIO 4
- 768 LEDs default (3 panels x 256), MAX_LEDS = 2048 (8 panels)
- WS2812B, GRB color order
- Default brightness: 32/255

## Key Dependencies

| Library | Purpose |
|---------|---------|
| FastLED 3.5.0 | WS2812B LED control and color math |
| ESPAsyncWebServer 3.4.5 | Non-blocking HTTP server with regex routing |
| U8g2 2.36.2 | OLED display rendering |
| Ai Esp32 Rotary Encoder 1.7 | Hardware encoder input |

## Threading Model

The mutex in LEDManager is critical. Any code path that touches LED state (animation parameters, palette, brightness) from the web server or telnet handlers **must** acquire the lock first. The web server runs on async callbacks (potentially different task context), so all API handlers go through LEDManager's mutex-protected setters.
