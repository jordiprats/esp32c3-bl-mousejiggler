# ESP32 BLE Mouse Jiggler

A mouse jiggler firmware for ESP32 boards (ESP32-C3, ESP32-S3, ESP32, etc.). It emulates a Bluetooth HID mouse that periodically moves the cursor to prevent the computer from entering sleep / screensaver mode.  
Includes **battery simulation**, **randomised timing**, **idle pauses**, and **deep sleep** to look and behave like a real wireless mouse.

## How it works

The device advertises as a BLE mouse with a configurable name (default: **ESP32 Mouse**).  
Optionally you can impersonate a vendor product locally via an `config.local.h` (see [BLE identity](#ble-identity)).

- **Connected:** moves the mouse in a repeating super-cycle of patterns (**box → diagonal → box → other diagonal → box**, 16 moves total, zero net drift). Each movement is spaced **~10 s apart +/-10 s** (0–20 s delay). The alternating diagonals break the statistical regularity of a pure square pattern.  
  After **each individual move** there's a **~10 % chance to pause for 10–100 s** – the pause can happen at any point in the pattern.

- **Battery simulation:** on first boot it picks a random battery level between **60–100 %**, then drops **1 % (or 2 % if the chip is heating up) on a non-linear discharge curve** (floor at **3 %**). The level is reported over BLE so the host OS can read it.

  | Level      | Drain rate       |
  |------------|------------------|
  | 100–81 %   | 1 % / 6 h       |
  | 80–31 %    | 1 % / 4 h       |
  | 30–11 %    | 1 % / 2 h       |
  | 10–3 %     | 1 % / 1 h       |

  The chip's internal temperature sensor is read each cycle. If the chip is hotter than the last drain, the battery drops an extra 1 % (simulating heat stress on cells).  
  Random **-1 % fluctuations** occur once per drain step (same interval) – they never increase the level.

  Special behavior at **100 %**: the first drop to 99 % happens after a **random 10–20 minutes** (simulating a full battery that briefly stays at 100 %). After that, the normal drain schedule takes over.

- **Deep sleep persistence:** battery level survives deep sleep, if the saved level is **below 15 %** on boot, a fresh random value is chosen instead.
- **Disconnected:** after **5 minutes** without a connection the device enters **deep sleep for 30 seconds**, then wakes up and advertises again. This saves power when not in use.

## Hardware

- **Board:** Any ESP32 with BLE (ESP32-C3, ESP32-S3, ESP32, etc.)
- **Framework:** Arduino (via PlatformIO)

## Project structure

```
├── lib/mousejiggler/
│   ├── config.h                Tunable constants (battery, timing, deep sleep, CPU)
│   ├── battery.h/.cpp          Battery simulation (drain, fluctuations, temp)
│   └── timing.h/.cpp           Movement pattern generation & delays
├── src/
│   ├── config.local.h          Local overrides (gitignored)
│   ├── config.local.h.example  Template for BLE identity override
│   └── main.cpp                BLE integration & setup
├── test/
│   └── test_all.cpp            15 Unity tests
├── .github/workflows/
│   └── test.yml                CI: runs tests on push/PR
├── platformio.ini
└── README.md
```

The battery and timing modules are **hardware-independent** – they use a random callback so they can be unit-tested on your development machine.

## Build & Flash

This project uses [PlatformIO](https://platformio.org/). Three environments are predefined in `platformio.ini`:

### For ESP32-C3 (e.g., ESP32-C3 Zero, ESP32-C3-DevKitM-1)

```bash
pio run --environment esp32-c3-zero
pio run --target upload --environment esp32-c3-zero
```

### For ESP32-S3 (e.g., ESP32-S3-DevKitC-1)

```bash
pio run --environment esp32-s3-devkit
pio run --target upload --environment esp32-s3-devkit
```

### For original ESP32 (e.g., ESP32 Dev Module)

```bash
pio run --environment esp32dev
pio run --target upload --environment esp32dev
```

### For other ESP32 boards

Add a new `[env:your-board]` section to `platformio.ini` – just copy one of the existing environments and change the `board` line.  
Most ESP32 variants with BLE will work without code changes.

```bash
# List all available environments
pio env list

# Monitor serial output (replace with your environment name)
pio device monitor --environment esp32-c3-zero
```

## Running unit tests

```bash
pio test --environment test
```

This compiles `battery.cpp` and `timing.cpp` with the native compiler and runs 15 Unity tests – no hardware needed.

## CI

A GitHub Actions workflow (`.github/workflows/test.yml`) runs the unit tests automatically on every push and pull request.

## BLE identity

The device advertises as **"ESP32 Mouse"** by default. If you want to use a custom vendor product name for better compatibility, create a **local-only** override file:

1. Copy `src/config.local.h.example` to `src/config.local.h`
2. Edit the strings
3. Build as usual

**`config.local.h` is listed in `.gitignore` and will never be committed** – this avoids trademark issues if you publish the project publicly.

## Configuration

Key constants are in `lib/mousejiggler/config.h`:

| Constant               | Default   | Description                         |
|------------------------|-----------|-------------------------------------|
| `DISCONNECT_TIMEOUT`   | 300 s (5 min) | Idle before deep sleep           |
| `DEEP_SLEEP_US`        | 30 s      | Wake interval when sleeping         |
| `CPU_FREQ_MHZ`         | 80        | CPU frequency (MHz) – keeps PLL active for BT |
| Battery floor          | 3 %       | Never drain below this              |
| Movement delay         | 0–20 s    | Jittered delay between moves        |
| Idle pause             | 10 %      | Chance to pause 10–100 s after any move |

The drain rate is **dynamic** – see the table above.

## Dependencies

- [ESP32 BLE Mouse](https://github.com/T-vK/ESP32-BLE-Mouse) library (automatically fetched by PlatformIO)

## License

MIT
