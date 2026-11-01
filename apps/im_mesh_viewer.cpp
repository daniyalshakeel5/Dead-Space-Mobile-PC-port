// Native Windows/Linux diagnostic viewer for geometry from a user's own IM-M3G.
// IMPORTANT: positions and triangle topology only. No claimed original game
// materials, UVs, normals, scene placement, animations or gameplay.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>

#include "ds_port/im_mesh.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr std::uint64_t kMaxFile = 64ull * 1024ull * 1024ull;
constexpr std::uint64_t kMaxVerts = 2000000;
constexpr std::uint64_t kMaxFaces = 1000000;

struct Point { double x, y, z; };
struct RenderVertex { float x, y, z, nx, ny, nz; };
struct DiagnosticScene {
    std::vector<RenderVertex> triangles;
    std::size_t skipped_degenerate = 0;
    std::string reason;
};

DiagnosticScene parse_generated_obj(const std::string& obj,
                                   std::uint64_t declared_vertices,
                                   std::uint64_t declared_faces) {
    DiagnosticScene scene;
    if (!declared_vertices || !declared_faces ||
        declared_vertices > kMaxVerts || declared_faces > kMaxFaces) {
        scene.reason = "model geometry exceeds safe viewer budget";
        return scene;
    }
    std::vector<Point> points;
    points.reserve(static_cast<std::size_t>(declared_vertices));
    std::vector<std::array<std::uint32_t, 3>> faces;
    faces.reserve(static_cast<std::size_t>(declared_faces));
    std::istringstream input(obj);
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string command;
        row >> command;
        if (command == "v") {
            Point point{};
            if (points.size() >= declared_vertices ||
                !(row >> point.x >> point.y >> point.z) ||
                !std::isfinite(point.x) || !std::isfinite(point.y) ||
                !std::isfinite(point.z)) {
                scene.reason = "invalid or oversized vertex stream";
                return scene;
            }
            points.push_back(point);
        } else if (command == "f") {
            std::array<std::uint32_t, 3> face{};
            if (faces.size() >= declared_faces ||
                !(row >> face[0] >> face[1] >> face[2]) ||
                face[0] == 0 || face[1] == 0 || face[2] == 0) {
                scene.reason = "invalid triangle index stream";
                return scene;
            }
            faces.push_back(face);
        }
    }
    if (points.size() != declared_vertices || faces.size() != declared_faces) {
        scene.reason = "OBJ geometry count mismatch";
        return scene;
    }
    Point low = points.front(), high = points.front();
    for (const auto& p : points) {
        low.x = std::min(low.x, p.x); low.y = std::min(low.y, p.y);
        low.z = std::min(low.z, p.z);
        high.x = std::max(high.x, p.x); high.y = std::max(high.y, p.y);
        high.z = std::max(high.z, p.z);
    }
    const Point center{(low.x + high.x) / 2.0, (low.y + high.y) / 2.0,
                       (low.z + high.z) / 2.0};
    const double extent = std::max({high.x - low.x, high.y - low.y,
                                    high.z - low.z});
    if (!std::isfinite(extent) || extent <= 0.0) {
        scene.reason = "zero-sized or non-finite geometry bounds";
        return scene;
    }
    const double scale = 2.0 / extent;
    scene.triangles.reserve(faces.size() * 3);
    for (const auto& f : faces) {
        if (f[0] > points.size() || f[1] > points.size() ||
            f[2] > points.size()) {
            scene.reason = "triangle references missing vertex";
            scene.triangles.clear();
            return scene;
        }
        const Point p[3] = {points[f[0]-1], points[f[1]-1], points[f[2]-1]};
        const double ax=p[1].x-p[0].x, ay=p[1].y-p[0].y, az=p[1].z-p[0].z;
        const double bx=p[2].x-p[0].x, by=p[2].y-p[0].y, bz=p[2].z-p[0].z;
        double nx=ay*bz-az*by, ny=az*bx-ax*bz, nz=ax*by-ay*bx;
        const double squared=nx*nx+ny*ny+nz*nz;
        if (!std::isfinite(squared) || squared < 1e-25) {
            ++scene.skipped_degenerate;
            continue;
        }
        const double inv=1.0/std::sqrt(squared);
        nx*=inv; ny*=inv; nz*=inv;  // geometric face normal, NOT game normals
        for (const auto& vertex : p) {
            const double x=(vertex.x-center.x)*scale;
            const double y=(vertex.y-center.y)*scale;
            const double z=(vertex.z-center.z)*scale;
            if (std::max({std::fabs(x), std::fabs(y), std::fabs(z)}) > 10.0) {
                scene.reason = "normalized vertex out of safe range";
                scene.triangles.clear();
                return scene;
            }
            scene.triangles.push_back({static_cast<float>(x),
                                       static_cast<float>(y),
                                       static_cast<float>(z),
                                       static_cast<float>(nx),
                                       static_cast<float>(ny),
                                       static_cast<float>(nz)});
        }
    }
    if (scene.triangles.empty()) scene.reason = "no non-degenerate triangles";
    return scene;
}

bool load_original_mesh(const char* path, DiagnosticScene& scene,
                        ds_port::ImMeshReport& info) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) { std::cerr << "Cannot open local asset.\n"; return false; }
    const auto file_size = input.tellg();
    if (file_size <= 0 || static_cast<std::uint64_t>(file_size) > kMaxFile) {
        std::cerr << "Asset exceeds bounded viewer file size.\n"; return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file_size));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), file_size)) {
        std::cerr << "Could not read local asset.\n"; return false;
    }
    std::ostringstream obj;
    info = ds_port::scan_im_mesh(bytes.data(), bytes.size(), &obj);
    if (!info.valid) {
        std::cerr << "IM-M3G position/triangle subset unsupported: "
                  << info.error << "\n";
        return false;
    }
    scene = parse_generated_obj(obj.str(), info.vertices, info.triangles);
    if (!scene.reason.empty()) {
        std::cerr << "Native geometry viewer rejected input: " << scene.reason << "\n";
        return false;
    }
    return true;
}

void draw(const DiagnosticScene& scene, int width, int height,
          float yaw, float pitch, float zoom, bool wireframe) {
    if (width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    glClearColor(0.07f, 0.08f, 0.095f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const double aspect=double(width)/height;
    constexpr double near_plane=0.1, far_plane=100.0, top=0.060;
    glFrustum(-top*aspect,top*aspect,-top,top,near_plane,far_plane);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const GLfloat light[4]={0.3f,0.8f,1.0f,0.0f};
    glLightfv(GL_LIGHT0,GL_POSITION,light);
    glTranslatef(0,0,-zoom);
    glRotatef(pitch,1,0,0);
    glRotatef(yaw,0,1,0);
    glPolygonMode(GL_FRONT_AND_BACK,wireframe?GL_LINE:GL_FILL);
    glColor3f(0.77f,0.80f,0.84f); // neutral diagnostic shading, NOT game material
    glVertexPointer(3,GL_FLOAT,sizeof(RenderVertex),&scene.triangles[0].x);
    glNormalPointer(GL_FLOAT,sizeof(RenderVertex),&scene.triangles[0].nx);
    glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(scene.triangles.size()));
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ds_im_mesh_viewer <local-owned-asset.m3g>\n"
                  << "Controls: left-drag orbit | mouse wheel zoom | W wireframe | R reset | Esc quit\n"
                  << "Geometry-only diagnostic: textures/rigging/visual fidelity NOT verified.\n";
        return 2;
    }
    DiagnosticScene scene;
    ds_port::ImMeshReport report;
    if (!load_original_mesh(argv[1],scene,report)) return 1;
    std::cout << "Verified IM-M3G geometry subset: " << report.mesh_objects
              << " meshes, " << report.vertices << " vertices, "
              << report.triangles << " triangles; "
              << scene.skipped_degenerate << " degenerate triangles skipped.\n";
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL2 initialization failed: " << SDL_GetError() << "\n";
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    SDL_Window* window=SDL_CreateWindow(
        "Dead Space Mobile - IM-M3G geometry inspector (not a playable game)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        std::cerr << "SDL2 window creation failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }
    SDL_GLContext context=SDL_GL_CreateContext(window);
    if (!context) {
        std::cerr << "OpenGL context creation failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window); SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // winding/orientation not verified from original engine
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    float yaw=25.0f, pitch=-20.0f, zoom=4.5f;
    bool wire=false, dragging=false, active=true;
    while (active) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) active=false;
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
                dragging=true;
            else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
                dragging=false;
            else if (event.type == SDL_MOUSEMOTION && dragging) {
                yaw += event.motion.xrel*0.35f;
                pitch = std::max(-89.0f,std::min(89.0f,
                                      pitch+event.motion.yrel*0.35f));
            } else if (event.type == SDL_MOUSEWHEEL) {
                zoom=std::max(1.6f,std::min(20.0f,zoom-event.wheel.y*0.35f));
            } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: active=false; break;
                    case SDLK_w: wire=!wire; break;
                    case SDLK_r: yaw=25.0f;pitch=-20.0f;zoom=4.5f;break;
                    default: break;
                }
            }
        }
        int width=0,height=0;
        SDL_GL_GetDrawableSize(window,&width,&height);
        draw(scene,width,height,yaw,pitch,zoom,wire);
        SDL_GL_SwapWindow(window);
        SDL_Delay(4);
    }
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
