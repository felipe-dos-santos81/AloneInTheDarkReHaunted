// stb_image_write for the engine's PNG dumps. Each includer defines
// STB_IMAGE_WRITE_STATIC and STB_IMAGE_WRITE_IMPLEMENTATION first.
// stb's HDR writer calls sprintf, which macOS deprecates: silence that here
// once rather than at every include.
#pragma once

#if defined(__GNUC__) // clang too
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image_write.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
