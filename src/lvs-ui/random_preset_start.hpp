#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace lvs {

// Clamp timestamps before the playback origin, including backward seeks.
inline uint64_t randomPresetSlot(uint64_t timestampMs, uint64_t startMs, uint64_t intervalMs)
{
    return (timestampMs > startMs ? timestampMs - startMs : 0) / intervalMs;
}

// Preserve the shuffled permutation while making the chosen preset first.
inline void prioritizeRandomPreset(std::vector<size_t>& order, size_t index)
{
    const auto it = std::find(order.begin(), order.end(), index);
    if (it != order.end()) {
        std::iter_swap(order.begin(), it);
    }
}

} // namespace lvs
