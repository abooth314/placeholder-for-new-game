#include "view/gl_api.hpp"
#include <SDL3/SDL.h>
#include <stdexcept>
namespace curved::gl {
#define DEFINE(type, name) type name = nullptr;
CURVED_GL_FUNCTIONS(DEFINE)
#undef DEFINE
void load() {
#define LOAD(type, name)                                                                                     \
    name = reinterpret_cast<type>(SDL_GL_GetProcAddress("gl" #name));                                        \
    if (!name)                                                                                               \
        throw std::runtime_error("Missing OpenGL entry: gl" #name);
    CURVED_GL_FUNCTIONS(LOAD)
#undef LOAD
}
} // namespace curved::gl
