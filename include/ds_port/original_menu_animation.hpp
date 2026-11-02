#pragma once
#include "ds_port/original_menu_scene_graph.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace ds_port {
// JSR-184 classes 1/2/19, decoded directly from the original IM-M3G
// object stream. No game asset data is compiled into the executable.
struct OriginalMotionKey {
    std::uint32_t time=0;
    std::vector<float> components;
};
struct OriginalMotionSequence {
    std::uint32_t object_id=0,duration=0;
    std::uint8_t interpolation=0,repeat=0;
    std::vector<OriginalMotionKey> keys;
};
struct OriginalMotionController {
    std::uint32_t object_id=0;
    float speed=1.f,weight=1.f,reference_sequence_time=0.f;
    std::int32_t active_start=0,active_end=0,reference_world_time=0;
};
struct OriginalMotionTrack {
    std::uint32_t target_id=0,sequence_id=0,controller_id=0,property_id=0;
};
struct OriginalMenuMotion {
    bool decoded=false;
    std::string error;
    std::vector<OriginalMotionSequence> sequences;
    std::vector<OriginalMotionController> controllers;
    std::vector<OriginalMotionTrack> tracks;
    std::size_t assigned_tracks=0;
};
OriginalMenuMotion decode_original_menu_motion(const std::uint8_t* data,std::size_t size);
// Evaluate the exact keyframe data; default respects the file's serialized
// controller active intervals. The preview_enable flag explicitly opts in
// to boot-time activation when the original executable would configure
// controllers later (that original callback is not reconstructed yet).
bool sample_original_menu_motion(const OriginalMenuMotion& animation,
    const std::vector<OriginalSceneNode>& source,std::int64_t time_ms,
    std::vector<OriginalSceneNode>& result,bool preview_enable=false);
}
