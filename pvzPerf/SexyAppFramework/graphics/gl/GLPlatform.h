#pragma once
#if defined(PVZ_GPU_WEBGL)
#include "graphics/webgl/WebGLPlatform.h"
namespace Sexy { using GLPlatform = WebGLPlatform; }
#else
#include "graphics/opengl/OpenGLPlatform.h"
namespace Sexy { using GLPlatform = OpenGLPlatform; }
#endif
