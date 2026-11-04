#define SDL_MAIN_HANDLED
#include "ds_port/original_splash_host.hpp"
#include "ds_port/original_host_asset_io.hpp"
#include "ds_port/im_ui_image.hpp"
#include <SDL.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

namespace ds_port {
namespace {
using Window=std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)>;
using Renderer=std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)>;
using Texture=std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)>;
struct SdlQuit { ~SdlQuit(){SDL_Quit();} };
bool read_bounded_original_image(
    const std::filesystem::path& root,const char* path,
    M3gRgbaImage& image,bool& found) {
    std::vector<std::uint8_t> bytes;
    if(!read_original_asset_file(root,path,16u*1024u*1024u,bytes,&found))
        return false;
    auto decoded=decode_im_ui_rgba_images(bytes.data(),bytes.size());
    if(!decoded.decoded||decoded.images.size()!=1)return false;
    image=std::move(decoded.images.front());
    return image.decoded&&image.width&&image.height;
}
} // namespace
// The original game's splash Image2D RGBA rows are bottom-origin.
// Only correct the SDL 2D splash presentation, not generic decoders
// or the original OpenGL texture coordinates.
bool original_splash_render_upright(
    SDL_Renderer* renderer,SDL_Texture* texture,
    const SDL_Rect& destination) noexcept {
    return renderer&&texture &&
           SDL_RenderCopyEx(renderer,texture,nullptr,&destination,0.0,
                            nullptr,SDL_FLIP_VERTICAL)==0;
}
namespace {
bool show_image(SDL_Renderer* renderer,SDL_Window* window,
                const M3gRgbaImage& image,bool& stopped) {
    Texture texture(SDL_CreateTexture(
        renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,
        static_cast<int>(image.width),static_cast<int>(image.height)),
        SDL_DestroyTexture);
    if(!texture)return false;
    if(SDL_UpdateTexture(texture.get(),nullptr,
          image.pixels_rgba.data(),static_cast<int>(image.width)*4)!=0)
        return false;
    SDL_SetTextureBlendMode(texture.get(),SDL_BLENDMODE_BLEND);
    const auto stage_start=SDL_GetTicks();
    constexpr std::uint32_t kDurationMs=2200; // host staging, not original timing
    constexpr std::uint32_t kFadeMs=240;
    for(;;) {
        bool advance=false;
        SDL_Event e{};
        while(SDL_PollEvent(&e)) {
            if(e.type==SDL_QUIT){
                stopped=true;return true;
            }
            if(e.type==SDL_KEYDOWN&&!e.key.repeat) {
                if(e.key.keysym.sym==SDLK_ESCAPE){
                    stopped=true;return true;
                }
                if(e.key.keysym.sym==SDLK_RETURN ||
                   e.key.keysym.sym==SDLK_SPACE)advance=true;
            }
            if(e.type==SDL_MOUSEBUTTONDOWN &&
               e.button.button==SDL_BUTTON_LEFT)advance=true;
        }
        const auto elapsed=SDL_GetTicks()-stage_start;
        if(advance||elapsed>=kDurationMs)return true;
        int w=0,h=0;
        SDL_GetRendererOutputSize(renderer,&w,&h);
        if(w<=0||h<=0)return false;
        const double scale=std::min(
            (double(w)*0.88)/double(image.width),
            (double(h)*0.88)/double(image.height));
        if(scale<=0)return false;
        const auto drawn_w=std::max(1,int(double(image.width)*scale));
        const auto drawn_h=std::max(1,int(double(image.height)*scale));
        const SDL_Rect dest{(w-drawn_w)/2,(h-drawn_h)/2,drawn_w,drawn_h};
        std::uint8_t alpha=255;
        if(elapsed<kFadeMs)
            alpha=static_cast<std::uint8_t>(
                elapsed*255u/kFadeMs);
        else if(elapsed>kDurationMs-kFadeMs)
            alpha=static_cast<std::uint8_t>(
                (kDurationMs-elapsed)*255u/kFadeMs);
        SDL_SetTextureAlphaMod(texture.get(),alpha);
        SDL_SetRenderDrawColor(renderer,0,0,0,255);
        if(SDL_RenderClear(renderer)!=0 ||
           !original_splash_render_upright(renderer,texture.get(),dest))
            return false;
        SDL_RenderPresent(renderer);
        SDL_Delay(12);
    }
}
} // namespace
OriginalSplashResult present_original_splashes(
    const std::filesystem::path& root) {
    M3gRgbaImage ea,dead_space;
    bool found=false;
    if(!read_bounded_original_image(
        root,"published/texturepacks_ui/splash_ea.m3g",ea,found))
        return found?OriginalSplashResult::ImageDecodeFailed:
                     OriginalSplashResult::MissingOriginalAssets;
    if(!read_bounded_original_image(
        root,"published/texturepacks_ui/splash_deadspace.m3g",
        dead_space,found))
        return found?OriginalSplashResult::ImageDecodeFailed:
                     OriginalSplashResult::MissingOriginalAssets;
    SDL_SetMainReady();
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)!=0)
        return OriginalSplashResult::GraphicsUnavailable;
    SdlQuit quit_sdl{};
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");
    Window window(SDL_CreateWindow(
        "Dead Space Mobile - Original native splash",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,720,
        SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI),
        SDL_DestroyWindow);
    if(!window)return OriginalSplashResult::GraphicsUnavailable;
    Renderer renderer(SDL_CreateRenderer(
        window.get(),-1,SDL_RENDERER_ACCELERATED|
        SDL_RENDERER_PRESENTVSYNC),SDL_DestroyRenderer);
    if(!renderer)
        renderer.reset(SDL_CreateRenderer(
            window.get(),-1,SDL_RENDERER_SOFTWARE));
    if(!renderer)return OriginalSplashResult::GraphicsUnavailable;
    bool abort=false;
    if(!show_image(renderer.get(),window.get(),ea,abort))
        return OriginalSplashResult::GraphicsUnavailable;
    if(abort)return OriginalSplashResult::UserAborted;
    if(!show_image(renderer.get(),window.get(),dead_space,abort))
        return OriginalSplashResult::GraphicsUnavailable;
    return abort?OriginalSplashResult::UserAborted:
                 OriginalSplashResult::Completed;
}
} // namespace ds_port
