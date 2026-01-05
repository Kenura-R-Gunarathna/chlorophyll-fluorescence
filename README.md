# TCD1304 CCD Spectrum Analyzer

Real-time spectrum visualization system for TCD1304 linear CCD sensor.

## 📁 Projects

| Folder | Description | Build System |
|--------|-------------|--------------|
| [`lgplot/`](lgplot/) | Desktop spectrum analyzer (Windows/Linux) | CMake + vcpkg |
| [`esp32/`](esp32/) | ESP32 firmware (sensor control + WiFi/USB) | ESP-IDF |

## 🚀 Quick Start

### Desktop App (lgplot)

**Download**: See [Releases](../../releases) for pre-built binaries.

**Build from source**: See [`lgplot/README.md`](lgplot/README.md)

### ESP32 Firmware

```bash
cd esp32
. $IDF_PATH/export.sh  # or export.ps1 on Windows
idf.py build flash monitor
```

See [`esp32/README.md`](esp32/README.md) for details.

## 🔌 Hardware

| TCD1304 Pin | ESP32 GPIO | Function |
|-------------|------------|----------|
| SH | 32 | Shift Gate |
| MCLK | 25 | Master Clock (800kHz) |
| ICG | 33 | Integration Clear |
| OS | 36 | Analog Output |

## 📡 Connection Modes

- **USB Serial**: 921600 baud (lowest latency)
- **WiFi UDP**: Port 8080, SSID `CCD_TCD1304`

## 🍎 macOS

See [`BUILDING_MACOS.md`](BUILDING_MACOS.md) for build instructions.

## 📄 License

MIT License
