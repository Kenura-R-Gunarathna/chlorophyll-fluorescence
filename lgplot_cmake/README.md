# Spectrum Analyzer - ImGui + ImPlot

A real-time spectrum analyzer built with C++, ImGui, and ImPlot. Features an animated Gaussian peak with noise simulation, perfect for learning modern C++ GUI development.

![Spectrum Analyzer Screenshot](https://via.placeholder.com/800x450.png?text=Spectrum+Analyzer)

## Features

- 🎨 **Split-panel UI** with dockable windows
- 📊 **Live animated spectrum plot** with ImPlot
- 🎛️ **Interactive controls** for signal parameters
- 🔧 **Modern C++ with vcpkg** for dependency management
- 🪟 **Cross-platform** (Windows, Linux, macOS)

## Prerequisites

- **Visual Studio 2019/2022** with C++ development tools
- **CMake** 3.10 or higher (included with Visual Studio)
- **vcpkg** (integrated with Visual Studio)

## Quick Start

### Option 1: Visual Studio (Recommended)

1. **Clone the repository**
   ```bash
   git clone <your-repo-url>
   cd learnvcpkg
   ```

2. **Open in Visual Studio**
   - Open Visual Studio
   - File → Open → CMake...
   - Select `CMakeLists.txt`

3. **Build and Run**
   - Visual Studio will automatically install dependencies via vcpkg
   - Press `F5` to build and run
   - Or use Build → Build All (`F7`)

### Option 2: Command Line

1. **Open Developer Command Prompt for VS**

2. **Configure the project**
   ```bash
   cmake -B out/build -G "Visual Studio 17 2022" -A x64
   ```

3. **Build**
   ```bash
   cmake --build out/build --config Debug
   ```

4. **Run**
   ```bash
   .\out\build\Debug\learnvcpkg.exe
   ```

## Dependencies

All dependencies are managed by vcpkg and installed automatically:

- **ImGui** - Immediate-mode GUI library
- **ImPlot** - Plotting library for ImGui
- **GLFW3** - Window and input management
- **GLAD** - OpenGL function loader
- **fmt** - Modern C++ formatting library

## Project Structure

```
learnvcpkg/
├── CMakeLists.txt          # CMake build configuration
├── vcpkg.json              # vcpkg dependencies manifest
├── learnvcpkg.cpp          # Main application code
├── learnvcpkg.h            # Header file
├── README.md               # This file
├── docs.md                 # Detailed code documentation
└── .gitignore              # Git ignore rules
```

## Usage

### Controls Panel (Left)

- **Animate**: Toggle animation on/off
- **Frequency**: Adjust sine wave frequency (0.1 - 20 Hz)
- **Amplitude**: Control signal amplitude (0.1 - 5.0)
- **Noise Level**: Add random noise (0.0 - 1.0)
- **Peak Position**: Move the Gaussian peak (10 - 190)
- **Peak Width**: Adjust peak width (1 - 30)
- **Background Color**: Change the clear color
- **Reset Parameters**: Restore default values

### Live Spectrum Panel (Right)

- Real-time animated plot showing:
  - Gaussian peak (wobbles slightly)
  - Sine wave component
  - Random noise overlay
  - Shaded area under the curve

### Docking

- Drag window title bars to rearrange panels
- Drop windows on edges to split the view
- Drop on other windows to create tabs
- Drag tabs to undock windows

## Building from Scratch

If you want to set up a similar project from scratch:

1. **Create vcpkg.json**
   ```json
   {
     "dependencies": [
       "fmt",
       {
         "name": "imgui",
         "features": ["glfw-binding", "opengl3-binding", "docking-experimental"]
       },
       "implot",
       "glfw3",
       "glad"
     ]
   }
   ```

2. **Create CMakeLists.txt** with proper package finding and linking

3. **Write your application** using ImGui and ImPlot APIs

See `docs.md` for a detailed code walkthrough.

## Troubleshooting

### "CMake was unable to find a build program"
- Use Visual Studio Developer Command Prompt
- Or install Ninja: `choco install ninja`

### "Could not find any instance of Visual Studio"
- Make sure Visual Studio is installed with C++ tools
- Use the Developer Command Prompt for VS

### Dependencies not installing
- Delete `out/` folder and reconfigure
- Check that `vcpkg.json` is in the project root

### Build errors after adding dependencies
- Project → Delete Cache and Reconfigure in Visual Studio
- Or delete `out/build` folder and rebuild

## Learning Resources

- [ImGui Documentation](https://github.com/ocornut/imgui)
- [ImPlot Documentation](https://github.com/epezent/implot)
- [The Cherno - ImGui Tutorial](https://www.youtube.com/watch?v=VRwhNKoxUtk)
- [CMake Tutorial](https://cmake.org/cmake/help/latest/guide/tutorial/index.html)

## License

This project is provided as-is for educational purposes.

## Contributing

Feel free to open issues or submit pull requests!

## Acknowledgments

- [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut
- [ImPlot](https://github.com/epezent/implot) by Evan Pezent
- [GLFW](https://www.glfw.org/)
