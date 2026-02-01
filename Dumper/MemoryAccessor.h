#pragma once

#include <cstdint>
#include <cstring>
#include "RemoteProcess.h"

/**
 * MemoryAccessor - Global memory access layer that works with both local and remote process
 * 
 * This class provides a unified interface for memory access that can work either:
 * 1. Locally (when running as injected DLL) - direct pointer dereferences
 * 2. Remotely (when running as external process) - ReadProcessMemory calls
 * 
 * The mode is determined at runtime based on whether a RemoteProcess is attached.
 */
class MemoryAccessor
{
public:
    /**
     * Read memory from target process (local or remote)
     * @param address Address to read from
     * @param buffer Buffer to read into
     * @param size Size to read
     * @return true if successful
     */
    static bool Read(uintptr_t address, void* buffer, size_t size)
    {
        if (g_RemoteProcess && g_RemoteProcess->IsAttached())
        {
            // Remote mode: use ReadProcessMemory
            return g_RemoteProcess->ReadMemory(address, buffer, size);
        }
        else
        {
            // Local mode: direct memory access
            if (address == 0 || buffer == nullptr || size == 0)
                return false;
            
            std::memcpy(buffer, reinterpret_cast<void*>(address), size);
            return true;
        }
    }

    /**
     * Read a typed value from target process
     */
    template<typename T>
    static T Read(uintptr_t address)
    {
        T value{};
        Read(address, &value, sizeof(T));
        return value;
    }

    /**
     * Check if an address is readable
     */
    static bool IsValidPtr(uintptr_t address)
    {
        if (address == 0)
            return false;

        if (g_RemoteProcess && g_RemoteProcess->IsAttached())
        {
            // Remote mode: try to read a byte
            uint8_t test;
            return g_RemoteProcess->ReadMemory(address, &test, 1);
        }
        else
        {
            // Local mode: use IsBadReadPtr
            return !IsBadReadPtr(reinterpret_cast<void*>(address), 1);
        }
    }

    /**
     * Check if we're in remote mode
     */
    static bool IsRemoteMode()
    {
        return g_RemoteProcess && g_RemoteProcess->IsAttached();
    }
};

// Helper macros for common operations
#define READ_REMOTE(address, type) MemoryAccessor::Read<type>(reinterpret_cast<uintptr_t>(address))
#define READ_REMOTE_BUF(address, buffer, size) MemoryAccessor::Read(reinterpret_cast<uintptr_t>(address), buffer, size)
#define IS_VALID_REMOTE_PTR(address) MemoryAccessor::IsValidPtr(reinterpret_cast<uintptr_t>(address))
