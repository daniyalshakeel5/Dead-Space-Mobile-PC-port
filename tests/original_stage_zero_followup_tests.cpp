#include "ds_port/original_stage_zero_followup.hpp"
#include <array>
#include <cstdint>
#include <iostream>
using ds_port::original_runtime::original_followup_store_0x2833f4;
#define CHECK(x) do { if (!(x)) { std::cerr << "ARM 0x2833f4 mismatch " << __LINE__ << ": " << #x << "\n"; return 1; } } while(0)
namespace {
std::uint32_t read_le32(const std::uint8_t* a) {
    return std::uint32_t(a[0]) | (std::uint32_t(a[1]) << 8) |
           (std::uint32_t(a[2]) << 16) | (std::uint32_t(a[3]) << 24);
}
}
int main() {
    std::array<std::uint8_t, 51> bytes{};
    bytes.fill(0xcd);
    const auto old = bytes;
    CHECK(!original_followup_store_0x2833f4(nullptr, bytes.size(), 0xffffffffu));
    CHECK(!original_followup_store_0x2833f4(bytes.data(), 0x27, 0x12345678u));
    CHECK(bytes == old);

    // Deliberately unaligned pointer as allowed on x86-64: the copied
    // ARM memory representation remains little-endian independently.
    auto* object = bytes.data()+1;
    CHECK(original_followup_store_0x2833f4(object, 0x28, 0x12345678u));
    CHECK(read_le32(object + 0x20) == 0u);
    CHECK(read_le32(object + 0x1c) == 0x12345678u);
    CHECK(read_le32(object + 0x24) == 0x12345678u);
    for(std::size_t i = 0; i < bytes.size(); ++i) {
        bool changed = (i >= 1+0x1c && i < 1+0x20) ||
                       (i >= 1+0x20 && i < 1+0x24) ||
                       (i >= 1+0x24 && i < 1+0x28);
        if (!changed) CHECK(bytes[i] == old[i]);
    }
    CHECK(original_followup_store_0x2833f4(object, 0x28, 0xffffffffu));
    CHECK(read_le32(object+0x1c)==0xffffffffu);
    CHECK(read_le32(object+0x20)==0u);
    CHECK(read_le32(object+0x24)==0xffffffffu);
    CHECK(original_followup_store_0x2833f4(object, 0x28, 0u));
    CHECK(read_le32(object+0x1c)==0u);
    CHECK(read_le32(object+0x20)==0u);
    CHECK(read_le32(object+0x24)==0u);
    std::cout << "Original full ARM 0x002833f4 3-store function verified\n";
    return 0;
}
