#include "ds_port/original_frame_dispatch.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
int main(){
    std::array<unsigned char,16> state{};
    using namespace ds_port::original_runtime;
    assert(execute_original_frame_prefix(state.data(),state.size(),nullptr)
        ==FramePrefixResult::MissingOriginalInitRoutine);
    for(auto b:state)assert(b==0);
    return 0;
}
