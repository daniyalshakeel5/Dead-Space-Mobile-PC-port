#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace ds_port {
struct OriginalLayoutField {
    std::string name;
    std::uint16_t datatype=0,byte_offset=0,auxiliary=0;
};
struct OriginalLayoutType {
    std::string name;
    std::vector<OriginalLayoutField> fields;
};
struct OriginalLayoutRef {
    std::uint32_t byte_offset=0;
    std::uint8_t tag=0;
};
struct OriginalLayoutVertex {
    float x=0.f,y=0.f,anchor_x=0.f,anchor_y=0.f;
};
struct OriginalLayoutEntity {
    std::string name,type,image,font,text_id,sublayout,align,blend,color;
    float width=0.f,height=0.f;
    std::uint32_t flags=0;
    bool visible=true,scrolling=false;
    std::vector<OriginalLayoutVertex> vertices;
};
struct OriginalLayoutScreen {
    std::string name;
    std::vector<OriginalLayoutEntity> entities;
};
struct OriginalLayoutDatabase {
    bool decoded=false;
    std::string error;
    std::vector<std::string> names;
    std::vector<OriginalLayoutType> types;
    std::vector<OriginalLayoutScreen> screens;
    std::vector<OriginalLayoutRef> references;
    std::vector<std::uint8_t> records;
    // Unlinked layout records cannot be treated as absolute screen coordinates.
    bool contains_name(const std::string& name)const;
};
OriginalLayoutDatabase decode_original_layout_sbin(
    const std::uint8_t* bytes,std::size_t size);
}
