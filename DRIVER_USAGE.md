# Kernel Driver Support for Dumper-7

Dumper-7 now supports using a custom kernel driver for reading process memory via IOCTL. This allows bypassing usermode protections that block `ReadProcessMemory`.

## Why Use a Driver?

Some games use anti-cheat or protection mechanisms that block usermode memory reading APIs like `ReadProcessMemory`. A kernel-mode driver can bypass these protections by reading memory directly from kernel space.

## Requirements

### Driver Implementation

Your kernel driver must implement the following IOCTL operations:

#### 1. Read Memory
**IOCTL Code**: `CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)`

**Input Structure**:
```cpp
struct DRIVER_READ_REQUEST
{
    DWORD ProcessId;
    uintptr_t Address;
    SIZE_T Size;
    void* Buffer;
};
```

**Operation**: Read `Size` bytes from `Address` in process `ProcessId` into `Buffer`.

#### 2. Get Module Base Address
**IOCTL Code**: `CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)`

**Input/Output Structure**:
```cpp
struct DRIVER_MODULE_REQUEST
{
    DWORD ProcessId;
    wchar_t ModuleName[260]; // Empty string = main module
    uintptr_t BaseAddress;   // Output
    SIZE_T ModuleSize;       // Output
};
```

**Operation**: Find module `ModuleName` in process `ProcessId` and return its base address.

#### 3. Get Module Size
**IOCTL Code**: `CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)`

Uses the same `DRIVER_MODULE_REQUEST` structure as above.

#### 4. Get Process Base Address
**IOCTL Code**: `CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)`

**Input/Output Structure**:
```cpp
struct DRIVER_PROCESS_BASE_REQUEST
{
    DWORD ProcessId;
    uintptr_t BaseAddress;  // Output
    SIZE_T ImageSize;       // Output
};
```

**Operation**: Get the main executable base address for process `ProcessId`.

### Driver Device Name

Your driver must create a device that can be opened via `CreateFile`. Common naming conventions:
- `\\\\.\\MyDriver`
- `\\\\.\\DumperDriver`
- `\\\\.\\MemoryDriver`

## Usage

### Command Line

```bash
# Use driver for memory reading
Dumper-7.exe Game.exe --driver \\\\.\\MyDriver

# By PID with driver
Dumper-7.exe 12345 --driver \\\\.\\MyDriver

# Interactive mode with driver (will prompt for process)
Dumper-7.exe --driver \\\\.\\MyDriver
```

### How It Works

1. Dumper-7 attempts to open the driver via the specified device name
2. If successful, all memory operations use the driver instead of `ReadProcessMemory`
3. If driver initialization fails, it automatically falls back to API mode
4. The mode (Driver or API) is displayed when the dumper starts

### Example Output

```
Initializing kernel driver: \\\\.\\MyDriver
Driver initialized successfully!
Memory will be read via driver IOCTL

Successfully attached to process:
  Name: Game-Win64-Shipping.exe
  PID: 12345
  Base: 0x7ff600000000
  Size: 0x5000000

[MODE] Using Kernel Driver for memory access
```

## Customizing IOCTL Codes

If your driver uses different IOCTL control codes, modify the definitions in `DriverInterface.h`:

```cpp
// Change these to match your driver
#define IOCTL_READ_MEMORY       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_MODULE_BASE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_MODULE_SIZE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_PROCESS_BASE  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
```

## Driver Development Notes

### Loading the Driver

```bash
# Create and start the driver service (requires admin)
sc create MyDriver binPath="C:\Path\To\MyDriver.sys" type=kernel
sc start MyDriver

# Stop and delete when done
sc stop MyDriver
sc delete MyDriver
```

### Driver Signing

- **Windows 10/11**: Drivers must be signed or test mode must be enabled
- **Test Mode**: `bcdedit /set testsigning on` (requires reboot)
- **Production**: Use a valid code signing certificate

### Security Considerations

- Kernel drivers run with full system privileges
- Ensure proper input validation in your driver
- Never expose dangerous operations via IOCTL
- Consider implementing access control (e.g., only allow specific processes)

## Example Driver Template

A minimal driver implementing the required IOCTLs would include:

```cpp
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    // Create device
    // Register dispatch routines
    // ...
}

NTSTATUS IoctlHandler(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG controlCode = stack->Parameters.DeviceIoControl.IoControlCode;
    
    switch (controlCode)
    {
    case IOCTL_READ_MEMORY:
        // Attach to target process
        // Use MmCopyVirtualMemory or similar
        break;
    
    case IOCTL_GET_MODULE_BASE:
        // Walk PEB->Ldr->InLoadOrderModuleList
        break;
    
    // ... other IOCTLs
    }
}
```

## Troubleshooting

### Driver Not Found
- Ensure the driver is loaded: `sc query MyDriver`
- Check device name matches: `\\\\.\\MyDriver`
- Run Dumper-7 as Administrator

### Access Denied
- Driver service must be running
- Check driver's security descriptor allows access
- Verify driver isn't being blocked by antivirus

### Memory Read Failures
- Verify process ID is correct
- Check driver's memory reading implementation
- Ensure target process memory is accessible

## Fallback Behavior

If the driver fails to initialize or any operation fails, Dumper-7 automatically falls back to using `ReadProcessMemory` API. This ensures the dumper always works, even without a driver.
