# Building Hypseus Singe on Windows with Visual Studio 2022

This document describes how to build Hypseus Singe from source on Windows using Visual Studio 2022.

## Prerequisites

1. **Visual Studio 2022** - Community, Professional, or Enterprise edition
   - Install with "Desktop development with C++" workload
   - Download from: https://visualstudio.microsoft.com/downloads/

2. **CMake** (version 3.10 or later)
   - Download from: https://cmake.org/download/
   - Make sure to add CMake to system PATH during installation

3. **Git** (for cloning the repository)
   - Download from: https://git-scm.com/download/win

4. **vcpkg** (for dependency management)
   - Will be automatically set up by the build process

## Build Instructions

### Option 1: Using Command Line with vcpkg

1. Open a **Developer Command Prompt for VS 2022** or **Developer PowerShell for VS 2022**

2. Clone the repository:
   ```cmd
   git clone https://github.com/DirtBagXon/hypseus-singe.git
   cd hypseus-singe
   ```

3. Install vcpkg (if not already installed):
   ```cmd
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   cd ..
   ```

4. Install dependencies via vcpkg:
   ```cmd
   vcpkg\vcpkg install sdl2:x64-windows
   vcpkg\vcpkg install sdl2-ttf:x64-windows
   vcpkg\vcpkg install sdl2-image:x64-windows
   vcpkg\vcpkg install sdl2-mixer:x64-windows
   vcpkg\vcpkg install zlib:x64-windows
   vcpkg\vcpkg install libzip:x64-windows
   vcpkg\vcpkg install libogg:x64-windows
   vcpkg\vcpkg install libvorbis:x64-windows
   ```

5. Configure the project with CMake:
   ```cmd
   cmake -B build -S src -G "Visual Studio 17 2022" -A x64 ^
     -DCMAKE_BUILD_TYPE=Release ^
     -DCMAKE_TOOLCHAIN_FILE=vcpkg\scripts\buildsystems\vcpkg.cmake ^
     -DVCPKG_TARGET_TRIPLET=x64-windows
   ```

6. Build the project:
   ```cmd
   cmake --build build --config Release
   ```

7. The executable will be in `build/Release/hypseus.exe`

### Option 2: Using Visual Studio IDE

1. Open a **Developer Command Prompt for VS 2022**

2. Clone the repository and install dependencies (steps 2-4 from Option 1)

3. Configure the project with CMake:
   ```cmd
   cmake -B build -S src -G "Visual Studio 17 2022" -A x64 ^
     -DCMAKE_TOOLCHAIN_FILE=vcpkg\scripts\buildsystems\vcpkg.cmake ^
     -DVCPKG_TARGET_TRIPLET=x64-windows
   ```

4. Open the generated solution file:
   ```cmd
   start build\hypseus.sln
   ```

5. In Visual Studio:
   - Select **Release** configuration from the dropdown
   - Select **x64** platform from the dropdown
   - Build > Build Solution (or press Ctrl+Shift+B)

### Option 3: Using vcpkg Manifest Mode (Recommended)

The repository includes a `vcpkg.json` manifest file for automatic dependency management:

1. Clone the repository:
   ```cmd
   git clone https://github.com/DirtBagXon/hypseus-singe.git
   cd hypseus-singe
   ```

2. Install vcpkg (if not already installed):
   ```cmd
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   cd ..
   ```

3. Configure and build with CMake (dependencies will be installed automatically):
   ```cmd
   cmake -B build -S src -G "Visual Studio 17 2022" -A x64 ^
     -DCMAKE_BUILD_TYPE=Release ^
     -DCMAKE_TOOLCHAIN_FILE=vcpkg\scripts\buildsystems\vcpkg.cmake ^
     -DVCPKG_TARGET_TRIPLET=x64-windows
   
   cmake --build build --config Release
   ```

## Building for 32-bit Windows

To build a 32-bit version, use the following changes:

- In the vcpkg install commands (step 4 of Option 1), use the `:x86-windows` triplet suffix instead of `:x64-windows` for all packages
- In CMake configuration, use `-A Win32` instead of `-A x64`
- Use `-DVCPKG_TARGET_TRIPLET=x86-windows` instead of `x64-windows`

Example:
```cmd
cmake -B build -S src -G "Visual Studio 17 2022" -A Win32 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x86-windows
```

## Troubleshooting

### CMake cannot find dependencies

Make sure you're using the correct vcpkg toolchain file path and that all dependencies were installed successfully. Check the vcpkg installation logs for any errors.

### Link errors related to SDL2

Ensure you're using the correct triplet (x64-windows or x86-windows) consistently across all vcpkg installations and CMake configuration.

### Resource compiler errors

Make sure you have the Windows SDK installed as part of Visual Studio. You can modify your Visual Studio installation to add the Windows SDK if it's missing.

## Running the Built Executable

After building, you'll need to:

1. Copy the necessary DLL files from vcpkg:
   ```cmd
   copy vcpkg\installed\x64-windows\bin\*.dll build\Release\
   ```

2. Ensure the following folders exist in your Hypseus directory:
   - `pics/`, `fonts/`, `ram/`, `roms/`, `midi/`, `sound/`, `singe/`, `vldp/`

3. Run hypseus with appropriate arguments:
   ```cmd
   build\Release\hypseus.exe lair vldp -framefile vldp\lair\lair.txt
   ```

For more information about running Hypseus Singe, see the main [README.md](README.md) and [win32/README.md](win32/README.md).

## CI/CD with GitHub Actions

The repository includes a GitHub Actions workflow (`.github/workflows/cmake-windows-vs2022-platform.yml`) that automatically builds the Windows version using Visual Studio 2022. This ensures that the build process works correctly and produces artifacts for distribution.

## Additional Notes

- The build uses C++11 standard
- MSVC-specific warning flags are configured in CMakeLists.txt
- The Windows resource file (`hypseus.rc`) is automatically included in Windows builds
- Serial port support uses Windows-specific implementation (`rs232_windows.c`)
