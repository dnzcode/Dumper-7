#include "PatternScan.h"
#include <cstring>
#include <cctype>

namespace PatternScan
{
    bool ParsePattern(const char* pattern, std::vector<uint8_t>& outBytes, std::vector<bool>& outMask)
    {
        if (pattern == nullptr)
        {
            return false;
        }

        outBytes.clear();
        outMask.clear();

        const char* p = pattern;
        while (*p)
        {
            // Skip whitespace
            while (*p && isspace(*p))
            {
                p++;
            }

            if (!*p)
            {
                break;
            }

            // Check for wildcard
            if (*p == '?')
            {
                outBytes.push_back(0);
                outMask.push_back(false);
                p++;
                
                // Handle double wildcard "??"
                if (*p == '?')
                {
                    p++;
                }
            }
            else if (isxdigit(*p))
            {
                // Parse hex byte
                char byte[3] = { 0 };
                byte[0] = *p++;
                
                if (*p && isxdigit(*p))
                {
                    byte[1] = *p++;
                }
                
                outBytes.push_back(static_cast<uint8_t>(strtoul(byte, nullptr, 16)));
                outMask.push_back(true);
            }
            else
            {
                // Invalid character
                return false;
            }
        }

        return !outBytes.empty();
    }

    int64_t FindPatternInBuffer(
        const uint8_t* buffer,
        size_t bufferSize,
        const std::vector<uint8_t>& patternBytes,
        const std::vector<bool>& patternMask)
    {
        if (buffer == nullptr || bufferSize == 0 || patternBytes.empty())
        {
            return -1;
        }

        const size_t patternSize = patternBytes.size();
        if (bufferSize < patternSize)
        {
            return -1;
        }

        // Search for pattern
        for (size_t i = 0; i <= bufferSize - patternSize; i++)
        {
            bool found = true;
            
            for (size_t j = 0; j < patternSize; j++)
            {
                if (patternMask[j] && buffer[i + j] != patternBytes[j])
                {
                    found = false;
                    break;
                }
            }

            if (found)
            {
                return static_cast<int64_t>(i);
            }
        }

        return -1;
    }

    uintptr_t ResolveRelativeAddress(uintptr_t instructionAddress, int32_t instructionSize, int32_t relativeOffset)
    {
        // RIP-relative addressing: RIP + instruction_size + relative_offset_value
        // The relative offset value is typically at offset 3 in the instruction
        return instructionAddress + instructionSize + relativeOffset;
    }
}
