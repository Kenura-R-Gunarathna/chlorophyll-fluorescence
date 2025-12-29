# TCD1304 CCD WiFi Access Point

ESP32 project that reads TCD1304 linear CCD sensor data and broadcasts it over WiFi UDP.

## Features

- **Modular design** - Easy to read and maintain
- **Dummy data mode** - Test without hardware using synthetic sine waves
- **WiFi AP mode** - ESP32 creates its own network
- **UDP broadcast** - Data sent to all connected clients

## Hardware Connections

| Signal | GPIO | Description |
|--------|------|-------------|
| SH     | 32   | Shift Gate |
| MCLK   | 25   | Master Clock (800kHz) |
| ICG    | 33   | Integration Clear Gate |
| ADC    | 36   | Analog input |

## Quick Start

### 1. Set up ESP-IDF environment

**CMD:**
```cmd
C:\esp-idf\export.bat
```

**PowerShell:**
```powershell
. C:\esp-idf\export.ps1
```

### 2. Build

```bash
idf.py build
```

### 3. Flash and monitor

```bash
idf.py -p COM<X> flash monitor
```

### 4. Connect and receive data

1. Connect to WiFi: `CCD_TCD1304`
2. Listen on UDP port `8080`
3. Receive 3694 pixels per packet

## Switching Between Real/Dummy Data

Edit `main/config.h`:
```c
#define USE_DUMMY_DATA  1   // 1 = sine wave test data
#define USE_DUMMY_DATA  0   // 0 = real CCD sensor
```

## Project Structure

```
main/
├── config.h           # All settings and pin definitions
├── ccd_sensor.h/cpp   # Real CCD hardware driver
├── ccd_dummy.h/cpp    # Dummy sine wave generator
├── wifi_ap.h/cpp      # WiFi Access Point
├── udp_broadcast.h/cpp # UDP packet transmission
└── main.cpp           # Entry point
```

## UDP Packet Format

| Field | Size | Description |
|-------|------|-------------|
| start_byte | 1 | 0xAA magic byte |
| sequence_num | 4 | Packet counter |
| timestamp_us | 4 | Microsecond timestamp |
| pixel_count | 2 | Number of pixels (3694) |
| checksum | 2 | Sum of pixel values |
| pixels | 7388 | 3694 x 16-bit values |

## Useful Commands

| Command | Description |
|---------|-------------|
| `idf.py build` | Build project |
| `idf.py flash` | Flash to device |
| `idf.py monitor` | Serial monitor |
| `idf.py menuconfig` | Configuration menu |
| `idf.py fullclean` | Clean build |
