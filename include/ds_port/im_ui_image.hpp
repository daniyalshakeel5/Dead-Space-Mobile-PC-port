#pragma once
#include "ds_port/m3g_container.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ds_port {
// Original IronMonkey IM-M3G section framing + Image2D (type 10).
// Unlike JSR-184 files, this format uses a single proprietary
// type/length object stream; Image2D contents use the same
// Image2D superclass and immutable RGBA layout. The first
// Image2D may contain large user-parameter metadata.
// Pixels come ONLY from a user-supplied original game resource.
struct ImUiPack {
    bool decoded=false;
    std::string error;
    std::vector<M3gRgbaImage> images;
};
ImUiPack decode_im_ui_rgba_images(
    const std::uint8_t* data,std::size_t bytes);
} // namespace ds_port
