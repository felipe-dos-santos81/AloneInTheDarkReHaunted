///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: the never-lock-the-cursor rule (AGENTS.md).
///////////////////////////////////////////////////////////////////////////////

#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// AGENTS.md rule 1: never lock, grab, confine or warp the OS cursor, and never
// turn off SDL's default auto-capture. Project code must not even name these,
// under FitdLib/ or Fitd/.
TEST_CASE("no FitdLib or Fitd source locks, grabs, confines or warps the OS cursor")
{
    const char* banned[] = {
        "SDL_SetWindowMouseGrab",
        "SDL_SetWindowRelativeMouseMode",
        "SDL_SetWindowMouseRect",
        "SDL_WarpMouseInWindow",
        "SDL_WarpMouseGlobal",
        "SDL_CaptureMouse",
        "SDL_HINT_MOUSE_AUTO_CAPTURE",
    };

    const fs::path roots[] = { fs::path(FITD_SOURCE_DIR), fs::path(FITD_EXE_DIR) };
    for (const auto& root : roots)
        REQUIRE(fs::is_directory(root));

    std::vector<std::string> offenders;
    for (const auto& root : roots)
        for (const auto& entry : fs::recursive_directory_iterator(root))
        {
            if (!entry.is_regular_file())
                continue;
            const std::string ext = entry.path().extension().string();
            if (ext != ".cpp" && ext != ".c" && ext != ".h" && ext != ".hpp" && ext != ".mm")
                continue;
            std::ifstream in(entry.path(), std::ios::binary);
            std::stringstream buffer;
            buffer << in.rdbuf();
            const std::string text = buffer.str();
            for (const char* name : banned)
            {
                if (text.find(name) != std::string::npos)
                    offenders.push_back(entry.path().string() + ": " + name);
            }
        }

    for (const auto& offender : offenders)
        MESSAGE(offender);
    CHECK(offenders.empty());
}
