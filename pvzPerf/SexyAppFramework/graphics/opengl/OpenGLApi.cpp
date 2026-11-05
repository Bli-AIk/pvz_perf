#include "OpenGLApi.h"
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace Sexy::GL3 {
#define SEXY_GL3_DEFINE(result, name, arguments) name##Proc name = nullptr;
SEXY_GL3_FUNCTIONS(SEXY_GL3_DEFINE)
#undef SEXY_GL3_DEFINE
glBindFragDataLocationIndexedProc glBindFragDataLocationIndexed = nullptr;
glFenceSyncProc glFenceSync = nullptr;
glClientWaitSyncProc glClientWaitSync = nullptr;
glDeleteSyncProc glDeleteSync = nullptr;

PROC GetProcAddress(const char* name)
{
    auto address = wglGetProcAddress(name);
    const auto value = reinterpret_cast<std::intptr_t>(address);
    if (!address || value == 1 || value == 2 || value == 3 || value == -1) {
        // Keep opengl32 loaded for the lifetime of all function pointers.
        static HMODULE library = LoadLibraryW(L"opengl32.dll");
        if (!library) throw std::runtime_error("Cannot load opengl32.dll");
        address = ::GetProcAddress(library, name);
    }
    return address;
}

void Load()
{
    if (!wglGetCurrentContext()) throw std::logic_error("OpenGL loader requires a current context");
#define SEXY_GL3_LOAD(result, name, arguments) \
    name = reinterpret_cast<name##Proc>(GetProcAddress(#name)); \
    if (!name) throw std::runtime_error("OpenGL 3.0 entry point missing: " #name);
    SEXY_GL3_FUNCTIONS(SEXY_GL3_LOAD)
    glBindFragDataLocationIndexed = reinterpret_cast<glBindFragDataLocationIndexedProc>(GetProcAddress("glBindFragDataLocationIndexed"));
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    const bool sync = major > 3 || (major == 3 && minor >= 2) || HasExtension("GL_ARB_sync");
    glFenceSync = sync ? reinterpret_cast<glFenceSyncProc>(GetProcAddress("glFenceSync")) : nullptr;
    glClientWaitSync = sync ? reinterpret_cast<glClientWaitSyncProc>(GetProcAddress("glClientWaitSync")) : nullptr;
    glDeleteSync = sync ? reinterpret_cast<glDeleteSyncProc>(GetProcAddress("glDeleteSync")) : nullptr;
    if (sync && (!glFenceSync || !glClientWaitSync || !glDeleteSync))
        throw std::runtime_error("Advertised OpenGL sync entry point missing");
#undef SEXY_GL3_LOAD
}

bool HasExtension(const char* name)
{
    if (!name || !*name || std::strchr(name, ' ')) return false;
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint i = 0; i < count; ++i) {
        const auto text = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, GLuint(i)));
        if (text && std::strcmp(name, text) == 0) return true;
    }
    return false;
}

void CheckError(const char* operation)
{
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR)
        throw std::runtime_error(std::string(operation) + ": OpenGL error " + std::to_string(error));
}
}
