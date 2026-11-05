#pragma once
#include <GLES3/gl3.h>
#include <string>
#include <string_view>
#define GL_BGRA 0x80E1
#define GL_CLAMP_READ_COLOR 0x891C
#define GL_CLAMP_FRAGMENT_COLOR 0x891B
#define GL_ONE_MINUS_SRC1_ALPHA 0x88FB
namespace Sexy::GL3 {
void CheckError(const char* operation);
std::string WebShaderSource(std::string_view source);
}
