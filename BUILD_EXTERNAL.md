# Building Dumper-7 in External Mode

## Quick Start

### Option 1: Using the Simplified External Build

The simplified external build provides a standalone executable that demonstrates driver integration without requiring modifications to the full dumper codebase.

**Build Instructions:**

1. **Using Visual Studio:**
   ```
   - Create a new Console Application project
   - Add these files:
     * Dumper/main_external_simple.cpp
     * driver.cpp
     * driver.h
   - Set Platform to x64
   - Set Configuration Type to Application (.exe)
   - Build and run
   ```

2. **Using CMake (Command Line):**
   ```bash
   # Create a simple CMakeLists.txt for the external build
   mkdir build_external
   cd build_external
   
   # Create minimal CMakeLists.txt
   cat > CMakeLists.txt << 'EOF'
   cmake_minimum_required(VERSION 3.15)
   project(Dumper7External)
   
   add_executable(Dumper-7-External
       ../Dumper/main_external_simple.cpp
       ../driver.cpp
   )
   
   target_include_directories(Dumper-7-External PRIVATE ${CMAKE_SOURCE_DIR}/..)
   target_compile_features(Dumper-7-External PRIVATE cxx_std_20)
   EOF
   
   cmake ..
   cmake --build . --config Release
   ```

### Option 2: Full External Mode (Advanced)

To build the complete dumper with external memory reading:

**Using CMake:**

```bash
# Build with external mode enabled
mkdir build
cd build
cmake -DEXTERNAL_MODE=ON ..
cmake --build . --config Release
```

Or use the provided CMakeLists_External.txt:

```bash
cp CMakeLists_External.txt CMakeLists.txt
mkdir build
cd build
cmake -DEXTERNAL_MODE=ON ..
cmake --build . --config Release
```

**Using Visual Studio:**

1. Open `Dumper-7.sln`
2. Right-click project → Properties
3. Configuration Properties → General:
   - Configuration Type: Application (.exe)
4. C/C++ → Preprocessor → Preprocessor Definitions:
   - Add: `EXTERNAL_MODE=1`
5. Linker → System → SubSystem:
   - Console (/SUBSYSTEM:CONSOLE)
6. Add `driver.cpp` to the project
7. Build

## Prerequisites

### Kernel Driver

You need a kernel driver that implements the memory read/write interface. The driver should:

1. Create a device named `\\.\dnzdriver`
2. Support these IOCTL codes:
   - `code_rw` (0x1645) - Read/Write memory
   - `code_ba` (0x1646) - Get base address
   - `code_get_guarded_region` (0x1647) - Get guarded region

See `driver.h` and `driver.cpp` for the expected interface.

### Loading the Driver

Before running the external dumper:

```batch
REM Load your kernel driver (requires admin privileges)
sc create dnzdriver binPath="C:\path\to\driver.sys" type=kernel
sc start dnzdriver

REM Verify driver is loaded
sc query dnzdriver
```

## Running the External Dumper

### Simplified Version:

```bash
# Run the executable
Dumper-7-External.exe

# Enter target process name when prompted
# Example: FortniteClient-Win64-Shipping.exe
```

The simplified version will:
- Initialize the kernel driver
- Find the target process
- Read the base address
- Perform a test memory read
- Display the results

### Full Version (After Complete Integration):

```bash
# Run the full external dumper
Dumper-7.exe

# Enter target process name when prompted
# SDK will be generated to C:\Dumper-7
```

## Switching Between Modes

The codebase supports both injection (DLL) and external (EXE) modes:

**For DLL Mode (Injection):**
- Don't define `EXTERNAL_MODE` or set it to 0
- Build as Dynamic Library (.dll)
- Use `DllMain` entry point
- Direct memory access

**For EXE Mode (External):**
- Define `EXTERNAL_MODE=1`
- Build as Application (.exe)
- Use `main()` entry point
- Driver-based memory access

## File Structure

```
Dumper-7/
├── driver.h                                # Kernel driver interface
├── driver.cpp                              # Driver communication
├── EXTERNAL_MODE_GUIDE.md                  # Complete integration guide
├── BUILD_EXTERNAL.md                       # This file
├── CMakeLists_External.txt                 # CMake configuration for external mode
└── Dumper/
    ├── main.cpp                            # Supports both DLL and EXE modes
    ├── main_external_simple.cpp            # Simplified external demo
    └── Platform/
        ├── Public/
        │   ├── MemoryInterface.h           # Memory abstraction layer
        │   ├── MemoryMacros.h              # Helper macros
        │   └── ExternalPtr.h               # Smart pointer wrapper
        └── Private/
            └── MemoryInterface.cpp         # Driver integration
```

## Integration Status

### ✅ Complete
- Driver interface (driver.h/driver.cpp)
- Memory abstraction layer (MemoryInterface.h/cpp)
- Main entry point support (main.cpp with EXTERNAL_MODE)
- Simplified working example (main_external_simple.cpp)
- Build system configuration (CMakeLists_External.txt)
- Documentation (EXTERNAL_MODE_GUIDE.md)

### ⚠️ Requires Manual Integration
The following files need to be updated to use `Memory::Read()` instead of direct pointer dereferencing:
- `Dumper/Engine/Private/Unreal/UnrealObjects.cpp`
- `Dumper/Engine/Private/Unreal/ObjectArray.cpp`
- `Dumper/Engine/Private/Unreal/NameArray.cpp`
- `Dumper/Engine/Private/OffsetFinder/OffsetFinder.cpp`
- `Dumper/Platform/Private/PlatformWindows.cpp`
- `Dumper/Engine/Public/Unreal/UnrealContainers.h`

See `EXTERNAL_MODE_GUIDE.md` for detailed conversion patterns.

## Troubleshooting

### "Failed to find driver"
- Ensure kernel driver is loaded: `sc query dnzdriver`
- Check driver name matches in driver.cpp (Line 12): `CreateFileW((L"\\\\.\\dnzdriver"), ...)`
- Run as Administrator

### "Failed to find process"
- Verify process name is exact (case-sensitive)
- Include ".exe" extension
- Ensure process is running before launching dumper

### "Failed to find image base address"
- Check that IOCTL `code_ba` is implemented in your driver
- Verify driver can read the target process memory
- Ensure target process is not protected by anti-cheat

### Compiler Errors
- Ensure C++20 standard is enabled
- Add `$(ProjectDir)\..\` to include directories
- Link against `driver.cpp` and `MemoryInterface.cpp`

## Next Steps

1. **Test the simplified version first** to ensure driver communication works
2. **Follow EXTERNAL_MODE_GUIDE.md** to integrate external reads into the full codebase
3. **Test with simple Unreal Engine games** before production use
4. **Add error handling** and logging as needed

## Security Considerations

- Kernel drivers can destabilize your system - test in a VM
- External memory reading may trigger anti-cheat systems
- Only use for authorized research and development
- Ensure your driver properly validates all requests

## License

Follow the same license as the main Dumper-7 project.

## Support

For issues:
1. Check that simplified example works first
2. Verify driver is properly implemented
3. Review EXTERNAL_MODE_GUIDE.md for integration patterns
4. Check driver IOCTL codes match your implementation
