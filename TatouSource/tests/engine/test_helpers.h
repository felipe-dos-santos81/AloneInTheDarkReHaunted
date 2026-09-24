///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: shared fixtures.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <array>
#include <cmath>
#include <cstdint>

// Same layout as the engine's cosTable (FitdLib/cosTable.cpp): 1024 entries,
// table[i] = sin(i * 2pi / 1024) * 32767, so table[256] == 32767.
inline const int16_t* testCosTable()
{
    static const std::array<int16_t, 1024> table = [] {
        std::array<int16_t, 1024> t{};
        for (int i = 0; i < 1024; ++i)
            t[i] = (int16_t)std::lround(std::sin(i * 6.283185307179586 / 1024.0) * 32767.0);
        return t;
    }();
    return table.data();
}
