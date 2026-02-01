# Dumper-7 External Mode - Complete Implementation Guide

## Overview
This document provides the complete code for converting Dumper-7 to external mode using a kernel driver.

## Files Modified/Created

### 1. MemoryInterface.h (Dumper/Platform/Public/MemoryInterface.h)
Already created - provides abstraction layer for memory access

### 2. MemoryInterface.cpp (Dumper/Platform/Private/MemoryInterface.cpp)  
Already created - implements driver-based memory reads/writes

### 3. main.cpp (Dumper/main.cpp)
Already modified - supports both DLL injection and external EXE mode

## Complete Integration Pattern

### Pattern Scanning (PlatformWindows.cpp)
Pattern scanning needs to read memory externally. Replace:
```cpp
// OLD - Direct memory access
const uint8_t* SearchStart = reinterpret_cast<uint8_t*>(StartAddress);
if (SearchStart[i] == 0x4C) { ... }
```

With:
```cpp
// NEW - External memory access
uint8_t currentByte = Memory::Read<uint8_t>(StartAddress + i);
if (currentByte == 0x4C) { ... }
```

### UnrealObjects Memory Access (Engine/Private/Unreal/UnrealObjects.cpp)
Replace all pointer dereferences:
```cpp
// OLD
*reinterpret_cast<void**>(Field + Off::FField::Owner)
```

With:
```cpp
// NEW
Memory::Read<void*>(Field + Off::FField::Owner)
```

### ObjectArray/NameArray Access
For arrays, use Memory::ReadBuffer():
```cpp
// OLD
FUObjectItem* Items = *reinterpret_cast<FUObjectItem**>(ChunkAddr);
```

With:
```cpp
// NEW
FUObjectItem Items[ChunkSize];
Memory::ReadBuffer(ChunkAddr, Items, sizeof(Items));
```

### IsBadReadPtr Replacement (PlatformWindows.cpp)
The IsBadReadPtr function should use Memory::IsValidAddress():
```cpp
bool PlatformWindows::IsBadReadPtr(const void* Address)
{
    if (Memory::bUseDriverMode)
        return !Memory::IsValidAddress(reinterpret_cast<uintptr_t>(Address));
    
    // Original implementation for injected mode
    MEMORY_BASIC_INFORMATION Mbi;
    if (VirtualQuery(Address, &Mbi, sizeof(Mbi)))
    {
        constexpr DWORD AccessibleMask = (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | 
                                         PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
        constexpr DWORD InaccessibleMask = (PAGE_GUARD | PAGE_NOACCESS);
        return !(Mbi.Protect & AccessibleMask) || (Mbi.Protect & InaccessibleMask);
    }
    return true;
}
```

## Build Configuration

### Visual Studio Project Settings (For EXTERNAL_MODE)

1. **Configuration Type**: Change from "Dynamic Library (.dll)" to "Application (.exe)"
2. **Subsystem**: Console (/SUBSYSTEM:CONSOLE)
3. **Add to Additional Include Directories**: `$(ProjectDir)\..\`
4. **Link driver.cpp and MemoryInterface.cpp**

### CMakeLists.txt Modifications

```cmake
# Add option for external mode
option(EXTERNAL_MODE "Build as external executable instead of DLL" ON)

if(EXTERNAL_MODE)
    add_definitions(-DEXTERNAL_MODE=1)
    # Build as executable
    add_executable(Dumper-7
        ${SOURCES}
        ${CMAKE_SOURCE_DIR}/driver.cpp
        Dumper/Platform/Private/MemoryInterface.cpp
    )
else()
    # Build as DLL
    add_library(Dumper-7 SHARED
        ${SOURCES}
    )
endif()
```

## Usage Instructions

### For External Mode (EXE):

1. Load your kernel driver (dnzdriver)
2. Run Dumper-7.exe
3. Enter the target process name when prompted (e.g., "GameProcess.exe")
4. SDK will be generated externally without injection

### For Injected Mode (DLL):

1. Compile with EXTERNAL_MODE undefined or set to 0
2. Inject Dumper-7.dll into target process
3. Press F6 to unload when complete

## Key Conversion Rules

### Rule 1: Replace All Pointer Dereferences
```cpp
// Before:
auto value = *reinterpret_cast<Type*>(address);

// After:
auto value = Memory::Read<Type>(address);
```

### Rule 2: Multi-Level Pointer Chains
```cpp
// Before:
void* ptr1 = *reinterpret_cast<void**>(base);
void* ptr2 = *reinterpret_cast<void**>(ptr1 + offset);

// After:
void* ptr1 = Memory::Read<void*>(base);
void* ptr2 = Memory::Read<void*>(reinterpret_cast<uintptr_t>(ptr1) + offset);
```

### Rule 3: Array Access
```cpp
// Before:
MyStruct* arr = reinterpret_cast<MyStruct*>(baseAddr);
MyStruct item = arr[index];

// After:
MyStruct item = Memory::Read<MyStruct>(baseAddr + (index * sizeof(MyStruct)));
```

### Rule 4: String Reading
```cpp
// Before:
const char* str = reinterpret_cast<const char*>(addr);

// After:
char buffer[256];
Memory::ReadBuffer(addr, buffer, sizeof(buffer));
const char* str = buffer;
```

## Critical Files to Modify

These files contain the most memory accesses and must be updated:

1. **Dumper/Engine/Private/Unreal/UnrealObjects.cpp** - Core UObject memory access
2. **Dumper/Engine/Private/Unreal/ObjectArray.cpp** - GObjects traversal
3. **Dumper/Engine/Private/Unreal/NameArray.cpp** - GNames traversal  
4. **Dumper/Engine/Private/OffsetFinder/OffsetFinder.cpp** - Pattern scanning
5. **Dumper/Platform/Private/PlatformWindows.cpp** - Platform memory operations
6. **Dumper/Engine/Public/Unreal/UnrealContainers.h** - TArray, TMap operators

## Testing

After conversion:

1. Test with a simple Unreal Engine game
2. Verify GObjects and GNames are found correctly
3. Ensure SDK generation completes without crashes
4. Compare generated SDK with injected mode output

## Troubleshooting

### Issue: Driver not found
- Ensure kernel driver is loaded: `sc query dnzdriver`
- Check driver handle creation in driver.cpp

### Issue: Failed to find process  
- Verify process name is exact (case-sensitive)
- Ensure process is running before launching dumper

### Issue: Invalid memory reads
- Check that Memory::Initialize() completed successfully
- Verify base address was found correctly
- Ensure driver IOCTL codes match your kernel driver

### Issue: Crash during dump
- Add null checks before Memory::Read() calls
- Validate addresses with Memory::IsValidAddress()
- Check for buffer overflows in ReadBuffer() calls

## Example: Complete File Conversion

Here's a complete example of UnrealTypes.cpp FName conversion:

```cpp
// In FName::ToString()
void FName::ToString(FFreableString& OutString) const
{
    if (Off::InSDK::Name::bIsUsingAppendStringOverToString)
    {
        // Use external memory read
        uint32 ComparisonIndex = Memory::Read<uint32>(reinterpret_cast<uintptr_t>(Address));
        
        if (GetNameEntryFromName)
        {
            const void* NameEntry = GetNameEntryFromName(ComparisonIndex);
            // Read name data externally
            // ... rest of implementation
        }
    }
}
```

## Final Code Structure

```
Dumper-7/
├── driver.h                          # Kernel driver interface (existing)
├── driver.cpp                        # Driver communication (existing)
└── Dumper/
    ├── main.cpp                      # Entry point (modified for EXE/DLL)
    └── Platform/
        ├── Public/
        │   ├── MemoryInterface.h     # NEW - Memory abstraction
        │   └── MemoryMacros.h        # NEW - Helper macros
        └── Private/
            ├── MemoryInterface.cpp   # NEW - Driver integration
            └── PlatformWindows.cpp   # MODIFY - Use Memory::Read()
```

## Conclusion

This guide provides the complete pattern for converting Dumper-7 to external mode. The key is replacing all direct memory operations with Memory::Read/Write calls while maintaining backward compatibility with injected mode.
