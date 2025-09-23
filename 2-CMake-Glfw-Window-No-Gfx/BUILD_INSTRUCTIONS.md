# Cross-Platform Build Instructions

This project provides a minimal C++ console application that can be built and run on Windows, macOS, and Ubuntu.

## Prerequisites

### Windows
- **Visual Studio 2022** (Community, Professional, or Enterprise) with C++ development tools
- **CMake** (3.4 or later)
- **Git** (optional, for version control)

### macOS
- **Xcode Command Line Tools**: `xcode-select --install`
- **CMake**: `brew install cmake` (requires Homebrew)
- **Git** (optional, for version control)

### Ubuntu/Debian
- **Build tools**: `sudo apt-get update && sudo apt-get install build-essential`
- **CMake**: `sudo apt-get install cmake`
- **Git** (optional, for version control)

## Quick Start

### Windows
```batch
# Build and run the application
build_and_run.bat

# Or run individual commands:
clean.bat          # Clean build directory
build_and_run.bat  # Build and run
run.bat            # Run existing build
```

### macOS/Ubuntu
```bash
# Make scripts executable (first time only)
chmod +x *.sh

# Build and run the application
./build_and_run.sh

# Or run individual commands:
./clean.sh         # Clean build directory
./build_and_run.sh # Build and run
./run.sh           # Run existing build
```

## Manual Build Process

### Windows
```batch
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -G "Visual Studio 17 2022" -A x64

# Build the project
cmake --build . --config Debug

# Run the application
cd Sandbox\Debug
MyGame.exe
```

### macOS/Ubuntu
```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build the project
make -j$(nproc)

# Run the application
cd Sandbox
./MyGame
```

## Project Structure

```
├── CMakeLists.txt          # Root CMake configuration
├── Engine/                 # Engine library
│   ├── CMakeLists.txt
│   └── Graphics/
│       ├── Window.hpp
│       └── Window.cpp
├── Sandbox/                # Application
│   ├── CMakeLists.txt
│   └── MyGame/
│       ├── main.cpp
│       ├── Game.hpp
│       └── Game.cpp
├── build/                  # Build output (generated)
├── build_and_run.bat       # Windows build and run script
├── build_and_run.sh        # Unix build and run script
├── clean.bat              # Windows clean script
├── clean.sh               # Unix clean script
├── run.bat                # Windows run script
└── run.sh                 # Unix run script
```

## Troubleshooting

### Common Issues

1. **CMake not found**
   - Windows: Install CMake and add to PATH
   - macOS: `brew install cmake`
   - Ubuntu: `sudo apt-get install cmake`

2. **Compiler not found**
   - Windows: Install Visual Studio with C++ tools
   - macOS: `xcode-select --install`
   - Ubuntu: `sudo apt-get install build-essential`

3. **Build fails with linking errors**
   - Clean the build directory and rebuild
   - Ensure all dependencies are installed

4. **Permission denied (Unix)**
   - Make scripts executable: `chmod +x *.sh`

### Platform-Specific Notes

#### Windows
- Uses Visual Studio 2022 generator by default
- Builds to `build\Sandbox\Debug\MyGame.exe`
- Requires Windows 10 or later

#### macOS
- Uses Unix Makefiles generator
- Builds to `build/Sandbox/MyGame`
- Requires macOS 10.15 or later

#### Ubuntu
- Uses Unix Makefiles generator
- Builds to `build/Sandbox/MyGame`
- Requires Ubuntu 18.04 or later

## Development

### Adding New Files
1. Add source files to appropriate directories
2. Update CMakeLists.txt if needed
3. Rebuild the project

### Modifying CMake Configuration
- Edit `CMakeLists.txt` files as needed
- Clean and rebuild after changes

### Code Style
- Follow C++20 standard
- Use consistent naming conventions
- Include proper error handling

## Output

When running successfully, the application will display:
```
=== Minimal C++ Console Application ===
Starting application...
Initializing Game: My Engine (800x600)
Game Engine Window Initialized...
Game initialization complete!
Game is running...
Game is shutting down...
Application completed successfully!
```

## Support

For issues or questions:
1. Check the troubleshooting section
2. Verify all prerequisites are installed
3. Clean and rebuild the project
4. Check the build output for specific error messages
