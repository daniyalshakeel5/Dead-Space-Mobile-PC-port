#include "ds_port/original_pointer_jni_exports.hpp"
#include "ds_port/original_input_events.hpp"
#include <array>
#include <iostream>
using Getter=std::int32_t (*)(void*,void*) noexcept;
int main(){
    using namespace ds_port::original_runtime;
    constexpr std::array<Getter,3> functions={
        Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerDown,
        Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerMove,
        Java_com_ea_blast_TouchSurfaceAndroid_NativeGetIdRawPointerUp
    };
    constexpr std::array<std::int32_t,3> original={
        static_cast<std::int32_t>(kOriginalRawPointerDown),
        static_cast<std::int32_t>(kOriginalRawPointerMove),
        static_cast<std::int32_t>(kOriginalRawPointerUp)
    };
    for(std::size_t i=0;i<functions.size();++i){
        if(functions[i](nullptr,nullptr)!=original[i]){
            std::cerr<<"Original pointer-event JNI constant mismatch at index "<<i<<"\n";
            return 1;
        }
    }
    return 0;
}
