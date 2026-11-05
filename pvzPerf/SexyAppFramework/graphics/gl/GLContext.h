#pragma once
#if defined(PVZ_GPU_WEBGL)
#include "graphics/webgl/WebGLContext.h"
namespace Sexy { using GLContext = WebGLContext; }
#else
#include "graphics/opengl/OpenGLContext.h"
namespace Sexy { using GLContext = OpenGLContext; }
#endif
