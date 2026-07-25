// stb_image is a single-header library: its implementation lives behind
// STB_IMAGE_IMPLEMENTATION and must be compiled in exactly one translation unit.
// Keeping it here rather than in main.cpp means no other file has to care about
// include order — note the implementation block sits outside stb_image.h's include
// guard, so defining the macro alongside any other include of the header would
// compile the implementation twice.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
