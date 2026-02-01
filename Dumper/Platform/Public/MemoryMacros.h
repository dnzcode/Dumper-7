#pragma once

// Memory access macros for external driver support
// These macros replace direct pointer dereferencing with driver reads when in external mode

#include "MemoryInterface.h"

// Read typed value from address
#define READ_MEMORY(Type, Address) \
	(Memory::bUseDriverMode ? Memory::Read<Type>(reinterpret_cast<uintptr_t>(Address)) : *reinterpret_cast<Type*>(Address))

// Read typed value from address with offset
#define READ_AT(Type, Address, Offset) \
	(Memory::bUseDriverMode ? Memory::Read<Type>(reinterpret_cast<uintptr_t>(Address) + (Offset)) : *reinterpret_cast<Type*>(reinterpret_cast<uintptr_t>(Address) + (Offset)))

// Write typed value to address
#define WRITE_MEMORY(Type, Address, Value) \
	(Memory::bUseDriverMode ? Memory::Write<Type>(reinterpret_cast<uintptr_t>(Address), (Value)) : (*reinterpret_cast<Type*>(Address) = (Value), true))

// Read buffer from address
#define READ_BUFFER(Address, Buffer, Size) \
	(Memory::bUseDriverMode ? Memory::ReadMemory(reinterpret_cast<uintptr_t>(Address), (Buffer), (Size)) : (memcpy((Buffer), reinterpret_cast<void*>(Address), (Size)), true))

// Safe pointer dereference that works with both modes
#define DEREF_PTR(Type, Ptr) READ_MEMORY(Type, Ptr)

// Cast to typed pointer (for compatibility)
#define MEM_CAST(Type, Address) reinterpret_cast<Type*>(Address)
