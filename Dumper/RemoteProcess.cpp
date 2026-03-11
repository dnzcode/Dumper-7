#include "RemoteProcess.h"
#include "DriverInterface.h"
#include <Psapi.h>
#include <iostream>
#include <algorithm>

// Global singleton instance
RemoteProcess* g_RemoteProcess = nullptr;

void InitRemoteProcess()
{
    if (!g_RemoteProcess)
    {
        g_RemoteProcess = new RemoteProcess();
    }
}

void ShutdownRemoteProcess()
{
    if (g_RemoteProcess)
    {
        delete g_RemoteProcess;
        g_RemoteProcess = nullptr;
    }
}

RemoteProcess::RemoteProcess()
    : m_processHandle(NULL)
    , m_processId(0)
    , m_mainModuleBase(0)
    , m_mainModuleSize(0)
    , m_readMode(MemoryReadMode::API)  // Default to API mode
{
}

RemoteProcess::~RemoteProcess()
{
    Detach();
}

bool RemoteProcess::Attach(const std::string& processName)
{
    DWORD pid = FindProcessByName(processName);
    if (pid == 0)
    {
        std::cerr << "Process '" << processName << "' not found!" << std::endl;
        return false;
    }

    return Attach(pid);
}

bool RemoteProcess::Attach(DWORD pid)
{
    // Detach from any existing process first
    Detach();

    m_processId = pid;

    // If driver is available, prefer driver mode
    if (g_DriverInterface && g_DriverInterface->IsInitialized())
    {
        std::cout << "Using driver mode for memory access" << std::endl;
        m_readMode = MemoryReadMode::Driver;
        
        // In driver mode, we don't need a process handle for reading
        // But we still try to open it for compatibility
        m_processHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    }
    else
    {
        // Use standard API mode
        m_readMode = MemoryReadMode::API;
        
        // Open the process with necessary permissions
        m_processHandle = OpenProcess(
            PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
            FALSE,
            pid
        );

        if (m_processHandle == NULL)
        {
            DWORD error = GetLastError();
            std::cerr << "Failed to open process (PID: " << pid << "). Error: " << error << std::endl;
            
            if (error == ERROR_ACCESS_DENIED)
            {
                std::cerr << "Access denied. Try running as administrator or using a driver." << std::endl;
            }
            
            return false;
        }
    }

    // Get process name
    char processName[MAX_PATH] = {};
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        
        if (Process32First(snapshot, &pe32))
        {
            do
            {
                if (pe32.th32ProcessID == pid)
                {
                    m_processName = pe32.szExeFile;
                    break;
                }
            } while (Process32Next(snapshot, &pe32));
        }
        
        CloseHandle(snapshot);
    }

    // Get main module base and size
    m_mainModuleBase = GetModuleBase(nullptr);
    m_mainModuleSize = GetModuleSize(nullptr);

    if (m_mainModuleBase == 0)
    {
        std::cerr << "Failed to get main module base address!" << std::endl;
        Detach();
        return false;
    }

    std::cout << "Successfully attached to process:" << std::endl;
    std::cout << "  Name: " << m_processName << std::endl;
    std::cout << "  PID: " << m_processId << std::endl;
    std::cout << "  Base: 0x" << std::hex << m_mainModuleBase << std::dec << std::endl;
    std::cout << "  Size: 0x" << std::hex << m_mainModuleSize << std::dec << std::endl;

    return true;
}

void RemoteProcess::Detach()
{
    if (m_processHandle != NULL)
    {
        CloseHandle(m_processHandle);
        m_processHandle = NULL;
    }
    
    m_processId = 0;
    m_processName.clear();
    m_mainModuleBase = 0;
    m_mainModuleSize = 0;
    m_moduleCache.clear();
}

bool RemoteProcess::IsAttached() const
{
    return m_processHandle != NULL;
}

bool RemoteProcess::ReadMemory(uintptr_t address, void* buffer, size_t size) const
{
    if (!IsAttached() || address == 0 || buffer == nullptr || size == 0)
    {
        return false;
    }

    // Use driver if available and in driver mode
    if (m_readMode == MemoryReadMode::Driver && g_DriverInterface && g_DriverInterface->IsInitialized())
    {
        return g_DriverInterface->ReadMemory(m_processId, address, buffer, size);
    }

    // Fallback to ReadProcessMemory API
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(m_processHandle, reinterpret_cast<LPCVOID>(address), buffer, size, &bytesRead))
    {
        return false;
    }

    return bytesRead == size;
}

std::string RemoteProcess::ReadString(uintptr_t address, size_t maxLength) const
{
    if (!IsAttached() || address == 0)
    {
        return "";
    }

    std::vector<char> buffer(maxLength + 1, 0);
    if (!ReadMemory(address, buffer.data(), maxLength))
    {
        return "";
    }

    // Ensure null termination
    buffer[maxLength] = '\0';
    
    return std::string(buffer.data());
}

std::wstring RemoteProcess::ReadWString(uintptr_t address, size_t maxLength) const
{
    if (!IsAttached() || address == 0)
    {
        return L"";
    }

    std::vector<wchar_t> buffer(maxLength + 1, 0);
    if (!ReadMemory(address, buffer.data(), maxLength * sizeof(wchar_t)))
    {
        return L"";
    }

    // Ensure null termination
    buffer[maxLength] = L'\0';
    
    return std::wstring(buffer.data());
}

uintptr_t RemoteProcess::FindPattern(const char* pattern, uintptr_t startAddress, size_t searchSize) const
{
    if (!IsAttached() || pattern == nullptr)
    {
        return 0;
    }

    // If no start address specified, use main module base
    if (startAddress == 0)
    {
        startAddress = m_mainModuleBase;
    }

    // If no search size specified, use main module size
    if (searchSize == 0)
    {
        searchSize = m_mainModuleSize;
    }

    // Parse pattern string into bytes and mask
    std::vector<uint8_t> patternBytes;
    std::vector<bool> patternMask;
    
    const char* p = pattern;
    while (*p)
    {
        if (*p == ' ')
        {
            p++;
            continue;
        }
        
        if (*p == '?')
        {
            patternBytes.push_back(0);
            patternMask.push_back(false);
            p++;
            if (*p == '?') p++; // Handle "??"
        }
        else
        {
            char byte[3] = { p[0], p[1], '\0' };
            patternBytes.push_back(static_cast<uint8_t>(strtol(byte, nullptr, 16)));
            patternMask.push_back(true);
            p += 2;
        }
    }

    if (patternBytes.empty())
    {
        return 0;
    }

    // Read memory in chunks to avoid reading too much at once
    constexpr size_t CHUNK_SIZE = 0x100000; // 1MB chunks
    std::vector<uint8_t> buffer(CHUNK_SIZE);

    for (size_t offset = 0; offset < searchSize; offset += CHUNK_SIZE)
    {
        size_t readSize = std::min(CHUNK_SIZE, searchSize - offset);
        uintptr_t currentAddress = startAddress + offset;

        if (!ReadMemory(currentAddress, buffer.data(), readSize))
        {
            continue; // Skip this chunk if we can't read it
        }

        // Search for pattern in this chunk
        for (size_t i = 0; i < readSize - patternBytes.size(); i++)
        {
            bool found = true;
            for (size_t j = 0; j < patternBytes.size(); j++)
            {
                if (patternMask[j] && buffer[i + j] != patternBytes[j])
                {
                    found = false;
                    break;
                }
            }

            if (found)
            {
                return currentAddress + i;
            }
        }
    }

    return 0;
}

uintptr_t RemoteProcess::GetModuleBase(const char* moduleName) const
{
    if (!IsAttached())
    {
        return 0;
    }

    // If driver mode and driver is initialized, use driver
    if (m_readMode == MemoryReadMode::Driver && g_DriverInterface && g_DriverInterface->IsInitialized())
    {
        return g_DriverInterface->GetModuleBase(m_processId, moduleName);
    }

    // Otherwise use standard API
    // Update cache if empty
    if (m_moduleCache.empty())
    {
        UpdateModuleCache();
    }

    // If no module name specified, return main module
    if (moduleName == nullptr || strlen(moduleName) == 0)
    {
        return m_mainModuleBase != 0 ? m_mainModuleBase : 
            (!m_moduleCache.empty() ? m_moduleCache[0].base : 0);
    }

    // Search in cache
    const ModuleInfo* info = FindModuleInCache(moduleName);
    if (info)
    {
        return info->base;
    }

    return 0;
}

size_t RemoteProcess::GetModuleSize(const char* moduleName) const
{
    if (!IsAttached())
    {
        return 0;
    }

    // If driver mode and driver is initialized, use driver
    if (m_readMode == MemoryReadMode::Driver && g_DriverInterface && g_DriverInterface->IsInitialized())
    {
        return g_DriverInterface->GetModuleSize(m_processId, moduleName);
    }

    // Otherwise use standard API
    // Update cache if empty
    if (m_moduleCache.empty())
    {
        UpdateModuleCache();
    }

    // If no module name specified, return main module size
    if (moduleName == nullptr || strlen(moduleName) == 0)
    {
        return m_mainModuleSize != 0 ? m_mainModuleSize : 
            (!m_moduleCache.empty() ? m_moduleCache[0].size : 0);
    }

    // Search in cache
    const ModuleInfo* info = FindModuleInCache(moduleName);
    if (info)
    {
        return info->size;
    }

    return 0;
}

void RemoteProcess::UpdateModuleCache() const
{
    m_moduleCache.clear();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, m_processId);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return;
    }

    MODULEENTRY32 me32;
    me32.dwSize = sizeof(MODULEENTRY32);

    if (Module32First(snapshot, &me32))
    {
        do
        {
            ModuleInfo info;
            info.name = me32.szModule;
            info.base = reinterpret_cast<uintptr_t>(me32.modBaseAddr);
            info.size = me32.modBaseSize;
            
            m_moduleCache.push_back(info);

            // Cache main module info
            if (m_mainModuleBase == 0)
            {
                m_mainModuleBase = info.base;
                m_mainModuleSize = info.size;
            }
        } while (Module32Next(snapshot, &me32));
    }

    CloseHandle(snapshot);
}

const RemoteProcess::ModuleInfo* RemoteProcess::FindModuleInCache(const char* moduleName) const
{
    std::string searchName = moduleName;
    std::transform(searchName.begin(), searchName.end(), searchName.begin(), ::tolower);

    for (const auto& info : m_moduleCache)
    {
        std::string cachedName = info.name;
        std::transform(cachedName.begin(), cachedName.end(), cachedName.begin(), ::tolower);
        
        if (cachedName == searchName || cachedName.find(searchName) != std::string::npos)
        {
            return &info;
        }
    }

    return nullptr;
}

std::vector<std::pair<DWORD, std::string>> RemoteProcess::ListProcesses()
{
    std::vector<std::pair<DWORD, std::string>> processes;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return processes;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &pe32))
    {
        do
        {
            processes.emplace_back(pe32.th32ProcessID, pe32.szExeFile);
        } while (Process32Next(snapshot, &pe32));
    }

    CloseHandle(snapshot);
    return processes;
}

DWORD RemoteProcess::FindProcessByName(const std::string& processName)
{
    std::string searchName = processName;
    std::transform(searchName.begin(), searchName.end(), searchName.begin(), ::tolower);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    DWORD foundPid = 0;
    if (Process32First(snapshot, &pe32))
    {
        do
        {
            std::string currentName = pe32.szExeFile;
            std::transform(currentName.begin(), currentName.end(), currentName.begin(), ::tolower);
            
            if (currentName == searchName || currentName.find(searchName) != std::string::npos)
            {
                foundPid = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(snapshot, &pe32));
    }

    CloseHandle(snapshot);
    return foundPid;
}
