#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
#include <cstddef>
#include <string>

// Windows SDK gl.h stops at 1.1. No extension entry points are link imports.
#if __has_include(<GL/glext.h>)
#include <GL/glext.h>
#else
using GLchar = char;
using GLintptr = std::ptrdiff_t;
using GLsizeiptr = std::ptrdiff_t;
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_TEXTURE4 0x84C4
#define GL_TEXTURE5 0x84C5
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_RGBA8 0x8058
#define GL_BGRA 0x80E1
#define GL_FUNC_ADD 0x8006
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_RENDERBUFFER 0x8D41
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C
#define GL_NUM_EXTENSIONS 0x821D
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_CLAMP_VERTEX_COLOR 0x891A
#define GL_CLAMP_FRAGMENT_COLOR 0x891B
#define GL_CLAMP_READ_COLOR 0x891C
#define GL_FIXED_ONLY 0x891D
#define GL_RGBA8UI 0x8D7C
#define GL_RGBA_INTEGER 0x8D99
#define GL_RG 0x8227
#define GL_RG32UI 0x823C
#define GL_RG_INTEGER 0x8228
#define GL_R8 0x8229
#define GL_RGBA16F 0x881A
#define GL_RGBA32F 0x8814
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_RED_INTEGER 0x8D94
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#define GL_PIXEL_PACK_BUFFER 0x88EB
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif

#ifndef GL_ONE_MINUS_SRC1_ALPHA
#define GL_ONE_MINUS_SRC1_ALPHA 0x88FB
#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS 0x88FC
#endif

#ifndef GL_ARB_sync
struct __GLsync;
using GLsync = __GLsync*;
using GLuint64 = unsigned long long;
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define GL_ALREADY_SIGNALED 0x911A
#define GL_TIMEOUT_EXPIRED 0x911B
#define GL_CONDITION_SATISFIED 0x911C
#define GL_WAIT_FAILED 0x911D
#endif

namespace Sexy::GL3 {
// Only desktop GL <= 3.0 entry points belong in this required function table.
#define SEXY_GL3_FUNCTIONS(X) \
    X(void, glActiveTexture, (GLenum)) \
    X(void, glGenBuffers, (GLsizei, GLuint*)) \
    X(void, glDeleteBuffers, (GLsizei, const GLuint*)) \
    X(void, glBindBuffer, (GLenum, GLuint)) \
    X(void, glBufferData, (GLenum, GLsizeiptr, const void*, GLenum)) \
    X(void, glBufferSubData, (GLenum, GLintptr, GLsizeiptr, const void*)) \
    X(void*, glMapBufferRange, (GLenum, GLintptr, GLsizeiptr, GLbitfield)) \
    X(GLboolean, glUnmapBuffer, (GLenum)) \
    X(void, glGenVertexArrays, (GLsizei, GLuint*)) \
    X(void, glDeleteVertexArrays, (GLsizei, const GLuint*)) \
    X(void, glBindVertexArray, (GLuint)) \
    X(void, glEnableVertexAttribArray, (GLuint)) \
    X(void, glDisableVertexAttribArray, (GLuint)) \
    X(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
    X(void, glVertexAttribIPointer, (GLuint, GLint, GLenum, GLsizei, const void*)) \
    X(GLuint, glCreateShader, (GLenum)) \
    X(void, glShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
    X(void, glCompileShader, (GLuint)) \
    X(void, glGetShaderiv, (GLuint, GLenum, GLint*)) \
    X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
    X(void, glDeleteShader, (GLuint)) \
    X(GLuint, glCreateProgram, ()) \
    X(void, glAttachShader, (GLuint, GLuint)) \
    X(void, glDetachShader, (GLuint, GLuint)) \
    X(void, glBindAttribLocation, (GLuint, GLuint, const GLchar*)) \
    X(void, glBindFragDataLocation, (GLuint, GLuint, const GLchar*)) \
    X(void, glLinkProgram, (GLuint)) \
    X(void, glGetProgramiv, (GLuint, GLenum, GLint*)) \
    X(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
    X(void, glUseProgram, (GLuint)) \
    X(void, glDeleteProgram, (GLuint)) \
    X(GLint, glGetUniformLocation, (GLuint, const GLchar*)) \
    X(void, glUniform1i, (GLint, GLint)) \
    X(void, glUniform1ui, (GLint, GLuint)) \
    X(void, glUniform2i, (GLint, GLint, GLint)) \
    X(void, glUniform4i, (GLint, GLint, GLint, GLint, GLint)) \
    X(void, glUniform1f, (GLint, GLfloat)) \
    X(void, glUniform2f, (GLint, GLfloat, GLfloat)) \
    X(void, glUniform3f, (GLint, GLfloat, GLfloat, GLfloat)) \
    X(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void, glUniform4fv, (GLint, GLsizei, const GLfloat*)) \
    X(void, glUniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat*)) \
    X(void, glGenFramebuffers, (GLsizei, GLuint*)) \
    X(void, glDeleteFramebuffers, (GLsizei, const GLuint*)) \
    X(void, glBindFramebuffer, (GLenum, GLuint)) \
    X(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(GLenum, glCheckFramebufferStatus, (GLenum)) \
    X(void, glBlitFramebuffer, (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)) \
    X(void, glGenerateMipmap, (GLenum)) \
    X(void, glBlendEquation, (GLenum)) \
    X(void, glBlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum)) \
    X(void, glClampColor, (GLenum, GLenum)) \
    X(void, glBlendEquationSeparate, (GLenum, GLenum)) \
    X(void, glDrawBuffers, (GLsizei, const GLenum*)) \
    X(void, glClearBufferfv, (GLenum, GLint, const GLfloat*)) \
    X(const GLubyte*, glGetStringi, (GLenum, GLuint))

#define SEXY_GL3_DECLARE(result, name, arguments) \
    using name##Proc = result (APIENTRY*) arguments; \
    extern name##Proc name;
SEXY_GL3_FUNCTIONS(SEXY_GL3_DECLARE)
#undef SEXY_GL3_DECLARE

using glBindFragDataLocationIndexedProc = void (APIENTRY*)(GLuint, GLuint, GLuint, const GLchar*);
extern glBindFragDataLocationIndexedProc glBindFragDataLocationIndexed;
// Core since 3.2 / ARB_sync. Optional for the retained GL 3.0 compatibility path.
using glFenceSyncProc = GLsync (APIENTRY*)(GLenum, GLbitfield);
using glClientWaitSyncProc = GLenum (APIENTRY*)(GLsync, GLbitfield, GLuint64);
using glDeleteSyncProc = void (APIENTRY*)(GLsync);
extern glFenceSyncProc glFenceSync;
extern glClientWaitSyncProc glClientWaitSync;
extern glDeleteSyncProc glDeleteSync;

// Requires a current context. Reload for each context: WGL addresses are ICD-specific.
void Load();
PROC GetProcAddress(const char* name);
bool HasExtension(const char* name);
void CheckError(const char* operation);
}
