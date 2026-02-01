#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>
#include <cstdint>

/**
 * RemoteProcess - Handles external process management and remote memory reading
 * 
 * This class provides an abstraction layer for reading memory from an external process.
 * Supports two modes:
 * 1. ReadProcessMemory API (default) - Standard Windows API
 * 2. Kernel Driver IOCTL - Custom driver for bypassing protections
 */

enum class MemoryReadMode
{
    API,        // Use ReadProcessMemory (default)
    Driver      // Use kernel driver IOCTL
};

class RemoteProcess
{
public:
    RemoteProcess();
    ~RemoteProcess();

    // Delete copy constructor and assignment operator
    RemoteProcess(const RemoteProcess&) = delete;
    RemoteProcess& operator=(const RemoteProcess&) = delete;

    /**
     * Attach to a process by name
     * @param processName The name of the process executable (e.g., "Game.exe")
     * @return true if successfully attached, false otherwise
     */
    bool Attach(const std::string& processName);

    /**
     * Attach to a process by PID
     * @param pid The process ID
     * @return true if successfully attached, false otherwise
     */
    bool Attach(DWORD pid);

    /**
     * Detach from the current process
     */
    void Detach();

    /**
     * Check if currently attached to a process
     * @return true if attached, false otherwise
     */
    bool IsAttached() const;

    /**
     * Read memory from the remote process
     * @param address The address to read from in the remote process
     * @param buffer The buffer to read into
     * @param size The number of bytes to read
     * @return true if successfully read, false otherwise
     */
    bool ReadMemory(uintptr_t address, void* buffer, size_t size) const;

    /**
     * Read a typed value from the remote process
     * @tparam T The type to read
     * @param address The address to read from
     * @return The value read (zero-initialized on failure)
     */
    template<typename T>
    T Read(uintptr_t address) const
    {
        T value{};
        if (!ReadMemory(address, &value, sizeof(T)))
        {
            // Return zero-initialized value on failure
            return T{};
        }
        return value;
    }

    /**
     * Read a null-terminated string from the remote process
     * @param address The address to read from
     * @param maxLength Maximum length to read
     * @return The string read (empty on failure)
     */
    std::string ReadString(uintptr_t address, size_t maxLength = 1024) const;

    /**
     * Read a null-terminated wide string from the remote process
     * @param address The address to read from
     * @param maxLength Maximum length to read
     * @return The wide string read (empty on failure)
     */
    std::wstring ReadWString(uintptr_t address, size_t maxLength = 1024) const;

    /**
     * Find a pattern in the remote process memory
     * @param pattern The pattern string (e.g., "48 8B 05 ? ? ? ?")
     * @param startAddress Start address for search (0 for module base)
     * @param searchSize Size of memory region to search (0 for entire module)
     * @return The address where pattern was found (0 if not found)
     */
    uintptr_t FindPattern(const char* pattern, uintptr_t startAddress = 0, size_t searchSize = 0) const;

    /**
     * Get the base address of a module in the remote process
     * @param moduleName The name of the module (nullptr for main module)
     * @return The base address of the module (0 if not found)
     */
    uintptr_t GetModuleBase(const char* moduleName = nullptr) const;

    /**
     * Get the size of a module in the remote process
     * @param moduleName The name of the module (nullptr for main module)
     * @return The size of the module (0 if not found)
     */
    size_t GetModuleSize(const char* moduleName = nullptr) const;

    /**
     * Get the process ID
     * @return The process ID (0 if not attached)
     */
    DWORD GetProcessId() const { return m_processId; }

    /**
     * Get the process handle
     * @return The process handle (NULL if not attached)
     */
    HANDLE GetProcessHandle() const { return m_processHandle; }

    /**
     * Get process name
     * @return The process name
     */
    std::string GetProcessName() const { return m_processName; }

    /**
     * Set the memory reading mode
     * @param mode The mode to use (API or Driver)
     */
    void SetMemoryReadMode(MemoryReadMode mode) { m_readMode = mode; }

    /**
     * Get the current memory reading mode
     * @return The current mode
     */
    MemoryReadMode GetMemoryReadMode() const { return m_readMode; }

    /**
     * Check if driver mode is active
     * @return true if using driver, false if using API
     */
    bool IsUsingDriver() const { return m_readMode == MemoryReadMode::Driver; }

    /**
     * List all running processes
     * @return Vector of pairs containing PID and process name
     */
    static std::vector<std::pair<DWORD, std::string>> ListProcesses();

    /**
     * Find process ID by name
     * @param processName The process name to search for
     * @return The process ID (0 if not found)
     */
    static DWORD FindProcessByName(const std::string& processName);

private:
    HANDLE m_processHandle;
    DWORD m_processId;
    std::string m_processName;
    uintptr_t m_mainModuleBase;
    size_t m_mainModuleSize;
    MemoryReadMode m_readMode;

    struct ModuleInfo
    {
        std::string name;
        uintptr_t base;
        size_t size;
    };

    mutable std::vector<ModuleInfo> m_moduleCache;

    /**
     * Update module cache
     */
    void UpdateModuleCache() const;

    /**
     * Find module in cache
     */
    const ModuleInfo* FindModuleInCache(const char* moduleName) const;
};

// Global singleton instance
extern RemoteProcess* g_RemoteProcess;

/**
 * Initialize the global remote process instance
 * Must be called before using any remote memory operations
 */
void InitRemoteProcess();

/**
 * Shutdown the global remote process instance
 */
void ShutdownRemoteProcess();
