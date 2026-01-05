# Building on macOS

macOS requires a manual build due to OpenGL loader compatibility issues with vcpkg's default `glad` library.

## Prerequisites

```bash
# Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake ninja glfw
```

## Option 1: Use Homebrew for All Dependencies (Recommended)

```bash
# Install ImGui and ImPlot via Homebrew
brew install imgui implot

# Clone and build
git clone https://github.com/YOUR_USERNAME/chlorophyll-fluorescence.git
cd chlorophyll-fluorescence/lgplot

# Configure with system libraries
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix)"

# Build
cmake --build build

# Run
./build/lgplot
```

## Option 2: Use gl3w Instead of glad

If using vcpkg, switch the OpenGL loader from `glad` to `gl3w`:

### 1. Modify `lgplot/vcpkg.json`

```json
{
  "dependencies": [
    {
      "name": "imgui",
      "features": [
        "glfw-binding",
        "opengl3-gl3w-binding",
        "docking-experimental"
      ]
    },
    "implot",
    "glfw3",
    "gl3w"
  ]
}
```

### 2. Build with vcpkg

```bash
# Clone vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && ./bootstrap-vcpkg.sh && cd ..

# Build
cd lgplot
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build build
./build/lgplot
```

## Troubleshooting

**Error: `glad` related symbols not found**
- Use Option 1 (Homebrew) or switch to gl3w as shown in Option 2

**Error: OpenGL deprecated warnings**
- These are warnings only and can be ignored on macOS 10.14+
- Apple deprecated OpenGL but it still works

**GLFW window doesn't appear**
- Grant terminal/app camera or accessibility permissions in System Preferences
