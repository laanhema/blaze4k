// Single translation unit that instantiates the vendored stb_rect_pack packer
// and stb_truetype rasterizer (#90). Every other file only includes the headers
// for declarations.
//
// stb_truetype is not hardened ("NO SECURITY GUARANTEE -- DO NOT USE THIS ON
// UNTRUSTED FONT FILES"). The fonts are bundled, not user-supplied, and
// FontFace (render/ttf_font.cpp) validates the sfnt table directory (magic,
// table count, every table extent, required tables, unitsPerEm) before any stb
// call, so a missing, truncated or garbage .ttf is rejected instead of crashing.
//
// stb_rect_pack comes first so stb_truetype uses the real skyline packer
// instead of its built-in fallback.
#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
