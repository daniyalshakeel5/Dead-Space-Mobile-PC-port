#pragma once
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
struct OriginalUiSbin {
    bool decoded=false;
    std::string error;
    std::vector<std::string> names;
    std::vector<OriginalUiStruct> types;
    std::vector<std::uint8_t> object_headers;
    std::vector<std::uint8_t> object_data;
};
// Parses the verified SBIN v2 binary metadata embedded as Image2D user
// parameter 500. Every string, field offset and chunk is bounded.
OriginalUiSbin decode_original_ui_sbin(const std::uint8_t* data,
                                      std::size_t bytes);
}
