///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: shared fixtures.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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

// The engine's own table, read from FitdLib/cosTable.cpp (FITD_SOURCE_DIR),
// every number between its braces. It is not testCosTable(): entry 0 is 4,
// so the model pose tests, which must match the engine exactly, use this one.
inline const std::vector<int16_t>& engineCosTableEntries()
{
    static const std::vector<int16_t> table = [] {
        std::ifstream in(FITD_SOURCE_DIR "/cosTable.cpp");
        std::stringstream text;
        text << in.rdbuf();
        const std::string s = text.str();
        const size_t open = s.find('{');
        const size_t close = s.find('}', open);
        std::vector<int16_t> t;
        if (open == std::string::npos || close == std::string::npos)
            return t;
        const char* p = s.c_str() + open + 1;
        const char* end = s.c_str() + close;
        while (p < end)
        {
            char* next = nullptr;
            const long v = std::strtol(p, &next, 10);
            if (next == p)
            {
                ++p;
                continue;
            }
            t.push_back((int16_t)v);
            p = next;
        }
        return t;
    }();
    return table;
}

inline const int16_t* engineCosTable()
{
    return engineCosTableEntries().data();
}
