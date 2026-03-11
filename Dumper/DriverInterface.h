#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>

/**
 * DriverInterface - Communication layer for kernel driver IOCTL operations
 * 
 * This class provides an interface to a kernel driver for reading process memory.
 * It's an alternative to ReadProcessMemory that can bypass usermode protections.
 * 
 * Expected driver capabilities:
 * - Read process memory by PID
 * - Get module base address
 * - Get module size
 * - Enumerate modules
 */

// IOCTL Control Codes
// Note: These should match your driver's IOCTL definitions
// Customize these to match your specific driver

#define IOCTL_READ_MEMORY       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_MODULE_BASE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_MODULE_SIZE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_PROCESS_BASE  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Request structures for driver communication
#pragma pack(push, 1)

struct DRIVER_READ_REQUEST
{
    DWORD ProcessId;
    uintptr_t Address;
    SIZE_T Size;
    void* Buffer;
};

struct DRIVER_MODULE_REQUEST
{
    DWORD ProcessId;
    wchar_t ModuleName[260]; // MAX_PATH
    uintptr_t BaseAddress;
    SIZE_T ModuleSize;
};

struct DRIVER_PROCESS_BASE_REQUEST
{
    DWORD ProcessId;
    uintptr_t BaseAddress;
    SIZE_T ImageSize;
};

#pragma pack(pop)

class DriverInterface
{
public:
    DriverInterface();
    ~DriverInterface();

    // Delete copy constructor and assignment operator
    DriverInterface(const DriverInterface&) = delete;
    DriverInterface& operator=(const DriverInterface&) = delete;

    /**
     * Initialize and connect to the kernel driver
     * @param driverName The driver device name (e.g., "\\\\.\\MyDriver")
     * @return true if successfully connected, false otherwise
     */
    bool Initialize(const std::string& driverName);

    /**
     * Close connection to driver
     */
    void Shutdown();

    /**
     * Check if driver is connected and operational
     */
    bool IsInitialized() const { return m_driverHandle != INVALID_HANDLE_VALUE; }

    /**
     * Read memory from target process via driver
     * @param processId The target process ID
     * @param address The address to read from
     * @param buffer The buffer to read into
     * @param size The number of bytes to read
     * @return true if successful, false otherwise
     */
    bool ReadMemory(DWORD processId, uintptr_t address, void* buffer, size_t size);

    /**
     * Get module base address via driver
     * @param processId The target process ID
     * @param moduleName The module name (nullptr for main module)
     * @return The base address (0 if not found)
     */
    uintptr_t GetModuleBase(DWORD processId, const char* moduleName = nullptr);

    /**
     * Get module size via driver
     * @param processId The target process ID
     * @param moduleName The module name (nullptr for main module)
     * @return The module size (0 if not found)
     */
    size_t GetModuleSize(DWORD processId, const char* moduleName = nullptr);

    /**
     * Get the main module (process) base address
     * @param processId The target process ID
     * @return The base address (0 if not found)
     */
    uintptr_t GetProcessBaseAddress(DWORD processId);

private:
    HANDLE m_driverHandle;
    std::string m_driverName;

    /**
     * Send IOCTL to driver
     * @param controlCode The IOCTL control code
     * @param inputBuffer Input buffer
     * @param inputSize Input buffer size
     * @param outputBuffer Output buffer
     * @param outputSize Output buffer size
     * @return true if successful
     */
    bool SendIoctl(DWORD controlCode, void* inputBuffer, DWORD inputSize, void* outputBuffer, DWORD outputSize);
};

// Global driver interface instance
extern DriverInterface* g_DriverInterface;

/**
 * Initialize the global driver interface
 * @param driverName The driver device name
 * @return true if successful
 */
bool InitDriverInterface(const std::string& driverName);

/**
 * Shutdown the global driver interface
 */
void ShutdownDriverInterface();
