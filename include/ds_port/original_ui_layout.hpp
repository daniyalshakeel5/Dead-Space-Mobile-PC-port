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
struct OriginalLayoutDatabase {
    bool decoded=false;
    std::string error;
    std::vector<std::string> names;
    std::vector<OriginalLayoutType> types;
    std::vector<OriginalLayoutRef> references;
    std::vector<std::uint8_t> records;
    // Unlinked layout records cannot be treated as absolute screen coordinates.
    bool contains_name(const std::string& name)const;
};
OriginalLayoutDatabase decode_original_layout_sbin(
    const std::uint8_t* bytes,std::size_t size);
}
