#pragma once

#include <cstdint>
#include <vector>
#include <string>

/**
 * PatternScan - Utilities for pattern scanning in remote process memory
 */
namespace PatternScan
{
    /**
     * Parse a pattern string into bytes and mask
     * Pattern format: "48 8B 05 ? ? ? ? 48 85 C0"
     * Where ? represents a wildcard byte
     * 
     * @param pattern The pattern string
     * @param outBytes Output vector for pattern bytes
     * @param outMask Output vector for pattern mask (true = match, false = wildcard)
     * @return true if pattern was successfully parsed, false otherwise
     */
    bool ParsePattern(const char* pattern, std::vector<uint8_t>& outBytes, std::vector<bool>& outMask);

    /**
     * Search for a pattern in a buffer
     * 
     * @param buffer The buffer to search in
     * @param bufferSize Size of the buffer
     * @param patternBytes The pattern bytes
     * @param patternMask The pattern mask
     * @return Offset within buffer where pattern was found, or -1 if not found
     */
    int64_t FindPatternInBuffer(
        const uint8_t* buffer, 
        size_t bufferSize,
        const std::vector<uint8_t>& patternBytes,
        const std::vector<bool>& patternMask
    );

    /**
     * Resolve a relative address from an instruction
     * Used for resolving RIP-relative addresses in x64 code
     * 
     * @param instructionAddress The address of the instruction
     * @param instructionSize Size of the instruction (usually 3-7 bytes)
     * @param relativeOffset Offset of the relative value within the instruction
     * @return The absolute address
     */
    uintptr_t ResolveRelativeAddress(uintptr_t instructionAddress, int32_t instructionSize, int32_t relativeOffset = 3);

    /**
     * Common pattern signatures for Unreal Engine
     */
    namespace Signatures
    {
        // GObjects signatures for different UE versions
        constexpr const char* GOBJECTS_UE4_CHUNKED = "48 8B 05 ? ? ? ? 48 8B 0C C8 48 8D 04 D1";
        constexpr const char* GOBJECTS_UE4_FIXED = "48 8B 05 ? ? ? ? 48 8B 0C D0";
        constexpr const char* GOBJECTS_UE5 = "48 8B 05 ? ? ? ? 48 85 C0 74 ? 8B 14 88";
        
        // GNames signatures
        constexpr const char* GNAMES_NAMEPOOL = "48 8D 0D ? ? ? ? E8 ? ? ? ? C6 05 ? ? ? ? 01";
        constexpr const char* GNAMES_ARRAY = "48 8B 05 ? ? ? ? 48 85 C0 75 ? B9";
        
        // ProcessEvent signatures
        constexpr const char* PROCESS_EVENT = "40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC";
    }
}
