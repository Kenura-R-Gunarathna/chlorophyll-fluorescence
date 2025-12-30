# lgplot - TCD1304 CCD Spectrum Analyzer

Real-time spectrum visualization application for TCD1304 CCD sensor data.

## Features

- **Real-time UDP receiver** - Receives data from ESP32 over WiFi
- **ImPlot spectrum chart** - High-performance plotting of 3694 CCD pixels
- **Docking layout** - Customizable panel arrangement (Controls, Spectrum, Console)
- **Persistent layout** - User docking preferences saved to `lgplot_layout.ini`

## Prerequisites

- Visual Studio 2022 (v143 platform toolset)
- Windows SDK 10.0

## Building

1. Open `lgplot.sln` in Visual Studio 2022
2. Select **Debug|x64** or **Release|x64**
3. Build (Ctrl+Shift+B)

## Usage

1. **Connect to ESP32 WiFi**
   - SSID: `CCD_TCD1304`
   - Password: (none - open network)

2. **Launch lgplot.exe**

3. **Click "Start Receiver"** in the Controls panel

4. **View spectrum** in the center panel

## Project Structure

```
lgplot/
├── main.cpp              # Application with UDP receiver and docking UI
├── imgui/                # Dear ImGui library
├── implot/               # ImPlot charting library
├── libs/glfw/            # GLFW window library
├── lgplot_layout.ini     # User docking layout (auto-generated)
└── lgplot.sln            # Visual Studio solution
```

## Layout Customization

- Drag panel title bars to rearrange
- Dock panels to edges or as tabs
- Layout saved automatically to `lgplot_layout.ini`
- Delete `lgplot_layout.ini` to reset to default layout

## License

MIT License
