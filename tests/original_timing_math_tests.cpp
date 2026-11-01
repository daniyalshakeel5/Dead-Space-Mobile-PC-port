#include "ds_port/original_timing_math.hpp"
#include <array>
#include <cstdint>
#include <iostream>
using namespace ds_port::original_runtime;
#define CHECK(x) do { if(!(x)){ std::cerr<<"ARM time math mismatch at "<<__LINE__<<"\n";return 1; }} while(0)

int main() {
    CHECK(original_time_pair_scaled_sum(0,0)==0);
    CHECK(original_time_pair_scaled_sum(2,500000)==2500000);
    CHECK(original_time_pair_scaled_sum(-2,250000)==-1750000);
    CHECK(original_time_pair_scaled_sum(-1,-1)==-1000001);
    CHECK(original_time_pair_scaled_sum(2147483647,2147483647)
          ==2147485794483647LL);
    CHECK(original_time_pair_scaled_sum(-2147483647-1,-2147483647-1)
          ==-2147485795483648LL);

    std::array<std::uint8_t,80> bytes{};
    bytes.fill(0xaa);
    CHECK(!original_store_time_pair_at_offset_0x40(nullptr,80,2,500000));
    CHECK(!original_store_time_pair_at_offset_0x40(bytes.data(),71,2,500000));
    CHECK(bytes[64]==0xaa);
    CHECK(original_store_time_pair_at_offset_0x40(bytes.data(),80,2,500000));
    constexpr std::uint64_t expected=2500000;
    for(unsigned i=0;i<8;++i)
        CHECK(bytes[64+i]==static_cast<std::uint8_t>((expected>>(8*i))&0xffu));
    for(std::size_t i=0;i<bytes.size();++i)
        if(i<64 || i>=72) CHECK(bytes[i]==0xaa);
    CHECK(original_store_time_pair_at_offset_0x40(bytes.data(),80,-1,-1));
    const auto negative=static_cast<std::uint64_t>(std::int64_t(-1000001));
    for(unsigned i=0;i<8;++i)
        CHECK(bytes[64+i]==static_cast<std::uint8_t>((negative>>(8*i))&0xffu));
    std::cout<<"Original ARM signed SMLAL and STRD time-value instruction subset verified\n";
    return 0;
}
