#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace xperia {
namespace texture {
constexpr std::uint32_t ATC_RGB=0x8c92u;
constexpr std::uint32_t ATC_RGBA_EXPLICIT=0x8c93u;
constexpr std::uint32_t ATC_RGBA_INTERPOLATED=0x87eeu;
constexpr std::uint32_t PVRTC_RGB_4BPP=0x8c00u;
constexpr std::uint32_t PVRTC_RGB_2BPP=0x8c01u;
constexpr std::uint32_t PVRTC_RGBA_4BPP=0x8c02u;
constexpr std::uint32_t PVRTC_RGBA_2BPP=0x8c03u;
bool supported(std::uint32_t format) noexcept;
// Validates both the texture dimensions and the exact payload size.
// Never passes an unvalidated guest pointer to the native PowerVR decoder.
bool expected_size(std::uint32_t format,int width,int height,
                   std::size_t& result,std::string& error) noexcept;
bool decode(std::uint32_t format,int width,int height,
            const std::uint8_t* bytes,std::size_t length,
            std::vector<std::uint8_t>& rgba,std::string& error);
}
}
