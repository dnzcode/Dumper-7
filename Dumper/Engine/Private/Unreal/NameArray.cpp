
#include <format>

#include "Unreal/ObjectArray.h"
#include "Unreal/NameArray.h"

#include "Platform.h"
#include "Architecture.h"
#include "MemoryAccessor.h"

uint8* NameArray::GNames = nullptr;

FNameEntry::FNameEntry(void* Ptr)
	: Address((uint8*)Ptr)
{
}

std::wstring FNameEntry::GetWString()
{
	if (!Address)
		return L"";

	return GetStr(Address);
}

std::string FNameEntry::GetString()
{
	if (!Address)
		return "";

	return UtfN::WStringToString(GetWString());
}

void* FNameEntry::GetAddress()
{
	return Address;
}

void FNameEntry::Init(const uint8_t* FirstChunkPtr, int64 NameEntryStringOffset)
{
	if (Settings::Internal::bUseNamePool)
	{
		constexpr int64 NoneStrLen = 0x4;
		constexpr uint16 BytePropertyStrLen = 0xC;

		constexpr uint32 BytePropertyStartAsUint32 = 'etyB'; // "Byte" part of "ByteProperty"

		Off::FNameEntry::NamePool::StringOffset = NameEntryStringOffset;
		Off::FNameEntry::NamePool::HeaderOffset = NameEntryStringOffset == 6 ? 4 : 0;

		uint8* FirstChunkAddress = MemoryAccessor::Read<uint8*>(reinterpret_cast<uintptr_t>(FirstChunkPtr));
		const uint8* AssumedBytePropertyEntry = FirstChunkAddress + NameEntryStringOffset + NoneStrLen;

		/* Check if there's pading after an FNameEntry. Check if there's up to 0x4 bytes padding. */
		for (int i = 0; i < 0x4; i++)
		{
			const uint32 FirstPartOfByteProperty = MemoryAccessor::Read<uint32>(reinterpret_cast<uintptr_t>(AssumedBytePropertyEntry + NameEntryStringOffset));

			if (FirstPartOfByteProperty == BytePropertyStartAsUint32)
				break;

			AssumedBytePropertyEntry += 0x1;
		}

		uint16 BytePropertyHeader = MemoryAccessor::Read<uint16>(reinterpret_cast<uintptr_t>(AssumedBytePropertyEntry + Off::FNameEntry::NamePool::HeaderOffset));

		/* Shifiting past the size of the header is not allowed, so limmit the shiftcount here */
		constexpr int32 MaxAllowedShiftCount = sizeof(BytePropertyHeader) * 0x8;

		while (BytePropertyHeader != BytePropertyStrLen && FNameEntryLengthShiftCount < MaxAllowedShiftCount)
		{			
			FNameEntryLengthShiftCount++;
			BytePropertyHeader >>= 1;
		}

		if (FNameEntryLengthShiftCount == MaxAllowedShiftCount)
		{
			std::cerr << "\nDumper-7: Error, couldn't get FNameEntryLengthShiftCount!\n" << std::endl;
			GetStr = [](uint8* NameEntry) -> std::wstring { return L"Invalid FNameEntryLengthShiftCount!"; };
			return;
		}

		GetStr = [](uint8* NameEntry) -> std::wstring
		{
			const uint16 HeaderWithoutNumber = MemoryAccessor::Read<uint16>(reinterpret_cast<uintptr_t>(NameEntry + Off::FNameEntry::NamePool::HeaderOffset));
			const int32 NameLen = HeaderWithoutNumber >> FNameEntry::FNameEntryLengthShiftCount;

			if (NameLen == 0)
			{
				const int32 EntryIdOffset = Off::FNameEntry::NamePool::StringOffset + ((Off::FNameEntry::NamePool::StringOffset == 6) * 2);

				const int32 NextEntryIndex = MemoryAccessor::Read<int32>(reinterpret_cast<uintptr_t>(NameEntry + EntryIdOffset));
				const int32 Number = MemoryAccessor::Read<int32>(reinterpret_cast<uintptr_t>(NameEntry + EntryIdOffset + sizeof(int32)));

				if (Number > 0)
					return NameArray::GetNameEntry(NextEntryIndex).GetWString() + L'_' + std::to_wstring(Number - 1);

				return NameArray::GetNameEntry(NextEntryIndex).GetWString();
			}

			if (HeaderWithoutNumber & NameWideMask)
			{
				std::wstring result(NameLen, L'\0');
				MemoryAccessor::Read(reinterpret_cast<uintptr_t>(NameEntry + Off::FNameEntry::NamePool::StringOffset), result.data(), NameLen * sizeof(wchar_t));
				return result;
			}

			std::string narrowStr(NameLen, '\0');
			MemoryAccessor::Read(reinterpret_cast<uintptr_t>(NameEntry + Off::FNameEntry::NamePool::StringOffset), narrowStr.data(), NameLen);
			return UtfN::StringToWString(narrowStr);
		};
	}
	else
	{
		const uint8_t* FNameEntryNone =     static_cast<uint8_t*>(NameArray::GetNameEntry(0x0).GetAddress());
		const uint8_t* FNameEntryIdxThree = static_cast<uint8_t*>(NameArray::GetNameEntry(0x3).GetAddress());
		const uint8_t* FNameEntryIdxEight = static_cast<uint8_t*>(NameArray::GetNameEntry(0x8).GetAddress());

		for (int i = 0; i < 0x20; i++)
		{
			if (MemoryAccessor::Read<uint32>(reinterpret_cast<uintptr_t>(FNameEntryNone + i)) == 'enoN') // None
			{
				Off::FNameEntry::NameArray::StringOffset = i;
				break;
			}
		}

		for (int i = 0; i < 0x20; i++)
		{
			// lowest bit is bIsWide mask, shift right by 1 to get the index
			if ((MemoryAccessor::Read<uint32>(reinterpret_cast<uintptr_t>(FNameEntryIdxThree + i)) >> 1) == 0x3 &&
				(MemoryAccessor::Read<uint32>(reinterpret_cast<uintptr_t>(FNameEntryIdxEight + i)) >> 1) == 0x8)
			{
				Off::FNameEntry::NameArray::IndexOffset = i;
				break;
			}
		}

		GetStr = [](uint8* NameEntry) -> std::wstring
		{
			const int32 NameIdx = MemoryAccessor::Read<int32>(reinterpret_cast<uintptr_t>(NameEntry + Off::FNameEntry::NameArray::IndexOffset));

			if (NameIdx & NameWideMask)
			{
				// Read wide string
				std::wstring result;
				uintptr_t strAddr = reinterpret_cast<uintptr_t>(NameEntry + Off::FNameEntry::NameArray::StringOffset);
				wchar_t wc;
				while (true)
				{
					wc = MemoryAccessor::Read<wchar_t>(strAddr);
					if (wc == L'\0')
						break;
					result += wc;
					strAddr += sizeof(wchar_t);
				}
				return result;
			}

			// Read narrow string
			std::string narrowStr;
			uintptr_t strAddr = reinterpret_cast<uintptr_t>(NameEntry + Off::FNameEntry::NameArray::StringOffset);
			char c;
			while (true)
			{
				c = MemoryAccessor::Read<char>(strAddr);
				if (c == '\0')
					break;
				narrowStr += c;
				strAddr += sizeof(char);
			}
			return UtfN::StringToWString<std::string>(narrowStr);
		};
	}
}

bool NameArray::InitializeNameArray(uint8_t* NameArray)
{
	int32 ValidPtrCount = 0x0;
	int32 ZeroQWordCount = 0x0;

	int32 PerChunk = 0x0;

	if (!NameArray || Platform::IsBadReadPtr(NameArray))
		return false;

	for (int i = 0; i < 0x800; i += sizeof(void*))
	{
		uint8_t* SomePtr = MemoryAccessor::Read<uint8_t*>(reinterpret_cast<uintptr_t>(NameArray + i));

		if (SomePtr == 0)
		{
			ZeroQWordCount++;
		}
		else if (ZeroQWordCount == 0x0 && SomePtr != nullptr)
		{
			ValidPtrCount++;
		}
		else if (ZeroQWordCount > 0 && SomePtr != 0)
		{
			int32 NumElements = MemoryAccessor::Read<int32>(reinterpret_cast<uintptr_t>(NameArray + i));
			int32 NumChunks = MemoryAccessor::Read<int32>(reinterpret_cast<uintptr_t>(NameArray + i + 4));

			if (NumChunks == ValidPtrCount)
			{
				Off::NameArray::NumElements = i;
				Off::NameArray::MaxChunkIndex = i + 4;

				ByIndex = [](void* NamesArray, int32 ComparisonIndex, int32 NamePoolBlockOffsetBits) -> void*
				{
					const int32 ChunkIdx = ComparisonIndex / 0x4000;
					const int32 InChunk = ComparisonIndex % 0x4000;

					if (ComparisonIndex > NameArray::GetNumElements())
						return nullptr;

					// Read chunk pointer
					uint8_t* ChunkPtr = MemoryAccessor::Read<uint8_t*>(reinterpret_cast<uintptr_t>(NamesArray) + ChunkIdx * sizeof(void*));
					if (!ChunkPtr)
						return nullptr;

					// Read entry pointer from chunk
					return MemoryAccessor::Read<void*>(reinterpret_cast<uintptr_t>(ChunkPtr) + InChunk * sizeof(void*));
				};

				return true;
			}
		}
	}

	return false;
}

bool NameArray::InitializeNamePool(uint8_t* NamePool)
{
	Off::NameArray::MaxChunkIndex = 0x0;
	Off::NameArray::ByteCursor = 0x4;

	Off::NameArray::ChunksStart = 0x10;

	bool bWasMaxChunkIndexFound = false;

	for (int i = 0x0; i < 0x20; i += 4)
	{
		const int32 PossibleMaxChunkIdx = MemoryAccessor::Read<int32>(reinterpret_cast<uintptr_t>(NamePool + i));

		if (PossibleMaxChunkIdx <= 0 || PossibleMaxChunkIdx > 0x10000)
			continue;

		int32 NotNullptrCount = 0x0;
		bool bFoundFirstPtr = false;

		/* Number of invalid pointers we can encounter before we assume that there are no valid pointers anymore. */
		constexpr int32 MaxAllowedNumInvalidPtrs = 0x500;
		int32 NumPtrsSinceLastValid = 0x0;

		for (int j = 0x0; j < 0x10000; j += 8)
		{
			const int32 ChunkOffset = i + 8 + j + (i % 8);

			if (MemoryAccessor::Read<uint8_t*>(reinterpret_cast<uintptr_t>(NamePool + ChunkOffset)) != nullptr)
			{
				NotNullptrCount++;
				NumPtrsSinceLastValid = 0;

				if (!bFoundFirstPtr)
				{
					bFoundFirstPtr = true;
					Off::NameArray::ChunksStart = i + 8 + j + (i % 8);
				}
			}
			else
			{
				NumPtrsSinceLastValid++;

				/* The last time we've seen a non-nullptr value was 0x500 iterations ago. It's safe to say we wont find any more. */
				if (NumPtrsSinceLastValid == MaxAllowedNumInvalidPtrs)
					break;
			}
		}

		if (PossibleMaxChunkIdx == (NotNullptrCount - 1))
		{
			Off::NameArray::MaxChunkIndex = i;
			Off::NameArray::ByteCursor = i + 4;
			bWasMaxChunkIndexFound = true;
			break;
		}
	}

	if (!bWasMaxChunkIndexFound)
		return false;

	constexpr uint64 CoreUObjAsUint64 = 0x6A624F5565726F43; // little endian "jbOUeroC" ["/Script/CoreUObject"]
	constexpr uint32 NoneAsUint32 = 0x656E6F4E; // little endian "None"

	uint8_t* ChunkPtr = MemoryAccessor::Read<uint8_t*>(reinterpret_cast<uintptr_t>(NamePool + Off::NameArray::ChunksStart));

	// "/Script/CoreUObject"
	bool bFoundCoreUObjectString = false;
	int64 FNameEntryHeaderSize = 0x0;

	constexpr int32 LoopLimit = 0x1000;

	for (int i = 0; i < LoopLimit; i++)
	{
		if (MemoryAccessor::Read<uint32>(reinterpret_cast<uintptr_t>(ChunkPtr + i)) == NoneAsUint32 && FNameEntryHeaderSize == 0)
		{
			FNameEntryHeaderSize = i;
		}
		else if (MemoryAccessor::Read<uint64>(reinterpret_cast<uintptr_t>(ChunkPtr + i)) == CoreUObjAsUint64)
		{
			bFoundCoreUObjectString = true;
			break;
		}
	}

	if (!bFoundCoreUObjectString)
		return false;

	NameEntryStride = FNameEntryHeaderSize == 2 ? 2 : 4;
	Off::InSDK::NameArray::FNameEntryStride = NameEntryStride;

	ByIndex = [](void* NamesArray, int32 ComparisonIndex, int32 NamePoolBlockOffsetBits) -> void*
	{
		const int32 ChunkIdx = ComparisonIndex >> NamePoolBlockOffsetBits;
		const int32 InChunkOffset = (ComparisonIndex & ((1 << NamePoolBlockOffsetBits) - 1)) * NameEntryStride;

		const bool bIsBeyondLastChunk = ChunkIdx == NameArray::GetNumChunks() && InChunkOffset > NameArray::GetByteCursor();

		if (ChunkIdx < 0 || ChunkIdx > GetNumChunks() || bIsBeyondLastChunk)
			return nullptr;

		uint8_t* ChunkPtrAddress = reinterpret_cast<uint8_t*>(NamesArray) + 0x10;

		// Read the chunk pointer from the chunk pointer array
		uint8_t* ChunkPtr = MemoryAccessor::Read<uint8_t*>(reinterpret_cast<uintptr_t>(ChunkPtrAddress) + ChunkIdx * sizeof(uint8_t*));

		return ChunkPtr + InChunkOffset;
	};

	Settings::Internal::bUseNamePool = true;
	// Pass the address in the NamePool where the chunk pointer is stored
	FNameEntry::Init(reinterpret_cast<uint8*>(NamePool + Off::NameArray::ChunksStart), FNameEntryHeaderSize);

	return true;
}


/* 
 * Finds a call to FName::GetNames, OR a reference to GNames directly, if the call has been inlined
 * 
 * returns { GetNames/GNames, bIsGNamesDirectly };
*/
inline std::pair<uintptr_t, bool> FindFNameGetNamesOrGNames_Windows(const uintptr_t EnterCriticalSectionAddress, const uintptr_t StartAddress)
{
#ifdef PLATFORM_WINDOWS

	/* 2 bytes operation + 4 bytes relative offset */
	constexpr int32 ASMRelativeCallSizeBytes = 0x6;

	/* Range from "ByteProperty" which we want to search upwards for "GetNames" call */
	constexpr int32 GetNamesCallSearchRange = 0x150;

	/* Find a reference to the string "ByteProperty" in 'FName::StaticInit' */
	const uint8* BytePropertyStringAddress = static_cast<uint8*>(Platform::FindByStringInAllSections(L"ByteProperty", StartAddress, 0x0, Settings::General::bSearchOnlyExecutableSectionsForStrings));

	/* Important to prevent infinite-recursion */
	if (!BytePropertyStringAddress)
		return { 0x0, false };

	for (int i = 0; i < GetNamesCallSearchRange; i++)
	{
		/* Check upwards (yes negative indexing) for a relative call opcode */
		if (BytePropertyStringAddress[-i] != 0xFF)
			continue;

#if defined(_WIN64)
		const uintptr_t CallTarget = Architecture_x86_64::Resolve32BitSectionRelativeCall(reinterpret_cast<uintptr_t>(BytePropertyStringAddress - i));
#elif defined(_WIN32)
		uintptr_t CallTarget = Architecture_x86_64::Resolve32bitAbsoluteCall(reinterpret_cast<uintptr_t>(BytePropertyStringAddress - i));
#endif

		if (CallTarget != EnterCriticalSectionAddress)
			continue;

		const uintptr_t InstructionAfterCall = reinterpret_cast<uintptr_t>(BytePropertyStringAddress - (i - ASMRelativeCallSizeBytes));
		
		/* Check if we're dealing with a 'call' opcode */
		if (MemoryAccessor::Read<uint8>(InstructionAfterCall) == 0xE8)
			return { Architecture_x86_64::Resolve32BitRelativeCall(InstructionAfterCall), false };

		// Looks like on 32bit like literally everything is absolute???? fuck you
#if defined(_WIN64)
		return { Architecture_x86_64::Resolve32BitRelativeMove(InstructionAfterCall), true };
#elif defined(_WIN32)
		return { Architecture_x86_64::Resolve32bitAbsoluteMove(InstructionAfterCall), true };
#endif
	}

	/* Continue and search for another reference to "ByteProperty", safe because we're checking if another string-ref was found*/
	return FindFNameGetNamesOrGNames_Windows(EnterCriticalSectionAddress, reinterpret_cast<uintptr_t>(BytePropertyStringAddress) + ASMRelativeCallSizeBytes);

#endif // PLATFORM_WINDOWS
};

bool NameArray::TryFindNameArray_Windows()
{
#ifdef PLATFORM_WINDOWS

	/* Type of 'static TNameEntryArray& FName::GetNames()' */
	using GetNameType = void* (*)();

	/* Range from 'FName::GetNames' which we want to search down for 'mov register, GNames' */
	constexpr int32 GetNamesCallSearchRange = 0x100;

	const void* EnterCriticalSectionAddress = Platform::GetAddressOfImportedFunctionFromAnyModule("kernel32.dll", "EnterCriticalSection");

	auto [Address, bIsGNamesDirectly] = FindFNameGetNamesOrGNames_Windows(reinterpret_cast<uintptr_t>(EnterCriticalSectionAddress), Platform::GetModuleBase());

	if (Address == 0x0)
		return false;

	if (bIsGNamesDirectly)
	{
		if (!Platform::IsAddressInProcessRange(Address) || Platform::IsBadReadPtr(MemoryAccessor::Read<void*>(Address)))
			return false;

		Off::InSDK::NameArray::GNames = Platform::GetOffset(Address);
		return true;
	}

	// TODO (encryqed): Fix below for 32-bit ue shit 

	/* Call GetNames to retreive the pointer to the allocation of the name-table, used for later comparison */
	void* Names = reinterpret_cast<GetNameType>(Address)();

	for (int i = 0; i < GetNamesCallSearchRange; i++)
	{
		/* Check upwards (yes negative indexing) for a relative call opcode */
		if (MemoryAccessor::Read<uint16>(Address + i) != 0x8B48)
			continue;

		const uintptr_t MoveTarget = Architecture_x86_64::Resolve32BitRelativeMove(Address + i);

		if (!Platform::IsAddressInProcessRange(MoveTarget))
			continue;

		const void* ValueOfMoveTargetAsPtr = MemoryAccessor::Read<void*>(MoveTarget);

		if (Platform::IsBadReadPtr(ValueOfMoveTargetAsPtr) || ValueOfMoveTargetAsPtr != Names)
			continue;

		Off::InSDK::NameArray::GNames = Platform::GetOffset(MoveTarget);
		return true;
	}
	
	return false;

#endif // PLATFORM_WINDOWS
}

bool NameArray::TryFindNamePool_Windows()
{
#ifdef PLATFORM_WINDOWS

	// TODO (encryqed): Fix this below for 32-bit ue games ig?

	/* Number of bytes we want to search for an indirect call to InitializeSRWLock */
	constexpr int32 InitSRWLockSearchRange = 0x50;

	/* Number of bytes we want to search for lea instruction loading the string "ByteProperty" */
	constexpr int32 BytePropertySearchRange = 0x2A0;

	/* FNamePool::FNamePool contains a call to InitializeSRWLock or RtlInitializeSRWLock, we're going to check for that later */
	const uintptr_t InitSRWLockAddress = reinterpret_cast<uintptr_t>(Platform::GetAddressOfImportedFunctionFromAnyModule("kernel32.dll", "InitializeSRWLock"));
	const uintptr_t RtlInitSRWLockAddress = reinterpret_cast<uintptr_t>(Platform::GetAddressOfImportedFunctionFromAnyModule("ntdll.dll", "RtlInitializeSRWLock"));

	/* Singleton instance of FNamePool, which is passed as a parameter to FNamePool::FNamePool */
	void* NamePoolIntance = nullptr;

	uintptr_t SigOccurrence = 0x0;;

	uintptr_t Counter = 0x0;

	while (!NamePoolIntance)
	{
		/* add 0x1 so we don't find the same occurence again and cause an infinite loop (20min. of debugging for that) */
		if (SigOccurrence > 0x0)
			SigOccurrence += 0x1;

		/* Find the next occurence of this signature to see if that may be a call to the FNamePool constructor */
		SigOccurrence = reinterpret_cast<uintptr_t>(Platform::FindPattern("48 8D 0D ? ? ? ? E8", 0x0, true, SigOccurrence));

		if (SigOccurrence == 0x0)
			break;

		constexpr int32 SizeOfMovInstructionBytes = 0x7;

		const uintptr_t PossibleConstructorAddress = Architecture_x86_64::Resolve32BitRelativeCall(SigOccurrence + SizeOfMovInstructionBytes);

		if (!Platform::IsAddressInProcessRange(PossibleConstructorAddress))
			continue;

		for (int i = 0; i < InitSRWLockSearchRange; i++)
		{
			/* Check for a relative call with the opcodes FF 15 00 00 00 00 */
			if (MemoryAccessor::Read<uint16>(PossibleConstructorAddress + i) != 0x15FF)
				continue;

			const uintptr_t RelativeCallTarget = Architecture_x86_64::Resolve32BitSectionRelativeCall(PossibleConstructorAddress + i);

			if (!Platform::IsAddressInProcessRange(RelativeCallTarget))
				continue;

			const uintptr_t ValueOfCallTarget = MemoryAccessor::Read<uintptr_t>(RelativeCallTarget);

			if (ValueOfCallTarget != InitSRWLockAddress && ValueOfCallTarget != RtlInitSRWLockAddress)
				continue;

			/* Try to find the "ByteProperty" string, as it's always referenced in FNamePool::FNamePool, so we use it to verify that we got the right function */
			const void* StringRef = Platform::FindByStringInAllSections(L"ByteProperty", PossibleConstructorAddress, BytePropertySearchRange, Settings::General::bSearchOnlyExecutableSectionsForStrings);

			/* We couldn't find a wchar_t string L"ByteProperty", now see if we can find a char string "ByteProperty" */
			if (StringRef == nullptr)
				StringRef = Platform::FindByStringInAllSections("ByteProperty", PossibleConstructorAddress, BytePropertySearchRange, Settings::General::bSearchOnlyExecutableSectionsForStrings);

			if (StringRef)
			{
				NamePoolIntance = reinterpret_cast<void*>(Architecture_x86_64::Resolve32BitRelativeMove(SigOccurrence));
				break;
			}
		}
	}

	if (NamePoolIntance)
	{
		Off::InSDK::NameArray::GNames = Platform::GetOffset(NamePoolIntance);
		return true;
	}

	return false;

#endif // PLATFORM_WINDOWS
}

bool NameArray::TryInit(bool bIsTestOnly)
{
	const uintptr_t ImageBase = Platform::GetModuleBase();

	uint8* GNamesAddress = nullptr;

	bool bFoundNameArray = false;
	bool bFoundnamePool = false;

	if (CALL_PLATFORM_SPECIFIC_FUNCTION(NameArray::TryFindNameArray))
	{
		std::cerr << std::format("Found 'TNameEntryArray GNames' at offset 0x{:X}\n", Off::InSDK::NameArray::GNames) << std::endl;
		GNamesAddress = MemoryAccessor::Read<uint8*>(ImageBase + Off::InSDK::NameArray::GNames);// Derefernce
		Settings::Internal::bUseNamePool = false;
		bFoundNameArray = true;
	}
	else if (CALL_PLATFORM_SPECIFIC_FUNCTION(NameArray::TryFindNamePool))
	{
		std::cerr << std::format("Found 'FNamePool GNames' at offset 0x{:X}\n", Off::InSDK::NameArray::GNames) << std::endl;
		GNamesAddress = reinterpret_cast<uint8*>(ImageBase + Off::InSDK::NameArray::GNames); // No derefernce
		Settings::Internal::bUseNamePool = true;
		bFoundnamePool = true;
	}

	if (!bFoundNameArray && !bFoundnamePool)
	{
		std::cerr << "\n\nCould not find GNames!\n\n" << std::endl;
		return false;
	}

	if (bIsTestOnly)
		return false;

	if (bFoundNameArray && NameArray::InitializeNameArray(GNamesAddress))
	{
		GNames = GNamesAddress;
		Settings::Internal::bUseNamePool = false;
		FNameEntry::Init();
		return true;
	}
	else if (bFoundnamePool && NameArray::InitializeNamePool(reinterpret_cast<uint8_t*>(GNamesAddress)))
	{
		GNames = GNamesAddress;
		Settings::Internal::bUseNamePool = true;
		/* FNameEntry::Init() was moved into NameArray::InitializeNamePool to avoid duplicated logic */
		return true;
	}

	std::cerr << "The address that was found couldn't be used by the generator, this might be due to GNames-encryption.\n" << std::endl;

	return false;
}


bool NameArray::TryInit(int32 OffsetOverride, bool bIsNamePool, const char* const ModuleName)
{
	const uintptr_t ImageBase = Platform::GetModuleBase(ModuleName);

	uint8* GNamesAddress = nullptr;

	const bool bIsNameArrayOverride = !bIsNamePool;
	const bool bIsNamePoolOverride = bIsNamePool;

	bool bFoundNameArray = false;
	bool bFoundnamePool = false;

	Off::InSDK::NameArray::GNames = OffsetOverride;

	if (bIsNameArrayOverride)
	{
		std::cerr << std::format("Overwrote offset: 'TNameEntryArray GNames' set as offset 0x{:X}\n", Off::InSDK::NameArray::GNames) << std::endl;
		GNamesAddress = MemoryAccessor::Read<uint8*>(ImageBase + Off::InSDK::NameArray::GNames);// Derefernce
		Settings::Internal::bUseNamePool = false;
		bFoundNameArray = true;
	}
	else if (bIsNamePoolOverride)
	{
		std::cerr << std::format("Overwrote offset: 'FNamePool GNames' set as offset 0x{:X}\n", Off::InSDK::NameArray::GNames) << std::endl;
		GNamesAddress = reinterpret_cast<uint8*>(ImageBase + Off::InSDK::NameArray::GNames); // No derefernce
		Settings::Internal::bUseNamePool = true;
		bFoundnamePool = true;
	}

	if (!bFoundNameArray && !bFoundnamePool)
	{
		std::cerr << "\n\nCould not find GNames!\n\n" << std::endl;
		return false;
	}

	if (bFoundNameArray && NameArray::InitializeNameArray(GNamesAddress))
	{
		GNames = GNamesAddress;
		Settings::Internal::bUseNamePool = false;
		FNameEntry::Init();
		return true;
	}
	else if (bFoundnamePool && NameArray::InitializeNamePool(reinterpret_cast<uint8_t*>(GNamesAddress)))
	{
		GNames = GNamesAddress;
		Settings::Internal::bUseNamePool = true;
		/* FNameEntry::Init() was moved into NameArray::InitializeNamePool to avoid duplicated logic */
		return true;
	}

	std::cerr << "The address was overwritten, but couldn't be used. This might be due to GNames-encryption.\n" << std::endl;

	return false;
}

bool NameArray::SetGNamesWithoutCommitting()
{
	/* GNames is already set */
	if (Off::InSDK::NameArray::GNames != 0x0)
		return false;

	if (CALL_PLATFORM_SPECIFIC_FUNCTION(NameArray::TryFindNameArray))
	{
		std::cerr << std::format("Found 'TNameEntryArray GNames' at offset 0x{:X}\n", Off::InSDK::NameArray::GNames) << std::endl;
		Settings::Internal::bUseNamePool = false;
		return true;
	}
	else if (CALL_PLATFORM_SPECIFIC_FUNCTION(NameArray::TryFindNamePool))
	{
		std::cerr << std::format("Found 'FNamePool GNames' at offset 0x{:X}\n", Off::InSDK::NameArray::GNames) << std::endl;
		Settings::Internal::bUseNamePool = true;
		return true;
	}

	std::cerr << "\n\nCould not find GNames!\n\n" << std::endl;
	return false;
}

void NameArray::PostInit()
{
	if (GNames && Settings::Internal::bUseNamePool)
	{
		// Reverse-order iteration because newer objects are more likely to have a chunk-index equal to NumChunks - 1
		
		NameArray::FNameBlockOffsetBits = 0xE;

		int i = ObjectArray::Num();
		while (i >= 0)
		{
			const int32 CurrentBlock = NameArray::GetNumChunks();

			UEObject Obj = ObjectArray::GetByIndex(i);

			if (!Obj)
			{
				i--;
				continue;
			}

			const int32 ObjNameChunkIdx = Obj.GetFName().GetCompIdx() >> NameArray::FNameBlockOffsetBits;

			if (ObjNameChunkIdx == CurrentBlock)
				break;

			if (ObjNameChunkIdx > CurrentBlock)
			{
				NameArray::FNameBlockOffsetBits++;
				i = ObjectArray::Num();
			}

			i--;
		}
		Off::InSDK::NameArray::FNamePoolBlockOffsetBits = NameArray::FNameBlockOffsetBits;

		std::cerr << "NameArray::FNameBlockOffsetBits: 0x" << std::hex << NameArray::FNameBlockOffsetBits << "\n" << std::endl;
	}
}

int32 NameArray::GetNumChunks()
{
	return MemoryAccessor::Read<int32>(reinterpret_cast<uintptr_t>(GNames + Off::NameArray::MaxChunkIndex));
}

int32 NameArray::GetNumElements()
{
	return !Settings::Internal::bUseNamePool ? MemoryAccessor::Read<int32>(reinterpret_cast<uintptr_t>(GNames + Off::NameArray::NumElements)) : 0;
}

int32 NameArray::GetByteCursor()
{
	return Settings::Internal::bUseNamePool ? MemoryAccessor::Read<int32>(reinterpret_cast<uintptr_t>(GNames + Off::NameArray::ByteCursor)) : 0;
}

FNameEntry NameArray::GetNameEntry(const void* Name)
{
	return ByIndex(GNames, FName(Name).GetCompIdx(), FNameBlockOffsetBits);
}

FNameEntry NameArray::GetNameEntry(int32 Idx)
{
	return ByIndex(GNames, Idx, FNameBlockOffsetBits);
}

