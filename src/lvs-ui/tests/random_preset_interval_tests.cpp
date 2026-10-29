#include "../random_preset_interval.hpp"

#include <cstdint>
#include <iostream>
#include <string>

int main()
{
    struct TestCase {
        const char* text;
        bool valid;
        uint64_t milliseconds;
    };

    const TestCase cases[] = {
        {"4.5", true, 4500},
        {"4,5", true, 4500},
        {"3.75", true, 3750},
        {"3,75", true, 3750},
        {"4.125", true, 4125},
        {"4,125", true, 4125},
        {"5", true, 5000},
        {"0.01", true, 10},
        {"0,010", true, 10},
        {"3600", true, 3600000},
        {"3600.000", true, 3600000},
        {".5", true, 500},
        {",5", true, 500},
        {"0004,50", true, 4500},
        {"", false, 0},
        {".", false, 0},
        {",", false, 0},
        {"4.", false, 0},
        {"4,", false, 0},
        {"4.5.5", false, 0},
        {"4,5.5", false, 0},
        {"4,5,5", false, 0},
        {"4.1234", false, 0},
        {"0", false, 0},
        {"0.001", false, 0},
        {"3600.001", false, 0},
        {"3601", false, 0},
        {"-4.5", false, 0},
        {"4e1", false, 0},
        {"NaN", false, 0},
        {"4 5", false, 0},
        {" 4.5", false, 0},
        {"4.5 ", false, 0}
    };

    for (const auto& test : cases) {
        uint64_t actual = 12345;
        const bool success = lvs::parseRandomPresetIntervalMs(test.text, actual);
        if (success != test.valid || (success && actual != test.milliseconds) ||
            (!success && actual != 12345)) {
            std::cerr << "Unexpected parse result for: " << test.text << std::endl;
            return 1;
        }
    }

    const struct { uint64_t ms; const char* text; } formats[] = {
        {10, "0.01"},
        {500, "0.5"},
        {3750, "3.75"},
        {4500, "4.5"},
        {4125, "4.125"},
        {5000, "5"},
        {3600000, "3600"}
    };

    for (const auto& test : formats) {
        if (lvs::formatRandomPresetIntervalMs(test.ms) != test.text) {
            std::cerr << "Unexpected formatting for: " << test.ms << std::endl;
            return 1;
        }
    }

    return 0;
}
