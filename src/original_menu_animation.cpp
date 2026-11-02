#include "ds_port/original_menu_animation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
namespace ds_port {
namespace {
std::uint32_t le32(const std::uint8_t* p) {
    return std::uint32_t(p[0])|std::uint32_t(p[1])<<8|
        std::uint32_t(p[2])<<16|std::uint32_t(p[3])<<24;
}
struct Reader {
    const std::uint8_t* data;std::size_t size,offset=0;
    bool byte(std::uint8_t& value){
        if(offset>=size)return false;value=data[offset++];return true;
    }
    bool word(std::uint32_t& value){
        if(offset>size||size-offset<4)return false;
        value=le32(data+offset);offset+=4;return true;
    }
    bool real(float& value){
        std::uint32_t raw=0;if(!word(raw))return false;
        std::memcpy(&value,&raw,4);return std::isfinite(value);
    }
    bool skip(std::size_t n) {
        if(offset>size||n>size-offset)return false;
        offset+=n;return true;
    }
    bool done()const{return offset==size;}
};
struct Chunk {std::uint8_t type;const std::uint8_t* data;std::size_t size;};
bool object3d(Reader& r,std::vector<std::uint32_t>& tracks,
              std::uint32_t id){
    std::uint32_t user=0,count=0,params=0;
    if(!r.word(user)||!r.word(count)||count>256||
       count>(r.size-r.offset)/4)return false;
    tracks.clear();
    for(std::uint32_t j=0;j<count;++j){
        std::uint32_t target=0;
        if(!r.word(target)||target==0||target>=id)return false;
        tracks.push_back(target);
    }
    if(!r.word(params)||params>128)return false;
    for(std::uint32_t j=0;j<params;++j){
        std::uint32_t param=0,length=0;
        if(!r.word(param)||!r.word(length)||!r.skip(length))return false;
    }
    return true;
}
bool parse_sequence(Reader& r,std::uint32_t id,OriginalMotionSequence& seq){
    std::uint8_t interpol=0,repeat=0,encoding=0;
    std::uint32_t duration=0,first=0,last=0,components=0,keys=0;
    if(!r.byte(interpol)||!r.byte(repeat)||!r.byte(encoding)||
       !r.word(duration)||!r.word(first)||!r.word(last)||
       !r.word(components)||!r.word(keys))return false;
    // The original main menu uses LINEAR and SLERP sequences.
    if((interpol!=176&&interpol!=177&&interpol!=180)||
       (repeat!=192&&repeat!=193)||encoding>2||
       !components||components>32||keys==0||keys>20000||
       first>=keys||last>=keys||duration>36000000)
        return false;
    std::vector<float> bias(components,0.f),scale(components,1.f);
    if(encoding) {
        for(float& f:bias)if(!r.real(f))return false;
        for(float& f:scale)if(!r.real(f))return false;
    }
    seq.object_id=id;seq.interpolation=interpol;
    seq.repeat=repeat;seq.duration=duration;
    seq.keys.reserve(keys);
    std::uint32_t previous_time=0;
    for(std::uint32_t k=0;k<keys;++k) {
        OriginalMotionKey frame{};
        if(!r.word(frame.time)||(k&&frame.time<previous_time)||
           frame.time>duration)return false;
        previous_time=frame.time;
        frame.components.reserve(components);
        for(std::uint32_t n=0;n<components;++n){
            float f=0.f;
            if(!encoding){
                if(!r.real(f))return false;
            }else if(encoding==1){
                std::uint8_t b=0;if(!r.byte(b))return false;
                f=bias[n]+scale[n]*(float(b)/255.f);
            }else {
                std::uint8_t lo=0,hi=0;
                if(!r.byte(lo)||!r.byte(hi))return false;
                f=bias[n]+scale[n]*(float(std::uint16_t(lo)|std::uint16_t(hi)<<8)/65535.f);
            }
            if(!std::isfinite(f))return false;
            frame.components.push_back(f);
        }
        seq.keys.push_back(std::move(frame));
    }
    // A sliced valid range is retained exactly, never expanded to guessed keys.
    if(first||last!=keys-1){
        seq.keys=std::vector<OriginalMotionKey>(
            seq.keys.begin()+first,seq.keys.begin()+last+1);
    }
    return r.done();
}
bool parse_controller(Reader& r,std::uint32_t id,OriginalMotionController& c){
    std::uint32_t from=0,to=0,world=0;
    if(!r.real(c.speed)||!r.real(c.weight)||!r.word(from)||
       !r.word(to)||!r.real(c.reference_sequence_time)||
       !r.word(world)||!r.done()||c.weight<0.f||c.weight>1.f)
        return false;
    c.object_id=id;c.active_start=static_cast<std::int32_t>(from);
    c.active_end=static_cast<std::int32_t>(to);
    c.reference_world_time=static_cast<std::int32_t>(world);
    return true;
}
std::vector<float> sample(const OriginalMotionSequence& seq,double time){
    if(seq.keys.empty())return {};
    if(seq.duration && seq.repeat==193) {
        time=std::fmod(time,double(seq.duration));
        if(time<0)time+=seq.duration;
    }
    if(time<=seq.keys.front().time)return seq.keys.front().components;
    if(time>=seq.keys.back().time)return seq.keys.back().components;
    const auto next=std::upper_bound(seq.keys.begin(),seq.keys.end(),time,
        [](double t,const OriginalMotionKey& k){return t<k.time;});
    const auto& b=*next;const auto& a=*(next-1);
    if(seq.interpolation==180 || b.time==a.time)return a.components;
    const float t=float((time-a.time)/double(b.time-a.time));
    std::vector<float> value=a.components;
    if(seq.interpolation==177&&value.size()==4) {
        float dot=0.f;
        for(int i=0;i<4;++i)dot+=a.components[i]*b.components[i];
        const float sign=dot<0.f?-1.f:1.f;dot=std::clamp(std::fabs(dot),0.f,1.f);
        float wa=1.f-t,wb=t;
        if(dot<0.9995f) {
            const float angle=std::acos(dot);
            const float denom=std::sin(angle);
            if(denom>1e-7f) {
                wa=std::sin((1.f-t)*angle)/denom;
                wb=std::sin(t*angle)/denom;
            }
        }
        for(int i=0;i<4;++i)
            value[i]=wa*a.components[i]+sign*wb*b.components[i];
        float norm=0.f;for(float f:value)norm+=f*f;
        if(norm>1e-10f){norm=std::sqrt(norm);for(float& f:value)f/=norm;}
    }else for(std::size_t i=0;i<value.size();++i)
        value[i]+=t*(b.components[i]-value[i]);
    return value;
}
void rotate_from_quaternion(std::array<float,16>& m,
                            const std::vector<float>& q){
    if(q.size()!=4)return;
    float x=q[0],y=q[1],z=q[2],w=q[3];
    const float norm=std::sqrt(x*x+y*y+z*z+w*w);
    if(norm<1e-8f)return;
    x/=norm;y/=norm;z/=norm;w/=norm;
    float scales[3]{};
    for(int col=0;col<3;++col){
        for(int row=0;row<3;++row)
            scales[col]+=m[col*4+row]*m[col*4+row];
        scales[col]=std::sqrt(scales[col]);
    }
    const float xy=x*y,xz=x*z,yz=y*z,wx=w*x,wy=w*y,wz=w*z;
    const float r[9]={1-2*(y*y+z*z),2*(xy+wz),2*(xz-wy),
                      2*(xy-wz),1-2*(x*x+z*z),2*(yz+wx),
                      2*(xz+wy),2*(yz-wx),1-2*(x*x+y*y)};
    for(int col=0;col<3;++col)
        for(int row=0;row<3;++row)m[col*4+row]=r[col*3+row]*scales[col];
}
}
OriginalMenuMotion decode_original_menu_motion(const std::uint8_t* data,
                                               std::size_t size){
    OriginalMenuMotion out{};
    const auto fail=[&](const char* message) {
        out.decoded=false;out.error=message;out.sequences.clear();
        out.controllers.clear();out.tracks.clear();return out;
    };
    const std::uint8_t magic[12]={171,73,77,45,77,51,71,187,13,10,26,10};
    if(!data||size<30||size>64u*1024u*1024u||
       !std::equal(magic,magic+12,data)||
       data[12]!=0||le32(data+13)!=size-12)
        return fail("Invalid original IM-M3G animation container");
    std::vector<Chunk> chunks;
    std::size_t offset=21;
    while(offset<size-4&&chunks.size()<20000) {
        if(size-4-offset<5)return fail("Truncated original animation object");
        const std::uint8_t type=data[offset++];
        const auto n=le32(data+offset);offset+=4;
        if(n>size-4-offset)return fail("Invalid animation object size");
        chunks.push_back({type,data+offset,n});offset+=n;
    }
    if(offset!=size-4)return fail("Incomplete original animation objects");
    std::unordered_map<std::uint32_t,std::vector<std::uint32_t>> owners;
    std::unordered_map<std::uint32_t,OriginalMotionTrack> tracks;
    for(std::size_t i=0;i<chunks.size();++i) {
        const auto& chunk=chunks[i];const auto id=std::uint32_t(i+1);
        // Every standard JSR-184 scene class inherits Object3D, including
        // Texture2D, Appearance and Material. The original menu attaches
        // three tracks outside the Node hierarchy as well.
        if(chunk.type==0 || chunk.type>22)continue;
        Reader r{chunk.data,chunk.size,0};
        std::vector<std::uint32_t> attached;
        if(!object3d(r,attached,id))return fail("Invalid animated Object3D header");
        if(chunk.type==1){
            OriginalMotionController controller{};
            if(!parse_controller(r,id,controller))
                return fail("Invalid original animation controller");
            out.controllers.push_back(controller);
        }else if(chunk.type==2){
            OriginalMotionTrack track{};
            if(!r.word(track.sequence_id)||!r.word(track.controller_id)||
               !r.word(track.property_id)||!r.done()||
               !track.sequence_id||track.sequence_id>=id||
               chunks[track.sequence_id-1].type!=19||
               !track.controller_id||track.controller_id>=id||
               chunks[track.controller_id-1].type!=1)
                return fail("Invalid original animation track");
            tracks[id]=track;
        }else if(chunk.type==19){
            OriginalMotionSequence sequence{};
            if(!parse_sequence(r,id,sequence))
                return fail("Unsupported or invalid original keyframe sequence");
            out.sequences.push_back(std::move(sequence));
        }else if(!attached.empty()) {
            owners[id]=std::move(attached);
        }
    }
    for(const auto& owner:owners)
        for(const auto id:owner.second) {
            const auto track=tracks.find(id);
            if(track==tracks.end())
                return fail("Unresolved original node animation track reference");
            auto assigned=track->second;assigned.target_id=owner.first;
            out.tracks.push_back(assigned);
            ++out.assigned_tracks;
        }
    out.decoded=true;return out;
}
bool sample_original_menu_motion(const OriginalMenuMotion& animation,
    const std::vector<OriginalSceneNode>& source,std::int64_t time_ms,
    std::vector<OriginalSceneNode>& result,bool preview_enable) {
    if(!animation.decoded)return false;
    result=source;
    std::unordered_map<std::uint32_t,std::size_t> node_ids;
    for(std::size_t i=0;i<result.size();++i)
        node_ids[result[i].object_id]=i;
    std::unordered_map<std::uint32_t,const OriginalMotionSequence*> sequences;
    for(const auto& s:animation.sequences)sequences[s.object_id]=&s;
    std::unordered_map<std::uint32_t,const OriginalMotionController*> controllers;
    for(const auto& c:animation.controllers)controllers[c.object_id]=&c;
    for(const auto& track:animation.tracks) {
        const auto node=node_ids.find(track.target_id);
        if(node==node_ids.end())continue;
        const auto seq=sequences.find(track.sequence_id);
        const auto ctl=controllers.find(track.controller_id);
        if(seq==sequences.end()||ctl==controllers.end())return false;
        const auto& c=*ctl->second;
        if(c.weight<=0)continue;
        if(!preview_enable &&
           (time_ms<c.active_start || time_ms>=c.active_end))continue;
        const double tick=double(c.reference_sequence_time)+
            double(c.speed)*double(time_ms-c.reference_world_time);
        const auto value=sample(*seq->second,tick);
        auto& local=result[node->second].local;
        if(track.property_id==275 && value.size()==3) {
            local[12]=value[0];local[13]=value[1];local[14]=value[2];
        }else if(track.property_id==268 && value.size()==4) {
            rotate_from_quaternion(local,value);
        }else if(track.property_id==270 && value.size()==3) {
            // SCALE: original track sets absolute component scale.
            // Keep the recovered rotation directions and normalize its
            // existing basis before applying each animated magnitude.
            for(int axis=0;axis<3;++axis) {
                float length=0.f;
                for(int row=0;row<3;++row)
                    length+=local[axis*4+row]*local[axis*4+row];
                length=std::sqrt(length);
                if(length<1e-8f) {
                    if(value[axis]!=0.f)return false;
                    continue;
                }
                for(int row=0;row<3;++row)
                    local[axis*4+row]*=value[axis]/length;
            }
        }else if(track.property_id==256 && value.size()==1) {
            result[node->second].alpha=
                std::clamp(value[0],0.f,1.f);
        }else if(track.property_id==276 && value.size()==1) {
            result[node->second].rendering_enabled=value[0]>=0.5f;
        }
    }
    return true;
}
} // namespace ds_port
