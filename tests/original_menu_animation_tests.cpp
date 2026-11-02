#ifdef NDEBUG
#undef NDEBUG
#endif
#include "ds_port/original_menu_animation.hpp"
#include <cassert>
#include <cmath>
using namespace ds_port;
int main(){
    const std::array<float,16> identity={
        1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    OriginalMenuMotion motion{};
    motion.decoded=true;
    motion.sequences={
        {19,1000,176,192,{{0,{0,0,0}},{1000,{10,20,30}}}},
        {20,1000,177,192,{{0,{0,0,0,1}},{1000,{0,0,1,0}}}},
        {21,1000,176,193,{{0,{0,0,0}},{1000,{5,0,0}}}}
    };
    OriginalMotionController c{};
    c.object_id=18;c.speed=1;c.weight=1;
    c.active_start=0;c.active_end=2000;
    motion.controllers={c};
    motion.tracks={{25,19,18,275},{26,20,18,268}};
    std::vector<OriginalSceneNode> base={
        {25,0,identity,true,1.f},{26,0,identity,true,1.f}};
    std::vector<OriginalSceneNode> out;
    assert(sample_original_menu_motion(motion,base,500,out));
    assert(std::fabs(out[0].local[12]-5.f)<1e-5f);
    assert(std::fabs(out[0].local[13]-10.f)<1e-5f);
    assert(std::fabs(out[0].local[14]-15.f)<1e-5f);
    assert(std::fabs(out[1].local[0])<1e-4f);
    assert(std::fabs(std::fabs(out[1].local[1])-1.f)<1e-4f);
    assert(base[0].local[12]==0.f); // source scene never mutated
    assert(sample_original_menu_motion(motion,base,3000,out));
    assert(out[0].local[12]==0.f); // controller interval honored
    motion.controllers[0].active_end=0;
    assert(sample_original_menu_motion(motion,base,500,out,true));
    assert(std::fabs(out[0].local[12]-5.f)<1e-5f);
    assert(sample_original_menu_motion(motion,base,500,out,false));
    assert(out[0].local[12]==0.f); // inactive controller
    motion.tracks[1].property_id=276;
    motion.sequences[1].keys={{0,{1}},{1000,{0}}};
    assert(sample_original_menu_motion(motion,base,800,out,true));
    assert(!out[1].rendering_enabled);
    return 0;
}
