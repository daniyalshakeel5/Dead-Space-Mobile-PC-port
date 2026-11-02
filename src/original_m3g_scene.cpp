#include "ds_port/original_m3g_scene.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <unordered_map>
#include <unordered_set>
namespace ds_port {
namespace {
using Matrix=std::array<float,16>;
constexpr Matrix I={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
constexpr std::uint8_t magic[12]={0xab,'I','M','-','M','3','G',0xbb,13,10,26,10};
std::uint32_t u32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | std::uint32_t(p[1])<<8 |
           std::uint32_t(p[2])<<16 | std::uint32_t(p[3])<<24;
}
struct Cursor {
    const std::uint8_t* p; std::size_t n,at=0;
    bool skip(std::size_t z) {
        if(at>n||z>n-at)return false;
        at+=z;return true;
    }
    bool byte(std::uint8_t& v) {
        if(at>=n)return false;
        v=p[at++];return true;
    }
    bool word(std::uint32_t& v) {
        if(at>n||n-at<4)return false;
        v=u32(p+at);at+=4;return true;
    }
    bool real(float& v) {
        std::uint32_t raw=0;
        if(!word(raw))return false;
        std::memcpy(&v,&raw,4);
        return std::isfinite(v);
    }
    bool flag(bool& b) {
        std::uint8_t v=0;if(!byte(v)||v>1)return false;
        b=v==1;return true;
    }
    bool matrix(Matrix& v) {
        // M3G Transform.get serializes rows; OpenGL uses column-major.
        for(int row=0;row<4;++row)
            for(int col=0;col<4;++col)
                if(!real(v[col*4+row]))return false;
        return true;
    }
    bool done() const{return at==n;}
};
struct Obj {std::uint8_t type;const std::uint8_t* data;std::size_t size;};
Matrix mul(const Matrix& a,const Matrix& b) {
    Matrix out{};
    for(int c=0;c<4;++c)for(int r=0;r<4;++r)
        for(int k=0;k<4;++k)out[c*4+r]+=a[k*4+r]*b[c*4+k];
    return out;
}
bool object3d(Cursor& c,std::uint32_t id,std::size_t& tracks) {
    std::uint32_t user=0,count=0,params=0;
    if(!c.word(user)||!c.word(count)||count>256||count>(c.n-c.at)/4)return false;
    for(std::uint32_t i=0;i<count;++i) {
        std::uint32_t index=0;
        if(!c.word(index)||index==0||index>=id)return false;
    }
    tracks+=count;
    if(!c.word(params)||params>128)return false;
    std::unordered_set<std::uint32_t> param_ids;
    for(std::uint32_t i=0;i<params;++i) {
        std::uint32_t param=0,length=0;
        if(!c.word(param)||!c.word(length)||!param_ids.insert(param).second||
           !c.skip(length))return false;
    }
    return true;
}
bool transformable(Cursor& c,Matrix& local) {
    bool component=false,general=false;
    if(!c.flag(component))return false;
    Matrix composed=I;
    if(component){
        float tr[3]{},sc[3]{},angle=0,axis[3]{};
        for(float& x:tr)if(!c.real(x))return false;
        for(float& x:sc)if(!c.real(x))return false;
        if(!c.real(angle))return false;
        for(float& x:axis)if(!c.real(x))return false;
        const float norm=std::sqrt(axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2]);
        if(norm<1e-8f && std::fabs(angle)>1e-8f)return false;
        if(norm>1e-8f)for(float& x:axis)x/=norm;
        const float radians=angle*0.01745329251994329577f;
        const float cs=std::cos(radians),sn=std::sin(radians),one=1.f-cs;
        const float x=axis[0],y=axis[1],z=axis[2];
        Matrix rotation=I;
        rotation[0]=cs+x*x*one;rotation[4]=x*y*one-z*sn;rotation[8]=x*z*one+y*sn;
        rotation[1]=y*x*one+z*sn;rotation[5]=cs+y*y*one;rotation[9]=y*z*one-x*sn;
        rotation[2]=z*x*one-y*sn;rotation[6]=z*y*one+x*sn;rotation[10]=cs+z*z*one;
        Matrix scale=I;scale[0]=sc[0];scale[5]=sc[1];scale[10]=sc[2];
        Matrix translation=I;translation[12]=tr[0];translation[13]=tr[1];translation[14]=tr[2];
        composed=mul(mul(translation,rotation),scale);
    }
    if(!c.flag(general))return false;
    if(general) {
        Matrix generic{};
        if(!c.matrix(generic))return false;
        composed=mul(composed,generic);
    }
    for(float f:composed)if(!std::isfinite(f))return false;
    local=composed;
    return true;
}
bool node(Cursor& c,Matrix& local) {
    if(!transformable(c,local))return false;
    bool render=false,pick=false,aligned=false;
    std::uint8_t alpha=0;std::uint32_t scope=0;
    if(!c.flag(render)||!c.flag(pick)||!c.byte(alpha)||
       !c.word(scope)||!c.flag(aligned))return false;
    if(aligned && !c.skip(2+4+4))return false;
    return true;
}
bool camera_projection(Cursor& c,Matrix& projection) {
    std::uint8_t mode=0;if(!c.byte(mode))return false;
    if(mode==48)return c.matrix(projection);
    if(mode!=49 && mode!=50)return false;
    float fovy=0,aspect=0,near=0,far=0;
    if(!c.real(fovy)||!c.real(aspect)||!c.real(near)||!c.real(far)||
       fovy<=0||aspect<=0||near==far)return false;
    projection={};
    if(mode==50){
        if(fovy>=180||near<=0||far<=near)return false;
        const float h=1.f/std::tan(fovy*0.008726646259971648f);
        projection[0]=h/aspect;projection[5]=h;
        projection[10]=-(far+near)/(far-near);
        projection[11]=-1.f;projection[14]=-(2.f*far*near)/(far-near);
    }else {
        if(far<=near)return false;
        projection[0]=2.f/(fovy*aspect);
        projection[5]=2.f/fovy;
        projection[10]=-2.f/(far-near);
        projection[14]=-(far+near)/(far-near);
        projection[15]=1.f;
    }
    for(float f:projection)if(!std::isfinite(f))return false;
    return true;
}
bool image2d(Cursor& c,M3gRgbaImage& image) {
    std::uint8_t fmt=0;bool mutable_image=false;
    std::uint32_t w=0,h=0,palette=0,pixels=0;
    if(!c.byte(fmt)||!c.flag(mutable_image)||mutable_image||
       !c.word(w)||!c.word(h)||!c.word(palette)||palette!=0||
       !c.word(pixels)||w==0||h==0||w>4096||h>4096)return false;
    const unsigned channels=fmt==100?4u:fmt==99?3u:0u;
    if(!channels||std::uint64_t(w)*h*channels!=pixels||
       pixels>32u*1024u*1024u||c.at>c.n||pixels>c.n-c.at)return false;
    image.width=w;image.height=h;
    image.pixels_rgba.resize(std::size_t(w)*h*4);
    for(std::size_t i=0;i<std::size_t(w)*h;++i){
        const auto* pixel=c.p+c.at+i*channels;
        auto* output=image.pixels_rgba.data()+i*4;
        output[0]=pixel[0];output[1]=pixel[1];output[2]=pixel[2];
        output[3]=channels==4?pixel[3]:255;
    }
    image.decoded=true;
    return c.skip(pixels)&&c.done();
}
}
OriginalM3gScene decode_original_m3g_scene(const std::uint8_t* data,std::size_t size) {
    OriginalM3gScene out{};
    auto fail=[&](const char* message){
        out.decoded=false;out.error=message;
        out.nodes.clear();out.materials.clear();out.images.clear();return out;
    };
    if(!data||size<30||size>64u*1024u*1024u||
       !std::equal(std::begin(magic),std::end(magic),data)||
       data[12]!=0||u32(data+13)!=size-12)return fail("Invalid IM-M3G framing");
    std::vector<Obj> objects;
    std::size_t pos=21;
    while(pos<size-4&&objects.size()<20000) {
        if(size-4-pos<5)return fail("Truncated M3G object");
        const auto type=data[pos++],n=u32(data+pos);pos+=4;
        if(n>size-4-pos)return fail("Out of bounds M3G object");
        objects.push_back({type,data+pos,n});pos+=n;
    }
    if(pos!=size-4||objects.empty()||objects[0].type!=0)
        return fail("Incomplete M3G objects");
    std::unordered_map<std::uint32_t,std::uint32_t> parents,texture_image;
    std::unordered_map<std::uint32_t,std::uint32_t> image_indices;
    std::vector<std::pair<std::uint32_t,std::uint32_t>> appearance_texture;
    std::size_t pixel_bytes=0;
    for(std::size_t index=0;index<objects.size();++index){
        const auto& o=objects[index];
        const auto id=static_cast<std::uint32_t>(index+1);
        if(o.type!=5&&o.type!=9&&o.type!=14&&o.type!=3&&
           o.type!=10&&o.type!=17)continue;
        Cursor c{o.data,o.size,0};
        if(!object3d(c,id,out.animation_references))
            return fail("Malformed M3G Object3D header");
        if(o.type==5||o.type==9||o.type==14) {
            Matrix local{};
            if(!node(c,local))return fail("Malformed original M3G node transform");
            out.nodes.push_back({id,0,local});
            if(o.type==5){
                if(out.camera.node_id||!camera_projection(c,out.camera.projection)||!c.done())
                    return fail("Malformed original M3G camera");
                out.camera.node_id=id;
            }else if(o.type==9) {
                ++out.groups;
                std::uint32_t count=0;
                if(!c.word(count)||count>1024||count>(c.n-c.at)/4)
                    return fail("Invalid original group child count");
                for(std::uint32_t n=0;n<count;++n){
                    std::uint32_t child=0;
                    if(!c.word(child)||child==0||child>=id ||
                       objects[child-1].type!=5&&objects[child-1].type!=9&&
                       objects[child-1].type!=14 ||
                       !parents.emplace(child,id).second)
                        return fail("Invalid or duplicate M3G child node");
                    ++out.group_links;
                }
                if(!c.done())return fail("Unexpected original group payload");
            }
        }else if(o.type==3){
            std::uint8_t layer=0;std::uint32_t ignored=0,count=0;
            if(!c.byte(layer))return fail("Invalid original Appearance layer");
            for(int n=0;n<4;++n)
                if(!c.word(ignored))return fail("Invalid original Appearance state");
            if(!c.word(count)||count>16||count>(c.n-c.at)/4)
                return fail("Invalid Appearance texture array");
            std::uint32_t first=0;
            for(std::uint32_t n=0;n<count;++n){
                std::uint32_t texture=0;
                if(!c.word(texture)||texture>=id||
                   (texture && objects[texture-1].type!=17))
                    return fail("Invalid original texture reference");
                if(n==0)first=texture;
            }
            if(!c.done())return fail("Malformed original Appearance tail");
            appearance_texture.push_back({id,first});
        }else if(o.type==17){
            Matrix tex_matrix{};
            std::uint32_t image_id=0;
            if(!transformable(c,tex_matrix)||!c.word(image_id)||
               !image_id||image_id>=id||objects[image_id-1].type!=10||
               !c.skip(3+1+2+2)||!c.done())
                return fail("Invalid original Texture2D record");
            texture_image.emplace(id,image_id);
        }else if(o.type==10){
            M3gRgbaImage img{};
            if(!image2d(c,img))return fail("Unsupported original scene image");
            pixel_bytes+=img.pixels_rgba.size();
            if(pixel_bytes>96u*1024u*1024u)return fail("Original scene image budget exceeded");
            image_indices[id]=static_cast<std::uint32_t>(out.images.size());
            out.images.push_back(std::move(img));
        }
    }
    for(auto& n:out.nodes)if(parents.count(n.object_id))
        n.parent_id=parents.at(n.object_id);
    for(const auto& a:appearance_texture){
        OriginalSceneBinding m{};
        m.appearance_id=a.first;
        if(a.second){
            const auto tex=texture_image.find(a.second);
            if(tex==texture_image.end()||!image_indices.count(tex->second))
                return fail("Appearance texture image not resolved");
            m.textured=true;m.image_index=image_indices.at(tex->second);
        }
        out.materials.push_back(m);
    }
    if(!out.camera.node_id||out.nodes.empty()||out.groups==0)
        return fail("Original scene camera/groups absent");
    out.decoded=true;
    return out;
}
} // namespace ds_port
