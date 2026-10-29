#pragma once

#include <cstdint>
#include <string>

namespace lvs {

// Keep intervals as whole milliseconds to avoid rounding drift at preset boundaries.
// Either '.' or ',' is accepted as the decimal separator, independently of the OS locale.
// Precision is limited to milliseconds and the supported range is 0.01 - 3600 seconds.
inline bool parseRandomPresetIntervalMs(const std::string& text, uint64_t& milliseconds)
{
    uint64_t seconds = 0;
    uint64_t fraction = 0;
    unsigned int fractionDigits = 0;
    bool separatorSeen = false;
    bool digitSeen = false;

    for (char ch : text) {
        if (ch >= '0' && ch <= '9') {
            digitSeen = true;
            if (separatorSeen) {
                if (++fractionDigits > 3) {
                    return false;
                }
                fraction = fraction * 10 + static_cast<unsigned int>(ch - '0');
            } else {
                seconds = seconds * 10 + static_cast<unsigned int>(ch - '0');
                if (seconds > 3600) {
                    return false;
                }
            }
        } else if ((ch == '.' || ch == ',') && !separatorSeen) {
            separatorSeen = true;
        } else {
            return false;
        }
    }

    if (!digitSeen || (separatorSeen && fractionDigits == 0)) {
        return false;
    }

    while (fractionDigits++ < 3) {
        fraction *= 10;
    }

    const uint64_t result = seconds * 1000 + fraction;
    if (result < 10 || result > 3600000) {
        return false;
    }

    milliseconds = result;
    return true;
}

inline std::string formatRandomPresetIntervalMs(uint64_t milliseconds)
{
    std::string result = std::to_string(milliseconds / 1000);
    uint64_t fraction = milliseconds % 1000;
    if (fraction == 0) {
        return result;
    }

    std::string digits = std::to_string(fraction);
    digits.insert(0, 3 - digits.size(), '0');
    while (digits.back() == '0') {
        digits.pop_back();
    }

    return result + "." + digits;
}

} // namespace lvs
