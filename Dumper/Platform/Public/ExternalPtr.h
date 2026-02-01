#pragma once

#include "MemoryInterface.h"
#include <cstdint>
#include <type_traits>

// External memory pointer wrapper for reading memory through driver
template<typename T>
class ExternalPtr
{
private:
	uintptr_t m_Address;

public:
	ExternalPtr() : m_Address(0) {}
	ExternalPtr(uintptr_t addr) : m_Address(addr) {}
	ExternalPtr(const void* ptr) : m_Address(reinterpret_cast<uintptr_t>(ptr)) {}
	ExternalPtr(std::nullptr_t) : m_Address(0) {}

	// Assignment operators
	ExternalPtr& operator=(uintptr_t addr) { m_Address = addr; return *this; }
	ExternalPtr& operator=(const void* ptr) { m_Address = reinterpret_cast<uintptr_t>(ptr); return *this; }
	ExternalPtr& operator=(std::nullptr_t) { m_Address = 0; return *this; }

	// Dereference operator - reads from external memory
	T operator*() const
	{
		if (Memory::bUseDriverMode)
			return Memory::Read<T>(m_Address);
		else
			return *reinterpret_cast<T*>(m_Address);
	}

	// Arrow operator for member access
	T operator->() const
	{
		return operator*();
	}

	// Array access operator
	T operator[](size_t index) const
	{
		if (Memory::bUseDriverMode)
			return Memory::Read<T>(m_Address + (index * sizeof(T)));
		else
			return reinterpret_cast<T*>(m_Address)[index];
	}

	// Pointer arithmetic
	ExternalPtr operator+(ptrdiff_t offset) const { return ExternalPtr(m_Address + (offset * sizeof(T))); }
	ExternalPtr operator-(ptrdiff_t offset) const { return ExternalPtr(m_Address - (offset * sizeof(T))); }
	ExternalPtr& operator+=(ptrdiff_t offset) { m_Address += (offset * sizeof(T)); return *this; }
	ExternalPtr& operator-=(ptrdiff_t offset) { m_Address -= (offset * sizeof(T)); return *this; }
	ExternalPtr& operator++() { m_Address += sizeof(T); return *this; }
	ExternalPtr operator++(int) { ExternalPtr tmp = *this; ++(*this); return tmp; }
	ExternalPtr& operator--() { m_Address -= sizeof(T); return *this; }
	ExternalPtr operator--(int) { ExternalPtr tmp = *this; --(*this); return tmp; }

	// Comparison operators
	bool operator==(const ExternalPtr& other) const { return m_Address == other.m_Address; }
	bool operator!=(const ExternalPtr& other) const { return m_Address != other.m_Address; }
	bool operator<(const ExternalPtr& other) const { return m_Address < other.m_Address; }
	bool operator>(const ExternalPtr& other) const { return m_Address > other.m_Address; }
	bool operator<=(const ExternalPtr& other) const { return m_Address <= other.m_Address; }
	bool operator>=(const ExternalPtr& other) const { return m_Address >= other.m_Address; }

	bool operator==(std::nullptr_t) const { return m_Address == 0; }
	bool operator!=(std::nullptr_t) const { return m_Address != 0; }

	// Conversion operators
	operator bool() const { return m_Address != 0; }
	operator uintptr_t() const { return m_Address; }
	uintptr_t GetAddress() const { return m_Address; }

	// Cast to regular pointer (for direct mode)
	T* AsPtr() const { return reinterpret_cast<T*>(m_Address); }
};

// Helper function to read memory at an address with offset
template<typename T>
inline T ReadAt(uintptr_t address, size_t offset = 0)
{
	if (Memory::bUseDriverMode)
		return Memory::Read<T>(address + offset);
	else
		return *reinterpret_cast<T*>(address + offset);
}

// Helper function to write memory at an address with offset
template<typename T>
inline bool WriteAt(uintptr_t address, const T& value, size_t offset = 0)
{
	if (Memory::bUseDriverMode)
		return Memory::Write<T>(address + offset, value);
	else
	{
		*reinterpret_cast<T*>(address + offset) = value;
		return true;
	}
}
