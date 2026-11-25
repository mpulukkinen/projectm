#include "../random_preset_start.hpp"

#include <iostream>

int main()
{
    // Preview from a nonzero position and render from a session offset.
    for (const uint64_t start : {uint64_t{0}, uint64_t{12345}, uint64_t{900000}}) {
        if (lvs::randomPresetSlot(start, start, 4500) != 0 ||
            lvs::randomPresetSlot(start + 4499, start, 4500) != 0 ||
            lvs::randomPresetSlot(start + 4500, start, 4500) != 1 ||
            lvs::randomPresetSlot(start + 9000, start, 4500) != 2 ||
            lvs::randomPresetSlot(0, start, 4500) != 0) {
            std::cerr << "Unexpected playback slot\n";
            return 1;
        }
    }

    std::vector<size_t> order{3, 1, 0, 2};
    lvs::prioritizeRandomPreset(order, 0);
    if (order != std::vector<size_t>{0, 1, 3, 2}) {
        std::cerr << "Selected preset was not moved to the start\n";
        return 1;
    }
    lvs::prioritizeRandomPreset(order, 2);
    if (order != std::vector<size_t>{2, 1, 3, 0}) {
        std::cerr << "Changing the starting preset did not preserve the permutation\n";
        return 1;
    }
    lvs::prioritizeRandomPreset(order, 2);
    lvs::prioritizeRandomPreset(order, 99);
    if (order != std::vector<size_t>{2, 1, 3, 0}) {
        std::cerr << "Current or missing selection altered the order\n";
        return 1;
    }
    std::vector<size_t> empty;
    lvs::prioritizeRandomPreset(empty, 0);
    if (!empty.empty()) {
        return 1;
    }
    return 0;
}
