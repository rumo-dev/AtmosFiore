#pragma once
#include "Tiny/json.hpp"
#include <array>
#include <cmath>
#include <limits>

// Explicit 32-bit fields: this is also the StructuredBuffer ABI in HLSL.
namespace GltfPbr
{
constexpr int TextureCount=17;
struct Texture
{
    std::array<float,4> transform{0,0,1,1}; // offset.xy, scale.xy
    std::array<float,4> sampling{1,0,0,0}; // cos(rotation), sin(rotation), UV set, reserved
    std::array<int,4> info{-1,0,0,0}; // glTF texture index, GPU sampler slot, reserved
};
struct Material
{
    std::array<float,4> base{1,1,1,1},emissive{0,0,0,1},surface{1,1,1,1};
    std::array<float,4> specular{1,1,1,1},coat{0,0,1,1.5f},sheen{0,0,0,0};
    std::array<float,4> iridescence{0,1.3f,100,400},transmission{0,0,(std::numeric_limits<float>::max)(),0};
    std::array<float,4> attenuation{1,1,1,0},anisotropy{0,0,0,0};
    std::array<int,4> flags{0,0,0,0}; // alpha mode, double sided, unlit, extension presence
    std::array<float,4> alpha{0.5f,0,0,0};
    std::array<Texture,TextureCount> textures{};
};
static_assert(sizeof(Texture)==48&&sizeof(Material)==1008,"PBR CPU/GPU material ABI changed");
using Json=nlohmann::json;
inline float scalar(const Json& j,const char* key,float fallback)
{auto it=j.find(key);return it!=j.end()&&it->is_number()&&std::isfinite(it->get<float>())?it->get<float>():fallback;}
inline Json object(const Json& j,const char* key)
{auto it=j.find(key);return it!=j.end()&&it->is_object()?*it:Json::object();}
template<size_t N> inline void vector(const Json& j,const char* key,std::array<float,N>& out,size_t count=N)
{auto it=j.find(key);if(it==j.end()||!it->is_array())return;for(size_t i=0;i<(std::min)(count,it->size());++i)if((*it)[i].is_number()){float x=(*it)[i].get<float>();if(std::isfinite(x))out[i]=x;}}
inline Texture texture(const Json& j)
{
    Texture t;t.info[0]=static_cast<int>(scalar(j,"index",-1));t.sampling[2]=scalar(j,"texCoord",0);
    auto transform=object(object(j,"extensions"),"KHR_texture_transform");
    std::array<float,4> offset{0,0,0,0},scale{1,1,0,0};vector(transform,"offset",offset,2);vector(transform,"scale",scale,2);
    t.transform={offset[0],offset[1],scale[0],scale[1]};const float angle=scalar(transform,"rotation",0);t.sampling[0]=std::cos(angle);t.sampling[1]=std::sin(angle);t.sampling[2]=scalar(transform,"texCoord",t.sampling[2]);return t;
}
inline Material parse(const Json& j)
{
    Material m;auto p=object(j,"pbrMetallicRoughness"),e=object(j,"extensions");
    vector(p,"baseColorFactor",m.base);vector(j,"emissiveFactor",m.emissive,3);
    m.surface[0]=scalar(p,"metallicFactor",1);m.surface[1]=scalar(p,"roughnessFactor",1);
    m.surface[2]=scalar(object(j,"normalTexture"),"scale",1);m.surface[3]=scalar(object(j,"occlusionTexture"),"strength",1);
    m.textures[0]=texture(object(p,"baseColorTexture"));m.textures[1]=texture(object(p,"metallicRoughnessTexture"));
    m.textures[2]=texture(object(j,"normalTexture"));m.textures[3]=texture(object(j,"emissiveTexture"));m.textures[4]=texture(object(j,"occlusionTexture"));
    const auto mode=j.value("alphaMode",std::string("OPAQUE"));m.flags[0]=mode=="MASK"?1:mode=="BLEND"?2:0;
    m.flags[1]=j.value("doubleSided",false)?1:0;m.alpha[0]=scalar(j,"alphaCutoff",0.5f);
    auto c=object(e,"KHR_materials_clearcoat");m.coat[0]=scalar(c,"clearcoatFactor",0);m.coat[1]=scalar(c,"clearcoatRoughnessFactor",0);m.coat[2]=scalar(object(c,"clearcoatNormalTexture"),"scale",1);m.coat[3]=scalar(object(e,"KHR_materials_ior"),"ior",1.5f);
    m.textures[5]=texture(object(c,"clearcoatTexture"));m.textures[6]=texture(object(c,"clearcoatRoughnessTexture"));m.textures[7]=texture(object(c,"clearcoatNormalTexture"));
    auto s=object(e,"KHR_materials_specular");vector(s,"specularColorFactor",m.specular,3);m.specular[3]=scalar(s,"specularFactor",1);m.textures[12]=texture(object(s,"specularTexture"));m.textures[13]=texture(object(s,"specularColorTexture"));
    auto sh=object(e,"KHR_materials_sheen");vector(sh,"sheenColorFactor",m.sheen,3);m.sheen[3]=scalar(sh,"sheenRoughnessFactor",0);m.textures[10]=texture(object(sh,"sheenColorTexture"));m.textures[11]=texture(object(sh,"sheenRoughnessTexture"));
    auto tr=object(e,"KHR_materials_transmission"),v=object(e,"KHR_materials_volume");m.transmission[0]=scalar(tr,"transmissionFactor",0);m.transmission[1]=scalar(v,"thicknessFactor",0);m.transmission[2]=scalar(v,"attenuationDistance",(std::numeric_limits<float>::max)());m.transmission[3]=scalar(object(e,"KHR_materials_dispersion"),"dispersion",0);vector(v,"attenuationColor",m.attenuation,3);
    m.textures[8]=texture(object(tr,"transmissionTexture"));m.textures[9]=texture(object(v,"thicknessTexture"));
    auto ir=object(e,"KHR_materials_iridescence");m.iridescence={scalar(ir,"iridescenceFactor",0),scalar(ir,"iridescenceIor",1.3f),scalar(ir,"iridescenceThicknessMinimum",100),scalar(ir,"iridescenceThicknessMaximum",400)};m.textures[15]=texture(object(ir,"iridescenceTexture"));m.textures[16]=texture(object(ir,"iridescenceThicknessTexture"));
    auto an=object(e,"KHR_materials_anisotropy");m.anisotropy[0]=scalar(an,"anisotropyStrength",0);m.anisotropy[1]=scalar(an,"anisotropyRotation",0);m.textures[14]=texture(object(an,"anisotropyTexture"));
    m.emissive[3]=scalar(object(e,"KHR_materials_emissive_strength"),"emissiveStrength",1);
    m.flags[2]=e.contains("KHR_materials_unlit")?1:0;
    for(const auto& kv:e.items())if(kv.key().rfind("KHR_materials_",0)==0&&kv.key()!="KHR_materials_emissive_strength")m.flags[3]=1;
    for(const auto& t:m.textures)if(t.sampling[2]!=0||t.transform!=std::array<float,4>{0,0,1,1}||t.sampling[0]!=1||t.sampling[1]!=0)m.flags[3]=1;
    return m;
}
}
