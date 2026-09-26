// Single translation unit that instantiates the vendored stb_image decoder.
// Every other file only includes <stb_image.h> for declarations.
//
// Defense in depth for untrusted images: cap the decoder's own dimension limit
// at the same 4096px ceiling used by Texture::from_file. stb rejects oversized
// headers before allocating pixel memory, so even a missed pre-decode probe
// cannot trigger a decompression-bomb allocation.
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
