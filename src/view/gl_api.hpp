#pragma once
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#define CURVED_GL_FUNCTIONS(X)                                                                               \
    X(PFNGLCREATESHADERPROC, CreateShader)                                                                   \
    X(PFNGLSHADERSOURCEPROC, ShaderSource) X(PFNGLCOMPILESHADERPROC, CompileShader)                          \
        X(PFNGLGETSHADERIVPROC, GetShaderiv) X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog)                  \
            X(PFNGLDELETESHADERPROC, DeleteShader) X(PFNGLCREATEPROGRAMPROC,                                 \
                                                     CreateProgram) X(PFNGLATTACHSHADERPROC, AttachShader)   \
                X(PFNGLLINKPROGRAMPROC, LinkProgram) X(PFNGLGETPROGRAMIVPROC, GetProgramiv) X(               \
                    PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog) X(PFNGLDELETEPROGRAMPROC, DeleteProgram)  \
                    X(PFNGLUSEPROGRAMPROC, UseProgram) X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation)    \
                        X(PFNGLUNIFORM4FVPROC, Uniform4fv) X(PFNGLUNIFORM3FPROC, Uniform3f)                  \
                            X(PFNGLUNIFORM2FPROC, Uniform2f) X(PFNGLUNIFORM1FPROC, Uniform1f) X(             \
                                PFNGLGENBUFFERSPROC, GenBuffers) X(PFNGLBINDBUFFERPROC, BindBuffer)          \
                                X(PFNGLBUFFERDATAPROC, BufferData) X(PFNGLBUFFERSUBDATAPROC, BufferSubData)  \
                                    X(PFNGLDELETEBUFFERSPROC, DeleteBuffers) X(PFNGLGENVERTEXARRAYSPROC,     \
                                                                               GenVertexArrays)              \
                                        X(PFNGLBINDVERTEXARRAYPROC, BindVertexArray)                         \
                                            X(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays)               \
                                                X(PFNGLENABLEVERTEXATTRIBARRAYPROC, EnableVertexAttribArray) \
                                                    X(PFNGLVERTEXATTRIBPOINTERPROC, VertexAttribPointer) X(  \
                                                        PFNGLVERTEXATTRIBDIVISORPROC, VertexAttribDivisor)   \
                                                        X(PFNGLDRAWARRAYSINSTANCEDPROC, DrawArraysInstanced)
namespace curved::gl {
#define DECLARE(type, name) extern type name;
CURVED_GL_FUNCTIONS(DECLARE)
#undef DECLARE
void load();
} // namespace curved::gl
