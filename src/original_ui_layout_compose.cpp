#include "ds_port/original_ui_layout_compose.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_set>
namespace ds_port {
namespace {
struct Rect{float x=0,y=0,w=0,h=0;};
std::uint32_t action_id(const std::string& root,const std::string& role) {
    std::uint32_t value=2166136261u;
    for(const char c:root+"/"+role) {
        value^=static_cast<std::uint8_t>(c);
        value*=16777619u;
    }
    return value?value:1u;
}
Rect entity_rect(const Rect& parent,const OriginalLayoutEntity& e) {
    const auto point=[&](const OriginalLayoutVertex& p){
        return std::pair<float,float>{
            parent.x+p.x+p.anchor_x*parent.w,
            parent.y+p.y+p.anchor_y*parent.h};
    };
    if(e.vertices.empty())return {};
    const auto a=point(e.vertices.front());
    if(e.vertices.size()==1)return {a.first,a.second,e.width,e.height};
    const auto b=point(e.vertices.back());
    return {std::min(a.first,b.first),std::min(a.second,b.second),
            std::fabs(a.first-b.first),std::fabs(a.second-b.second)};
}
bool is_button(const std::string& name){
    return name.rfind("BTN_",0)==0;
}
std::string icon_for_role(const std::string& role){
    if(role=="BTN_PLAY")return "STORY";
    if(role=="BTN_INFO")return "INFO";
    if(role=="BTN_EXTRAS")return "EXTRAS";
    if(role=="BTN_OPTIONS")return "OPTIONS";
    if(role=="BTN_EA")return "EA";
    if(role=="BTN_DS")return "DS";
    return "";
}
bool conditional_icon(const std::string& name){
    return name=="STORY"||name=="INFO"||name=="EXTRAS"||
           name=="OPTIONS"||name=="ICON_DS_ON"||name=="ICON_DS_OFF"||
           name=="ICON_EA_ON"||name=="ICON_EA_OFF";
}
}
OriginalUiComposedScreen compose_original_ui_screen(
    const OriginalLayoutDatabase& database,const OriginalUiSbin& atlas,
    const std::string& root,std::uint32_t hovered){
    OriginalUiComposedScreen out{};
    const auto fail=[&](const std::string& message){
        out.valid=false;out.error=message;out.draws.clear();
        out.actions.clear();return out;
    };
    if(!database.decoded||!atlas.decoded)
        return fail("Original main-menu layout and atlas not available");
    std::unordered_map<std::string,const OriginalLayoutScreen*> screens;
    for(const auto& screen:database.screens)
        screens.emplace(screen.name,&screen);
    if(!screens.count(root))return fail("Original menu screen absent");
    std::unordered_map<std::string,const OriginalUiSprite*> sprites;
    for(const auto& sprite:atlas.sprites)
        sprites.emplace(sprite.name,&sprite);
    std::unordered_set<std::string> ancestors;
    const auto round=[](float v)->int{return static_cast<int>(std::lround(v));};
    std::function<bool(const std::string&,const Rect&,const std::string&,int)> visit;
    visit=[&](const std::string& name,const Rect& bounds,
              const std::string& inherited_role,int depth)->bool{
        if(depth>=16||!ancestors.insert(name).second)return false;
        const auto iter=screens.find(name);
        if(iter==screens.end()){ancestors.erase(name);return false;}
        for(const auto& entity:iter->second->entities) {
            if(!entity.visible||entity.vertices.empty())continue;
            const std::string role=is_button(entity.name)?
                entity.name:inherited_role;
            const auto expected=icon_for_role(role);
            if((name=="UI_BUTTON_MAIN_ON"||name=="UI_BUTTON_MAIN_OFF") &&
               conditional_icon(entity.name)&&
               entity.name!=expected)continue;
            if(name=="UI_BUTTON_MAIN_TOP" &&
               conditional_icon(entity.name) &&
               entity.name.find(expected)==std::string::npos)continue;
            if(name=="UI_BUTTON_MAIN" &&
               (entity.name=="STORY_ON"||entity.name=="STORY_OFF")){
                const bool over=hovered==action_id(root,role);
                if((entity.name=="STORY_ON")!=over)continue;
            }
            const Rect rect=entity_rect(bounds,entity);
            if(entity.type=="SUBLAYOUT") {
                if(entity.sublayout.empty() ||
                   !visit(entity.sublayout,rect,role,depth+1))
                    ++out.unsupported_primitives;
            }else if(entity.type=="HELPERRECT") {
                if(!role.empty()&&is_button(role) && rect.w>0 && rect.h>0) {
                    const auto id=action_id(root,role);
                    out.actions.emplace(id,role);
                    out.draws.push_back({0,round(rect.x),round(rect.y),
                        std::max(1,round(rect.w)),std::max(1,round(rect.h)),
                        true,true,id});
                }
            }else if(entity.type=="IMAGE") {
                const auto found=sprites.find(entity.image);
                if(found==sprites.end()){
                    ++out.missing_original_images;continue;
                }
                const auto& sprite=*found->second;
                const float width=entity.width>0?entity.width:
                    rect.w>0?rect.w:float(sprite.source_width);
                const float height=entity.height>0?entity.height:
                    rect.h>0?rect.h:float(sprite.source_height);
                if(width<=0||height<=0)continue;
                out.draws.push_back({sprite.object_id,
                    round(rect.x),round(rect.y),
                    std::max(1,round(width)),std::max(1,round(height)),
                    false,false,0});
            }else ++out.unsupported_primitives;
            if(out.draws.size()>4096) {
                ancestors.erase(name);return false;
            }
        }
        ancestors.erase(name);return true;
    };
    if(!visit(root,{0,0,480,320},"",0))
        return fail("Original UI layout hierarchy invalid or exceeds budget");
    if(out.draws.empty())return fail("Original UI layout has no drawable sprites");
    out.valid=true;return out;
}
}
