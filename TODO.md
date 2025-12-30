# TODO: Future Features Implementation Plan

## 1. Checksum Validation for Data Integrity

### Overview
Add checksum verification to detect corrupted data in both USB Serial and UDP modes.

### ESP32 Changes
- [ ] Add CRC16 calculation to USB serial send (`usb_serial.cpp`)
- [ ] Modify packet format: `[0x11][CRC_hi][CRC_lo][pixel_data...]`
- [ ] Keep UDP checksum (already has simple sum in header)

### Desktop App Changes (lgplot_cmake)
- [ ] Add CRC16 validation in USB receiver (`connection.cpp`)
- [ ] Show checksum status in Statistics panel
- [ ] Track checksum error count
- [ ] Add "Packet Errors" display in Controls panel

### Protocol Enhancement
```
Current USB format:
  [0x11] → [0xA5][low][high][0x5A] × 3694

Enhanced format (Option A - per-frame CRC):
  [0x11][CRC16_hi][CRC16_lo] → [0xA5][low][high][0x5A] × 3694

Enhanced format (Option B - simpler):
  [0x11] → [pixel_0..pixel_3693] → [CRC16] (raw data, no per-pixel framing)
```

### Files to Modify
- `esp32/main/usb_serial.cpp` - Add CRC calculation
- `esp32/main/config.h` - Add CRC constants
- `lgplot_cmake/src/connection.cpp` - Add CRC validation
- `lgplot_cmake/src/app_state.h` - Add error counters
- `lgplot_cmake/src/ui_panels.cpp` - Display checksum status

---

## 2. Project Separation (Organized Snapshots)

### Overview
Group snapshots into named "projects" for better organization of experiments.

### Features
- [ ] Create/select projects from dropdown
- [ ] Each project has its own subfolder
- [ ] Project metadata (name, description, creation date)
- [ ] Browse snapshots within a project
- [ ] Copy/move snapshots between projects
- [ ] Export entire project as ZIP

### Data Structure
```
snapshots/
├── Project_LaserCalibration/
│   ├── project.json          # Metadata
│   ├── 2024-12-30_14-00-00/  # Snapshot folder
│   │   ├── spectrum_data.csv
│   │   ├── peaks.csv
│   │   └── metadata.txt
│   └── 2024-12-30_14-05-00/
├── Project_LEDSpectrum/
│   └── ...
└── Uncategorized/            # Default for unassigned snapshots
```

### project.json Format
```json
{
  "name": "Laser Calibration",
  "description": "HeNe laser + Mercury lamp calibration",
  "created": "2024-12-30T14:00:00",
  "snapshot_count": 5
}
```

### UI Changes
- [ ] Add "Projects" panel or section in Snapshot History
- [ ] Project dropdown selector
- [ ] "New Project" button with name input
- [ ] Project description text field
- [ ] Filter snapshots by project

### Files to Modify
- `lgplot_cmake/src/app_state.h` - Add project state
- `lgplot_cmake/src/ui_panels.cpp` - Add project UI
- `lgplot_cmake/src/ui_panels.h` - Add project function declarations

---

## Implementation Priority

| Feature | Effort | Impact | Priority |
|---------|--------|--------|----------|
| CRC16 checksum | Medium | High | 🔴 High |
| Project separation | Medium | Medium | 🟡 Medium |
| ZIP export | Low | Low | 🟢 Low |

## Checksum Implementation Steps

### Step 1: Add CRC16 library
```cpp
// Standard CRC16-CCITT
uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i] << 8;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}
```

### Step 2: Modify ESP32 sender
```cpp
// After sending pixel data, send CRC
uint16_t crc = crc16((uint8_t*)buffer, count * 2);
uint8_t crc_bytes[2] = {(uint8_t)(crc >> 8), (uint8_t)crc};
uart_write_bytes(UART_NUM_0, crc_bytes, 2);
```

### Step 3: Modify desktop receiver
```cpp
// After receiving all pixels, verify CRC
uint16_t expected_crc = (buffer[end-2] << 8) | buffer[end-1];
uint16_t actual_crc = crc16(pixel_data, pixel_count * 2);
if (expected_crc != actual_crc) {
    g_app.checksum_errors++;
    log_message("Checksum error! Expected %04X, got %04X", expected_crc, actual_crc);
}
```

---

## Notes
- Checksum adds ~2 bytes overhead per frame (~0.03% increase)
- Project separation is purely client-side, no ESP32 changes needed
- Consider SQLite for more complex project management in future
