# TCD1304 CCD Spectrum Analyzer

A complete system for reading TCD1304 linear CCD sensor data and visualizing it in real-time.

## Components

| Folder | Description |
|--------|-------------|
| `esp32/` | ESP32 firmware - WiFi AP broadcasting CCD data via UDP |
| `lgplot/` | Windows desktop app - Real-time spectrum visualization with ImGui/ImPlot |

## Quick Start

### 1. ESP32 Firmware

```powershell
cd esp32

# Set up ESP-IDF environment
. C:\esp-idf\export.ps1

# Build and flash
idf.py build
idf.py -p COM<X> flash monitor
```

### 2. Desktop Application

1. Open `lgplot/lgplot.sln` in Visual Studio 2022
2. Build (Debug|x64)
3. Connect to WiFi: `CCD_TCD1304`
4. Run the application

## Communication Protocol

The ESP32 broadcasts UDP packets on port **8080** to `192.168.4.255`.

### Packet Format

| Field | Offset | Size | Description |
|-------|--------|------|-------------|
| `start_byte` | 0 | 1 | Magic byte `0xAA` |
| `sequence_num` | 1 | 4 | Packet counter |
| `timestamp_us` | 5 | 4 | Microsecond timestamp |
| `pixel_count` | 9 | 2 | Number of pixels (3694) |
| `checksum` | 11 | 2 | Sum of pixel values (lower 16 bits) |
| `pixels` | 13 | 7388 | 3694 × 16-bit pixel values |

**Total packet size**: 7401 bytes

### Parsing Example (C++)

```cpp
#pragma pack(push, 1)
struct UdpPacketHeader {
    uint8_t  start_byte;     // 0xAA
    uint32_t sequence_num;
    uint32_t timestamp_us;
    uint16_t pixel_count;
    uint16_t checksum;
};
#pragma pack(pop)

// After receiving packet:
if (buffer[0] == 0xAA) {
    auto* header = reinterpret_cast<UdpPacketHeader*>(buffer);
    uint16_t* pixels = reinterpret_cast<uint16_t*>(buffer + sizeof(UdpPacketHeader));
    // Process pixels[0] to pixels[header->pixel_count - 1]
}
```

## Hardware Connections

| TCD1304 Signal | ESP32 GPIO | Description |
|----------------|------------|-------------|
| SH | GPIO 32 | Shift Gate |
| MCLK | GPIO 25 | Master Clock (800kHz) |
| ICG | GPIO 33 | Integration Clear Gate |
| OS | GPIO 36 | Analog output → ADC |

## Testing Without Hardware

Set `USE_DUMMY_DATA` to `1` in `esp32/main/config.h` to generate synthetic sine wave data.

## License

MIT License
