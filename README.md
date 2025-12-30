# TCD1304 CCD Spectrum Analyzer - Workspace

A complete system for reading TCD1304 linear CCD sensor data and visualizing it in real-time.

## 📁 Project Structure

```
ch2_fl/
├── esp32/              # ESP32 firmware (ESP-IDF)
├── lgplot/             # Desktop app - MSBuild version
├── lgplot_cmake/       # Desktop app - CMake version (recommended)
└── README.md           # This file
```

## 🚀 Quick Start

### Option 1: CMake Project (Recommended)

**Open in Visual Studio:**
```
File → Open → Folder → Select: c:\projects\cpp\ch2_fl\lgplot_cmake
```

**Build & Run:**
1. Select "x64-debug" configuration
2. Select "lgplot.exe" as startup item
3. Press F5

### Option 2: MSBuild Project

**Open in Visual Studio:**
```
File → Open → Project/Solution → Select: lgplot\lgplot.sln
```

**Build & Run:**
1. Select "Debug | x64"
2. Press F5

### ESP32 Firmware

```powershell
cd esp32
. C:\esp-idf\export.ps1
idf.py build
idf.py -p COM<X> flash monitor
```

## 📡 Communication Protocol

**UDP Broadcast:** Port 8080 to `192.168.4.255`

### Packet Format (7401 bytes)

| Field | Offset | Size | Description |
|-------|--------|------|-------------|
| `start_byte` | 0 | 1 | Magic `0xAA` |
| `sequence_num` | 1 | 4 | Packet counter |
| `timestamp_us` | 5 | 4 | Microseconds |
| `pixel_count` | 9 | 2 | 3694 pixels |
| `checksum` | 11 | 2 | Sum (lower 16 bits) |
| `pixels` | 13 | 7388 | 3694 × 16-bit values |

### USB Serial Format

```
[0xAA][seq_hi][seq_lo][cnt_hi][cnt_lo][pixel_data...][0x55]
```

## 🔌 Hardware Connections

| TCD1304 | ESP32 GPIO | Function |
|---------|------------|----------|
| SH | GPIO 32 | Shift Gate |
| MCLK | GPIO 25 | Master Clock (800kHz) |
| ICG | GPIO 33 | Integration Clear |
| OS | GPIO 36 | Analog Output → ADC |

## 🛠️ Development

### Prerequisites

- **Visual Studio 2026** (or 2022)
- **vcpkg** (automatic via manifest mode)
- **ESP-IDF** (for ESP32 firmware)

### Dependencies (auto-installed via vcpkg)

- imgui (with docking, glfw, opengl3 bindings)
- implot
- glfw3

### Building from Command Line

```powershell
# CMake version
cd lgplot_cmake
cmake --preset x64-debug
cmake --build out/build/x64-debug

# MSBuild version
cd lgplot
msbuild lgplot.vcxproj /p:Configuration=Debug /p:Platform=x64
```

## 📚 Documentation Structure

Each subproject has its own README:
- [`esp32/README.md`](esp32/README.md) - ESP32 firmware details
- [`lgplot/README.md`](lgplot/README.md) - MSBuild version
- [`lgplot_cmake/README.md`](lgplot_cmake/README.md) - CMake version

## 🧪 Testing Without Hardware

Set `USE_DUMMY_DATA = 1` in `esp32/main/config.h` for synthetic sine wave data.

## 📝 Git Workflow

```bash
# Check status
git status

# Stage changes
git add .

# Commit
git commit -m "Description of changes"

# View history
git log --oneline
```

## 🐛 Troubleshooting

**CMake cache errors:**
```powershell
# Delete cache and reconfigure
Remove-Item -Recurse out
# Then rebuild in VS
```

**vcpkg not installing:**
```powershell
# Check vcpkg root
echo $env:VCPKG_ROOT
# Should be: C:\vcpkg or VS integrated path
```

**IntelliSense errors (but builds fine):**
- Close Visual Studio
- Delete `.vs` folder
- Reopen project

## 📄 License

MIT License

---

**For detailed documentation, see individual project READMEs.**
