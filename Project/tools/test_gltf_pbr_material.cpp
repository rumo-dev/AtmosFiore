#include "Engine/Graphics/Model/gltf_pbr_material.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
using Json=nlohmann::json;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
bool near(float a,float b){return std::abs(a-b)<1e-5f;}
Json read_json(const std::string& path){std::ifstream file(path);Json j;file>>j;return j;}
Json read_glb(const std::string& path){std::ifstream file(path,std::ios::binary);std::vector<char>b((std::istreambuf_iterator<char>(file)),{});check(b.size()>20,"Missing GLB");uint32_t magic=0,n=0;std::memcpy(&magic,b.data(),4);std::memcpy(&n,b.data()+12,4);check(magic==0x46546c67&&20+n<=b.size(),"Invalid GLB");return Json::parse(b.data()+20,b.data()+20+n);}
int main()
{
    try{
        auto d=GltfPbr::parse(Json::object());
        check(d.base==std::array<float,4>{1,1,1,1},"Base color default");check(d.emissive==std::array<float,4>{0,0,0,1},"Emissive defaults");
        check(d.surface==std::array<float,4>{1,1,1,1},"Core factor defaults");check(d.specular==std::array<float,4>{1,1,1,1},"Specular defaults");
        check(d.coat==std::array<float,4>{0,0,1,1.5f},"Clearcoat and IOR defaults");check(d.sheen==std::array<float,4>{0,0,0,0},"Sheen defaults");
        check(d.iridescence==std::array<float,4>{0,1.3f,100,400},"Iridescence defaults");check(d.transmission[0]==0&&d.transmission[1]==0&&d.transmission[2]>1e30f&&d.transmission[3]==0,"Transmission/volume/dispersion defaults");
        check(d.attenuation==std::array<float,4>{1,1,1,0},"Attenuation defaults");check(d.anisotropy[0]==0&&d.anisotropy[1]==0,"Anisotropy defaults");check(d.flags==std::array<int,4>{0,0,0,0}&&d.alpha[0]==0.5f,"Alpha/default flags");
        for(auto& t:d.textures)check(t.info[0]==-1&&t.transform==std::array<float,4>{0,0,1,1}&&t.sampling==std::array<float,4>{1,0,0,0},"Absent texture must be neutral");
        Json extensions=Json::object();for(const auto* name:{"anisotropy","clearcoat","dispersion","emissive_strength","ior","iridescence","sheen","specular","transmission","volume"})extensions[std::string("KHR_materials_")+name]=Json::object();
        auto empty=GltfPbr::parse({{"extensions",extensions}});check(empty.coat==d.coat&&empty.iridescence==d.iridescence&&empty.specular==d.specular&&empty.transmission==d.transmission,"Empty extensions must preserve defaults");
        auto t=GltfPbr::texture({{"index",3},{"texCoord",0},{"extensions",{{"KHR_texture_transform",{{"offset",{0.2f,0.3f}},{"scale",{2,3}},{"rotation",0.5f},{"texCoord",1}}}}}});
        check(t.info[0]==3&&t.sampling[2]==1&&near(t.sampling[0],std::cos(0.5f))&&near(t.transform[1],0.3f)&&t.transform[3]==3,"Per-texture transform / UV override");
        size_t count=0;auto manifest=read_json("data/gltf_pbr/manifest.json");
        for(const auto& f:manifest["features"]){for(const auto& path:{f["model"].get<std::string>(),"data/gltf_pbr/"+f["id"].get<std::string>()+"_missing.glb"}){auto model=read_glb(path);for(const auto& m:model.value("materials",Json::array())){auto parsed=GltfPbr::parse(m);for(float value:parsed.base)check(std::isfinite(value),"Non-finite base color");check(std::isfinite(parsed.coat[3])&&std::isfinite(parsed.transmission[2]),"Non-finite extension factor");++count;}}}
        auto none=read_glb("data/gltf_pbr/default_material_missing.glb");check(!none.contains("materials"),"Material-free fixture");
        std::cout<<"PASS: core and all 10 extension defaults, absent textures, texture transforms, "<<count<<" official/omission materials, and material-free fixture.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
