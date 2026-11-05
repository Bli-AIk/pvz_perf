#pragma once

#include <string_view>

namespace Sexy::GL3 {

std::string_view SceneVertexShader();
std::string_view PoolVertexShader();
std::string_view SceneFragmentShader();
std::string_view SpriteFragmentShader();
std::string_view MixedSpriteFragmentShader();
std::string_view QuadVertexShader();
std::string_view ImageFragmentShader();
std::string_view FontFragmentShader();
std::string_view PresentFragmentShader();

}
