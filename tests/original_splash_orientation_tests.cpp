#define SDL_MAIN_HANDLED
#include "ds_port/original_splash_host.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <array>
#include <cassert>
#include <cstdint>
#include <SDL.h>
int main(){
    // No proprietary source image: an intentionally asymmetric 2x2
    // texture with row 0 representing the original bottom row.
    SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy");
    SDL_SetMainReady();
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SDL_Surface* surf=SDL_CreateRGBSurfaceWithFormat(
        0,2,2,32,SDL_PIXELFORMAT_RGBA32);
    assert(surf);
    SDL_Renderer* render=SDL_CreateSoftwareRenderer(surf);
    assert(render);
    SDL_Texture* texture=SDL_CreateTexture(
        render,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,2,2);
    assert(texture);
    const std::array<std::uint8_t,16> raw={{
        255,0,0,255,  0,255,0,255,
        0,0,255,255,  255,255,0,255
    }};
    assert(SDL_UpdateTexture(texture,nullptr,raw.data(),8)==0);
    assert(SDL_SetRenderDrawColor(render,0,0,0,255)==0);
    assert(SDL_RenderClear(render)==0);
    const SDL_Rect full{0,0,2,2};
    assert(ds_port::original_splash_render_upright(render,texture,full));
    std::array<std::uint8_t,16> pixels{};
    assert(SDL_RenderReadPixels(render,nullptr,SDL_PIXELFORMAT_RGBA32,
                                pixels.data(),8)==0);
    // Correct displayed top: BLUE, YELLOW; bottom: RED, GREEN.
    assert(pixels[0]==0&&pixels[1]==0&&pixels[2]==255);
    assert(pixels[4]==255&&pixels[5]==255&&pixels[6]==0);
    assert(pixels[8]==255&&pixels[9]==0&&pixels[10]==0);
    assert(pixels[12]==0&&pixels[13]==255&&pixels[14]==0);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(render);
    SDL_FreeSurface(surf);
    SDL_Quit();
    return 0;
}
