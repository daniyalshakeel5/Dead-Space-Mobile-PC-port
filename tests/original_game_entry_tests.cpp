#include "ds_port/original_game_entry.hpp"
#include <cstdint>
#include <iostream>

int main() {
    int local=5;
    if (Java_com_ea_DeadSpace_DeadSpace_runEntryPoint(nullptr,nullptr)!=0 ||
        Java_com_ea_DeadSpace_DeadSpace_runEntryPoint(&local,&local)!=0 ||
        local!=5) {
        std::cerr<<"Original ARM runEntryPoint must return zero without effects\n";
        return 1;
    }
    std::cout<<"Original JNI game-entry zero-return stub reconstructed\n";
    return 0;
}
