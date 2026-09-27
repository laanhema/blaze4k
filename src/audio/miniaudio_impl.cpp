#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wpedantic"

// Ogg Vorbis support: miniaudio only enables its Vorbis backend when
// stb_vorbis' declarations are visible, and it expects the stb_vorbis
// implementation to be compiled in the same translation unit. Without this,
// every .ogg simfile fails to load with MA_INVALID_FILE (-10).
#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#undef STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"

#pragma GCC diagnostic pop
