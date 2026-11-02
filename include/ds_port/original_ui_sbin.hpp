#pragma once
#include "ds_port/m3g_container.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace ds_port {
struct OriginalUiField {
    std::string name;
    std::uint16_t type=0,offset=0,auxiliary=0;
};
struct OriginalUiStruct {
    std::string name;
    std::vector<OriginalUiField> fields;
};
struct OriginalUiRectangle {
    std::uint16_t x=0,y=0,width=0,height=0;
};
struct OriginalUiSprite {
    std::uint32_t object_id=0;
    std::string name;
    std::uint8_t atlas=0,content_scale=1;
    std::uint16_t source_width=0,source_height=0;
    OriginalUiRectangle source_rect;
    OriginalUiRectangle atlas_rect;
};
struct OriginalUiSbin {
    bool decoded=false;
    std::string error;
    std::vector<std::string> names;
    std::vector<OriginalUiStruct> types;
    std::vector<OriginalUiSprite> sprites;
    std::vector<std::uint8_t> object_headers;
    std::vector<std::uint8_t> object_data;
};
// Parses the verified SBIN v2 binary metadata embedded as Image2D user
// parameter 500. Every string, field offset and chunk is bounded.
// Reconstruct a sprite from the source/destination crop boxes stored in
// the original atlas. Blank trimmed margins remain transparent.
M3gRgbaImage reconstruct_original_ui_sprite(
    const OriginalUiSprite& sprite,
    const std::vector<M3gRgbaImage>& original_atlases);
OriginalUiSbin decode_original_ui_sbin(const std::uint8_t* data,
                                      std::size_t bytes);
}
