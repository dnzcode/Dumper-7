#include "DriverInterface.h"
#include <iostream>

// Global driver interface instance
DriverInterface* g_DriverInterface = nullptr;

bool InitDriverInterface(const std::string& driverName)
{
    if (!g_DriverInterface)
    {
        g_DriverInterface = new DriverInterface();
    }
    
    return g_DriverInterface->Initialize(driverName);
}

void ShutdownDriverInterface()
{
    if (g_DriverInterface)
    {
        g_DriverInterface->Shutdown();
        delete g_DriverInterface;
        g_DriverInterface = nullptr;
    }
}

DriverInterface::DriverInterface()
    : m_driverHandle(INVALID_HANDLE_VALUE)
{
}

DriverInterface::~DriverInterface()
{
    Shutdown();
}

bool DriverInterface::Initialize(const std::string& driverName)
{
    if (IsInitialized())
    {
        std::cerr << "Driver already initialized!" << std::endl;
        return true;
    }

    m_driverName = driverName;

    // Open handle to driver
    m_driverHandle = CreateFileA(
        driverName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (m_driverHandle == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        std::cerr << "Failed to open driver '" << driverName << "'. Error: " << error << std::endl;
        
        if (error == ERROR_FILE_NOT_FOUND)
        {
            std::cerr << "Driver not found. Make sure the driver is loaded." << std::endl;
        }
        else if (error == ERROR_ACCESS_DENIED)
        {
            std::cerr << "Access denied. Try running as Administrator." << std::endl;
        }
        
        return false;
    }

    std::cout << "Successfully connected to driver: " << driverName << std::endl;
    return true;
}

void DriverInterface::Shutdown()
{
    if (m_driverHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_driverHandle);
        m_driverHandle = INVALID_HANDLE_VALUE;
    }
}

bool DriverInterface::SendIoctl(DWORD controlCode, void* inputBuffer, DWORD inputSize, void* outputBuffer, DWORD outputSize)
{
    if (!IsInitialized())
    {
        return false;
    }

    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        m_driverHandle,
        controlCode,
        inputBuffer,
        inputSize,
        outputBuffer,
        outputSize,
        &bytesReturned,
        nullptr
    );

    return result != FALSE;
}

bool DriverInterface::ReadMemory(DWORD processId, uintptr_t address, void* buffer, size_t size)
{
    if (!IsInitialized() || buffer == nullptr || size == 0 || address == 0)
    {
        return false;
    }

    DRIVER_READ_REQUEST request = {};
    request.ProcessId = processId;
    request.Address = address;
    request.Size = size;
    request.Buffer = buffer;

    // Option 1: Driver writes directly to our buffer
    bool result = SendIoctl(
        IOCTL_READ_MEMORY,
        &request,
        sizeof(request),
        buffer,  // Output buffer
        static_cast<DWORD>(size)
    );

    if (!result)
    {
        // Option 2: Some drivers may use different structure
        // Try alternative approach where driver returns data in the same struct
        result = SendIoctl(
            IOCTL_READ_MEMORY,
            &request,
            sizeof(request),
            &request,
            sizeof(request)
        );
    }

    return result;
}

uintptr_t DriverInterface::GetModuleBase(DWORD processId, const char* moduleName)
{
    if (!IsInitialized())
    {
        return 0;
    }

    DRIVER_MODULE_REQUEST request = {};
    request.ProcessId = processId;
    request.BaseAddress = 0;
    request.ModuleSize = 0;

    if (moduleName != nullptr)
    {
        // Convert to wide string
        size_t len = strlen(moduleName);
        if (len >= 260) len = 259;
        
        for (size_t i = 0; i < len; i++)
        {
            request.ModuleName[i] = static_cast<wchar_t>(moduleName[i]);
        }
        request.ModuleName[len] = L'\0';
    }
    else
    {
        request.ModuleName[0] = L'\0';  // Empty name means main module
    }

    if (SendIoctl(
        IOCTL_GET_MODULE_BASE,
        &request,
        sizeof(request),
        &request,
        sizeof(request)))
    {
        return request.BaseAddress;
    }

    return 0;
}

size_t DriverInterface::GetModuleSize(DWORD processId, const char* moduleName)
{
    if (!IsInitialized())
    {
        return 0;
    }

    DRIVER_MODULE_REQUEST request = {};
    request.ProcessId = processId;
    request.BaseAddress = 0;
    request.ModuleSize = 0;

    if (moduleName != nullptr)
    {
        // Convert to wide string
        size_t len = strlen(moduleName);
        if (len >= 260) len = 259;
        
        for (size_t i = 0; i < len; i++)
        {
            request.ModuleName[i] = static_cast<wchar_t>(moduleName[i]);
        }
        request.ModuleName[len] = L'\0';
    }
    else
    {
        request.ModuleName[0] = L'\0';
    }

    if (SendIoctl(
        IOCTL_GET_MODULE_SIZE,
        &request,
        sizeof(request),
        &request,
        sizeof(request)))
    {
        return request.ModuleSize;
    }

    return 0;
}

uintptr_t DriverInterface::GetProcessBaseAddress(DWORD processId)
{
    if (!IsInitialized())
    {
        return 0;
    }

    DRIVER_PROCESS_BASE_REQUEST request = {};
    request.ProcessId = processId;
    request.BaseAddress = 0;
    request.ImageSize = 0;

    if (SendIoctl(
        IOCTL_GET_PROCESS_BASE,
        &request,
        sizeof(request),
        &request,
        sizeof(request)))
    {
        return request.BaseAddress;
    }

    return 0;
}
