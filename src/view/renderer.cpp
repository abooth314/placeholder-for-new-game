#include "view/renderer.hpp"
#include "assets/mesh.hpp"
#include "core/path.hpp"
#include "view/gl_api.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <numbers>
#include <stdexcept>

namespace curved {
namespace {
constexpr const char *mesh_vs = R"GLSL(#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aColor;
layout(location=3) in float aMotion;
layout(location=4) in vec4 iPosition;
layout(location=5) in vec4 iEast;
layout(location=6) in vec4 iUp;
layout(location=7) in vec4 iNorth;
layout(location=8) in vec4 iTint;
layout(location=9) in vec4 iAnimation;
uniform vec2 uProjection,uDepth;
uniform float uL;
out vec3 vColor;
out float vFog;
void main(){
    vec3 local=aPosition;
    if(aMotion>.5&&aMotion<2.5){float a=sin(iAnimation.x)*iAnimation.y*(aMotion<1.5?.38:-.38);float y=local.y-.9;local.y=.9+y*cos(a)-local.z*sin(a);local.z=y*sin(a)+local.z*cos(a);}
    if(aMotion>2.5){float a=sin(clamp(iAnimation.z/.3,0.,1.)*3.14159265)*.8;float y=local.y-1.4,z=local.z-.2;local.y=1.4+y*cos(a)-z*sin(a);local.z=.2+y*sin(a)+z*cos(a);}
    local*=iAnimation.w;
    float len=length(local),r=len/uL;
    vec4 q=cosh(r)*iPosition+(len>.000001?sinh(r)/len:1./uL)*(iEast*local.x+iUp*local.y+iNorth*local.z);
    gl_Position=vec4(uProjection.x*q.y,uProjection.y*q.z,uDepth.x*q.w-uDepth.y*q.x,q.w);
    float light=.53+.47*max(0.,dot(normalize(aNormal),normalize(vec3(-.45,.85,.3))));
    vColor=mix(mix(aColor.rgb,iTint.rgb,aColor.a)*light,vec3(1.,.91,.7),min(.6,iTint.a*2.));
    vFog=smoothstep(55.,190.,uL*acosh(max(1.,q.x)));
})GLSL";
constexpr const char *terrain_vs = R"GLSL(#version 330 core
layout(location=0) in vec4 aPosition;
layout(location=1) in vec3 aColor;
uniform vec2 uProjection,uDepth;
uniform float uL;
out vec3 vColor;
out float vFog;
void main(){vec4 q=aPosition;gl_Position=vec4(uProjection.x*q.y,uProjection.y*q.z,uDepth.x*q.w-uDepth.y*q.x,q.w);vColor=aColor;vFog=smoothstep(65.,200.,uL*acosh(max(1.,q.x)));}
)GLSL";
constexpr const char *fragment = R"GLSL(#version 330 core
in vec3 vColor;
in float vFog;
out vec4 outColor;
void main(){outColor=vec4(mix(vColor,vec3(.63,.74,.70),vFog*.82),1.);}
)GLSL";
GLuint program(const char *vertex) {
    auto shader = [](GLenum type, const char *text) {
        const auto s = gl::CreateShader(type);
        gl::ShaderSource(s, 1, &text, nullptr);
        gl::CompileShader(s);
        GLint ok = 0;
        gl::GetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char error[4096]{};
            gl::GetShaderInfoLog(s, 4096, nullptr, error);
            gl::DeleteShader(s);
            throw std::runtime_error(std::string("Shader compilation: ") + error);
        }
        return s;
    };
    const auto v = shader(GL_VERTEX_SHADER, vertex), f = shader(GL_FRAGMENT_SHADER, fragment),
               p = gl::CreateProgram();
    gl::AttachShader(p, v);
    gl::AttachShader(p, f);
    gl::LinkProgram(p);
    gl::DeleteShader(v);
    gl::DeleteShader(f);
    GLint ok = 0;
    gl::GetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char error[4096]{};
        gl::GetProgramInfoLog(p, 4096, nullptr, error);
        gl::DeleteProgram(p);
        throw std::runtime_error(std::string("Shader linking: ") + error);
    }
    return p;
}
struct Instance {
    float data[24];
};
struct TerrainVertex {
    float t, x, y, z, r, g, b;
};
struct StaticVertex {
    h3::Vec point;
    std::array<float, 3> color;
};
struct Model {
    GLuint vao = 0, vertices = 0, instances = 0;
    int count = 0;
    std::vector<Instance> pending;
};
struct Prop {
    std::string mesh;
    h3::Vec point;
    double yaw, scale;
    int faction = -1;
};
static std::array<float, 4> local(h3::Vec p, const Camera &c) {
    return {static_cast<float>(-h3::dot(p, c.position)), static_cast<float>(h3::dot(p, c.right)),
            static_cast<float>(h3::dot(p, c.up)), static_cast<float>(h3::dot(p, c.forward))};
}
} // namespace
struct Renderer::Impl {
    const Catalog &catalog;
    const Terrain *terrain;
    GLuint mesh_program = 0, terrain_program = 0, terrain_vao = 0, terrain_buffer = 0, atlas = 0;
    std::map<std::string, Model> models;
    std::vector<StaticVertex> landscape;
    std::vector<TerrainVertex> relative;
    std::vector<Prop> props;
    RenderStats stats;
    Impl(const Catalog &c, const Terrain &t) : catalog(c), terrain(&t) {}
    ~Impl() {
        for (auto &[id, m] : models) {
            (void)id;
            gl::DeleteBuffers(1, &m.vertices);
            gl::DeleteBuffers(1, &m.instances);
            gl::DeleteVertexArrays(1, &m.vao);
        }
        if (terrain_buffer)
            gl::DeleteBuffers(1, &terrain_buffer);
        if (terrain_vao)
            gl::DeleteVertexArrays(1, &terrain_vao);
        if (atlas)
            glDeleteTextures(1, &atlas);
        if (mesh_program)
            gl::DeleteProgram(mesh_program);
        if (terrain_program)
            gl::DeleteProgram(terrain_program);
    }
    void uniforms(GLuint p, const Camera &camera) {
        gl::UseProgram(p);
        const auto L = terrain->space().radius();
        const double near_depth = std::tanh(.2 / L), far_depth = std::tanh(280 / L),
                     f = 1 / std::tan(camera.fov * .5);
        gl::Uniform2f(gl::GetUniformLocation(p, "uProjection"), static_cast<float>(f / camera.aspect),
                      static_cast<float>(f));
        gl::Uniform2f(gl::GetUniformLocation(p, "uDepth"),
                      static_cast<float>((far_depth + near_depth) / (far_depth - near_depth)),
                      static_cast<float>(2 * far_depth * near_depth / (far_depth - near_depth)));
        gl::Uniform1f(gl::GetUniformLocation(p, "uL"), static_cast<float>(L));
    }
    void put(const std::string &id, h3::Vec p, double yaw, double scale, std::array<float, 3> tint,
             const Camera &camera, double walk = 0, bool moving = false, double flash = 0) {
        auto it = models.find(id);
        if (it == models.end())
            return;
        auto &m = it->second;
        if (m.pending.size() >= 1024)
            return;
        Instance instance{};
        const auto f = terrain->space().frame(p, yaw);
        std::size_t at = 0;
        for (auto vector : {p, f.east, f.up, f.north})
            for (float v : local(vector, camera))
                instance.data[at++] = v;
        for (float v : tint)
            instance.data[at++] = v;
        instance.data[at++] = static_cast<float>(flash);
        instance.data[at++] = static_cast<float>(walk);
        instance.data[at++] = moving ? 1.f : 0.f;
        instance.data[at++] = static_cast<float>(flash);
        instance.data[at++] = static_cast<float>(scale);
        m.pending.push_back(instance);
    }
};
Renderer::Renderer(const Catalog &catalog, const Terrain &terrain)
    : impl_(std::make_unique<Impl>(catalog, terrain)) {
    gl::load();
    auto &r = *impl_;
    r.stats.gpu = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    r.stats.version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    r.mesh_program = program(mesh_vs);
    r.terrain_program = program(terrain_vs);
    for (auto &[id, recipe] : catalog.visuals) {
        auto data = evaluate_mesh(recipe);
        Model m;
        gl::GenVertexArrays(1, &m.vao);
        gl::GenBuffers(1, &m.vertices);
        gl::GenBuffers(1, &m.instances);
        gl::BindVertexArray(m.vao);
        gl::BindBuffer(GL_ARRAY_BUFFER, m.vertices);
        gl::BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(Vertex)), data.data(),
                       GL_STATIC_DRAW);
        m.count = static_cast<int>(data.size());
        const int sizes[] = {3, 3, 4, 1};
        const int offsets[] = {0, 12, 24, 40};
        for (int i = 0; i < 4; ++i) {
            gl::EnableVertexAttribArray(i);
            gl::VertexAttribPointer(i, sizes[i], GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                    reinterpret_cast<void *>(static_cast<std::uintptr_t>(offsets[i])));
        }
        gl::BindBuffer(GL_ARRAY_BUFFER, m.instances);
        gl::BufferData(GL_ARRAY_BUFFER, 1024 * sizeof(Instance), nullptr, GL_STREAM_DRAW);
        m.pending.reserve(1024);
        for (int i = 4; i < 10; ++i) {
            gl::EnableVertexAttribArray(i);
            gl::VertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, sizeof(Instance),
                                    reinterpret_cast<void *>(static_cast<std::uintptr_t>((i - 4) * 16)));
            gl::VertexAttribDivisor(i, 1);
        }
        r.stats.mesh_bytes += data.size() * sizeof(Vertex);
        r.stats.instance_bytes += 1024 * sizeof(Instance);
        r.models.emplace(id, std::move(m));
    }
    gl::GenVertexArrays(1, &r.terrain_vao);
    gl::GenBuffers(1, &r.terrain_buffer);
    gl::BindVertexArray(r.terrain_vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, r.terrain_buffer);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), nullptr);
    gl::EnableVertexAttribArray(1);
    gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex), reinterpret_cast<void *>(16));
    gl::BindVertexArray(0);
    r.stats.models = static_cast<unsigned>(r.models.size());
    r.stats.buffers = r.stats.models * 2 + 1;
    r.stats.textures = 1;
    set_terrain(terrain);
}
Renderer::~Renderer() = default;
void Renderer::set_terrain(const Terrain &terrain) {
    auto &r = *impl_;
    r.terrain = &terrain;
    r.landscape.clear();
    r.props.clear();
    const auto &h = terrain.space();
    for (auto &triangle : terrain.triangles()) {
        const auto &a = terrain.nodes()[triangle.nodes[0]];
        const auto f = h.frame(a.point);
        const auto v = h.log(a.point, terrain.nodes()[triangle.nodes[1]].point),
                   w = h.log(a.point, terrain.nodes()[triangle.nodes[2]].point);
        const double ax = h3::dot(v, f.east), ay = h3::dot(v, f.up), az = h3::dot(v, f.north),
                     bx = h3::dot(w, f.east), by = h3::dot(w, f.up), bz = h3::dot(w, f.north);
        double nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
        const double len = std::max(1e-9, std::sqrt(nx * nx + ny * ny + nz * nz));
        if (ny < 0) {
            nx = -nx;
            ny = -ny;
            nz = -nz;
        }
        const float shade =
            static_cast<float>(.65 + .35 * std::max(0.0, (-.45 * nx + .85 * ny + .3 * nz) / len));
        bool wet = false;
        for (int id : triangle.nodes) {
            auto &n = terrain.nodes()[id];
            auto color = n.sample.color;
            for (auto &c : color)
                c *= shade;
            r.landscape.push_back({n.point, color});
            wet |= n.sample.water;
        }
        if (wet)
            for (int id : triangle.nodes)
                r.landscape.push_back({h.lift(terrain.nodes()[id].base, .18), terrain.recipe().water});
    }
    r.relative.resize(r.landscape.size());
    gl::BindBuffer(GL_ARRAY_BUFFER, r.terrain_buffer);
    gl::BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(r.relative.size() * sizeof(TerrainVertex)),
                   nullptr, GL_STREAM_DRAW);
    std::uint32_t random = terrain.seed() ^ 0xb5297a4dU;
    auto rand = [&]() {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        return random / 4294967296.0;
    };
    for (std::size_t i = 0; i < terrain.nodes().size() && r.props.size() < 190; i += 7) {
        const auto &n = terrain.nodes()[i];
        if (!n.sample.walkable || n.sample.road || n.sample.bridge || h.dist(h3::origin(), n.point) < 12)
            continue;
        if (rand() < .55)
            r.props.push_back({"core:tree", n.point, rand() * 6.283, .8 + rand() * .55});
    }
    for (int team = 0; team < 4; ++team) {
        const auto home = terrain.nodes()[terrain.homes()[team]].point;
        for (int j = 0; j < 3; ++j) {
            const auto p = terrain.surface(h.offset(home, (j - 1) * 5, 0, -12));
            if (terrain.legal(p))
                r.props.push_back({"core:house", p, .4 + team, .85, -1});
        }
        const auto horse = terrain.surface(h.offset(home, 9, 0, 0));
        if (terrain.legal(horse) && r.models.contains("core:horse"))
            r.props.push_back({"core:horse", horse, team * .8, 1, team});
    }
    for (int river = 0; river < terrain.recipe().rivers; ++river)
        for (double v : {-23.0, 0.0, 23.0}) {
            const double u = (river - (terrain.recipe().rivers - 1) * .5) * 20, L = h.radius();
            const auto base = h3::Vec{std::cosh(u / L) * std::cosh(v / L), std::sinh(u / L), 0,
                                      std::cosh(u / L) * std::sinh(v / L)};
            const auto p = terrain.surface(base);
            const auto next = h3::Vec{std::cosh((u + .5) / L) * std::cosh(v / L), std::sinh((u + .5) / L), 0,
                                      std::cosh((u + .5) / L) * std::sinh(v / L)};
            const auto tangent = h.log(p, terrain.surface(next));
            const auto f = h.frame(p);
            const double yaw = std::atan2(h3::dot(tangent, f.east), h3::dot(tangent, f.north));
            r.props.push_back({"core:bridge", p, yaw, 1, -1});
        }
    constexpr int size = 256;
    std::vector<unsigned char> pixels(size * size * 4);
    for (int i = 0; i < size * size; ++i) {
        pixels[i * 4] = 18;
        pixels[i * 4 + 1] = 35;
        pixels[i * 4 + 2] = 29;
        pixels[i * 4 + 3] = 255;
    }
    auto xy = [&](h3::Vec p) {
        const auto k = h.klein(p);
        return std::array<double, 2>{(k[0] / terrain.extent() * .46 + .5) * (size - 1),
                                     (k[1] / terrain.extent() * .46 + .5) * (size - 1)};
    };
    for (auto &triangle : terrain.triangles()) {
        std::array<std::array<double, 2>, 3> p;
        std::array<float, 3> col{};
        for (int j = 0; j < 3; ++j) {
            auto &n = terrain.nodes()[triangle.nodes[j]];
            p[j] = xy(n.point);
            for (int k = 0; k < 3; ++k)
                col[k] += n.sample.color[k] / 3;
        }
        const int x0 = std::max(0, static_cast<int>(std::floor(std::min({p[0][0], p[1][0], p[2][0]})))),
                  x1 = std::min(size - 1, static_cast<int>(std::ceil(std::max({p[0][0], p[1][0], p[2][0]})))),
                  y0 = std::max(0, static_cast<int>(std::floor(std::min({p[0][1], p[1][1], p[2][1]})))),
                  y1 = std::min(size - 1, static_cast<int>(std::ceil(std::max({p[0][1], p[1][1], p[2][1]}))));
        const double den =
            (p[1][1] - p[2][1]) * (p[0][0] - p[2][0]) + (p[2][0] - p[1][0]) * (p[0][1] - p[2][1]);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const double a = ((p[1][1] - p[2][1]) * (x - p[2][0]) + (p[2][0] - p[1][0]) * (y - p[2][1])) /
                                 den,
                             b = ((p[2][1] - p[0][1]) * (x - p[2][0]) + (p[0][0] - p[2][0]) * (y - p[2][1])) /
                                 den;
                if (a >= -.001 && b >= -.001 && a + b <= 1.001)
                    for (int k = 0; k < 3; ++k)
                        pixels[(y * size + x) * 4 + k] =
                            static_cast<unsigned char>(std::clamp(col[k], 0.f, 1.f) * 255);
            }
    }
    if (!r.atlas)
        glGenTextures(1, &r.atlas);
    glBindTexture(GL_TEXTURE_2D, r.atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}
void Renderer::draw(const World &world, const Camera &camera, int width, int height, bool detailed) {
    auto &r = *impl_;
    const auto start = std::chrono::steady_clock::now();
    r.stats.draw_calls = 0;
    r.stats.triangles = 0;
    r.stats.visible_actors = 0;
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glClearColor(.62f, .75f, .80f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (std::size_t i = 0; i < r.landscape.size(); ++i) {
        const auto q = local(r.landscape[i].point, camera);
        const auto c = r.landscape[i].color;
        r.relative[i] = {q[0], q[1], q[2], q[3], c[0], c[1], c[2]};
    }
    r.uniforms(r.terrain_program, camera);
    gl::BindVertexArray(r.terrain_vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, r.terrain_buffer);
    gl::BufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(r.relative.size() * sizeof(TerrainVertex)),
                      r.relative.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<int>(r.relative.size()));
    ++r.stats.draw_calls;
    r.stats.triangles += static_cast<unsigned>(r.relative.size() / 3);
    for (auto &[id, m] : r.models) {
        (void)id;
        m.pending.clear();
    }
    const auto &h = r.terrain->space();
    for (std::size_t i = 0; i < r.props.size(); ++i) {
        const auto &p = r.props[i];
        if (!detailed && p.mesh == "core:tree" && i % 2)
            continue;
        const auto projected = CameraRig::project(h.offset(p.point, 0, 2, 0), camera, width, height);
        if (!projected.visible)
            continue;
        const auto tint =
            p.faction < 0 ? std::array<float, 3>{.32f, .45f, .23f} : r.catalog.factions[p.faction].color;
        r.put(p.mesh, p.point, p.yaw, p.scale, tint, camera);
    }
    for (auto &fort : world.fortresses()) {
        if (fort.ruined) {
            for (int i = 0; i < 4; ++i)
                r.put("core:effect", h.offset(fort.position, (i % 2 * 2 - 1) * 2, 1, (i / 2 * 2 - 1) * 2), 0,
                      4, {.48f, .46f, .38f}, camera);
        } else
            r.put("core:castle", fort.position, 0, 1, r.catalog.factions[fort.faction].color, camera);
    }
    for (auto handle : world.actors())
        if (auto *a = world.inspect(handle)) {
            const auto screen = CameraRig::project(h.offset(a->position, 0, 1, 0), camera, width, height);
            if (!screen.visible)
                continue;
            r.put(r.catalog.units.at(a->base).appearance, a->position, a->yaw, 1.08,
                  r.catalog.factions[a->faction].color, camera, a->walk, a->values.velocity > .05, a->flash);
            ++r.stats.visible_actors;
        }
    for (auto &p : world.projectiles())
        r.put(p.siege ? "core:effect" : "core:projectile", p.position, 0, p.siege ? 1.4 : 1,
              {.53f, .48f, .38f}, camera);
    unsigned shown = 0;
    for (auto &e : world.effects()) {
        if (shown++ > static_cast<unsigned>(detailed ? 180 : 60))
            break;
        const double t = e.age / e.life, scale = (e.kind == Feedback::Siege      ? 2.5
                                                  : e.kind == Feedback::Collapse ? 5
                                                                                 : .8) *
                                                 (1 - t);
        const auto color = e.kind == Feedback::Heal    ? std::array<float, 3>{.46f, .97f, .57f}
                           : e.kind == Feedback::Arrow ? std::array<float, 3>{.84f, .77f, .51f}
                                                       : std::array<float, 3>{1, .68f, .24f};
        r.put("core:effect", h.offset(e.position, 0, t * 2, 0), 0, std::max(.03, scale), color, camera);
    }
    r.uniforms(r.mesh_program, camera);
    for (auto &[id, m] : r.models) {
        (void)id;
        if (m.pending.empty())
            continue;
        gl::BindVertexArray(m.vao);
        gl::BindBuffer(GL_ARRAY_BUFFER, m.instances);
        gl::BufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(m.pending.size() * sizeof(Instance)),
                          m.pending.data());
        gl::DrawArraysInstanced(GL_TRIANGLES, 0, m.count, static_cast<int>(m.pending.size()));
        ++r.stats.draw_calls;
        r.stats.triangles += m.count / 3 * static_cast<unsigned>(m.pending.size());
    }
    gl::BindVertexArray(0);
    r.stats.submit_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    const auto error = glGetError();
    if (error != GL_NO_ERROR)
        throw std::runtime_error("OpenGL draw error " + std::to_string(error));
}
void Renderer::screenshot(const std::filesystem::path &path, int width, int height) const {
    std::filesystem::create_directories(path.parent_path());
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4),
        row(static_cast<std::size_t>(width) * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    for (int y = 0; y < height / 2; ++y) {
        auto *a = pixels.data() + static_cast<std::size_t>(y) * width * 4;
        auto *b = pixels.data() + static_cast<std::size_t>(height - 1 - y) * width * 4;
        std::memcpy(row.data(), a, row.size());
        std::memcpy(a, b, row.size());
        std::memcpy(b, row.data(), row.size());
    }
    SDL_Surface *surface =
        SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels.data(), width * 4);
    if (!surface || !SDL_SaveBMP(surface, utf8_path(path).c_str())) {
        if (surface)
            SDL_DestroySurface(surface);
        throw std::runtime_error(std::string("Screenshot failed: ") + SDL_GetError());
    }
    SDL_DestroySurface(surface);
}
unsigned Renderer::atlas_texture() const {
    return impl_->atlas;
}
const RenderStats &Renderer::stats() const {
    return impl_->stats;
}
} // namespace curved
