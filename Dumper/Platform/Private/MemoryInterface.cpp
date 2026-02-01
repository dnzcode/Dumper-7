#include "MemoryInterface.h"
#include "../../../driver.h"
#include <TlHelp32.h>
#include <iostream>

namespace Memory
{
	bool bUseDriverMode = true;
	static uintptr_t g_BaseAddress = 0;
	static HANDLE g_ProcessHandle = nullptr;

	bool Initialize(const wchar_t* processName, int32_t processId)
	{
		if (bUseDriverMode)
		{
			// Initialize kernel driver
			if (!mem::find_driver())
			{
				std::cerr << "[!] Failed to find driver!\n";
				return false;
			}

			// Find target process
			if (processId == 0 && processName != nullptr)
			{
				// Convert wchar_t to char for find_process
				char processNameChar[MAX_PATH] = { 0 };
				WideCharToMultiByte(CP_ACP, 0, processName, -1, processNameChar, MAX_PATH, NULL, NULL);
				
				processId = mem::find_process(processNameChar);
				if (processId == 0)
				{
					std::cerr << "[!] Failed to find process: " << processNameChar << "\n";
					return false;
				}
			}

			mem::process_id = processId;

			// Get base address
			g_BaseAddress = mem::find_image();
			if (g_BaseAddress == 0)
			{
				std::cerr << "[!] Failed to find image base address!\n";
				return false;
			}

			std::cerr << "[+] Driver initialized successfully\n";
			std::cerr << "[+] Process ID: " << processId << "\n";
			std::cerr << "[+] Base Address: 0x" << std::hex << g_BaseAddress << std::dec << "\n";
		}
		else
		{
			// Direct memory access mode (for injected DLL)
			g_BaseAddress = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
			g_ProcessHandle = GetCurrentProcess();
		}

		return true;
	}

	void Shutdown()
	{
		if (bUseDriverMode && mem::driver_handle != nullptr)
		{
			CloseHandle(mem::driver_handle);
			mem::driver_handle = nullptr;
		}
	}

	bool ReadMemory(uintptr_t address, void* buffer, size_t size)
	{
		if (bUseDriverMode)
		{
			mem::read_physical(reinterpret_cast<PVOID>(address), buffer, static_cast<DWORD>(size));
			return true;
		}
		else
		{
			// Direct memory access for injected mode
			if (address == 0 || buffer == nullptr || size == 0)
				return false;

			memcpy(buffer, reinterpret_cast<void*>(address), size);
			return true;
		}
	}

	bool WriteMemory(uintptr_t address, const void* buffer, size_t size)
	{
		if (bUseDriverMode)
		{
			mem::write_physical(reinterpret_cast<PVOID>(address), const_cast<PVOID>(buffer), static_cast<DWORD>(size));
			return true;
		}
		else
		{
			// Direct memory access for injected mode
			if (address == 0 || buffer == nullptr || size == 0)
				return false;

			memcpy(reinterpret_cast<void*>(address), buffer, size);
			return true;
		}
	}

	bool IsValidAddress(uintptr_t address)
	{
		if (address == 0)
			return false;

		if (bUseDriverMode)
		{
			// Try to read a single byte to check validity
			uint8_t dummy = 0;
			mem::read_physical(reinterpret_cast<PVOID>(address), &dummy, sizeof(dummy));
			// Driver doesn't return error codes, so we assume it's valid if address is non-zero
			return true;
		}
		else
		{
			// Use IsBadReadPtr for injected mode
			MEMORY_BASIC_INFORMATION mbi;
			if (VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) == 0)
				return false;

			return (mbi.State == MEM_COMMIT) && (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE));
		}
	}

	uintptr_t GetBaseAddress()
	{
		return g_BaseAddress;
	}
}
