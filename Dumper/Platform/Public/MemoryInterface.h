#pragma once

#include <Windows.h>
#include <cstdint>

namespace Memory
{
	// External memory access mode (using driver)
	extern bool bUseDriverMode;

	// Initialize memory interface
	bool Initialize(const wchar_t* processName = nullptr, int32_t processId = 0);
	
	// Shutdown memory interface
	void Shutdown();

	// Read memory from target process
	bool ReadMemory(uintptr_t address, void* buffer, size_t size);

	// Write memory to target process
	bool WriteMemory(uintptr_t address, const void* buffer, size_t size);

	// Check if address is valid/readable
	bool IsValidAddress(uintptr_t address);

	// Get base address of target process
	uintptr_t GetBaseAddress();

	// Templated read function
	template<typename T>
	T Read(uintptr_t address)
	{
		T buffer{};
		ReadMemory(address, &buffer, sizeof(T));
		return buffer;
	}

	// Templated write function
	template<typename T>
	bool Write(uintptr_t address, const T& value)
	{
		return WriteMemory(address, &value, sizeof(T));
	}

	// Read buffer
	template<typename T>
	bool ReadBuffer(uintptr_t address, T* buffer, size_t count)
	{
		return ReadMemory(address, buffer, sizeof(T) * count);
	}
}
