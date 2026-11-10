#pragma once
#include "scene_technology_showcase.h"
#include <ScreenGrab.h>
#include <wincodec.h>

// Reproducible GPU integration check: capture every generated exhibit, both
// enabled and disabled, without desktop input or GUI obscuring the result.
class Scene_ShowcaseAudit final : public Scene_Base
{
    using Json=nlohmann::json;
public:
    explicit Scene_ShowcaseAudit(bool pbrOnly=false):pbr_only_(pbrOnly){}
    void initialize() override {
        catalog_=Scene_TechnologyShowcase::read_catalog();
        if(pbr_only_){Json selected=Json::array();for(const auto& entry:catalog_)if(entry.value("pbr_asset",false))selected.push_back(entry);
            const auto normal=selected;for(auto entry:normal){entry["id"]=entry.at("id").get<std::string>()+"_missing";entry["model"]=entry.at("missing_model");entry["pbr_feature"]=0;entry["missing"]=true;selected.push_back(entry);}
            auto defaults=normal[4];defaults["id"]="gltf_pbr_default_material";defaults["model"]="data/gltf_pbr/default_material_missing.glb";defaults["pbr_feature"]=0;defaults["missing"]=true;selected.push_back(defaults);catalog_=selected;directory_="data/gltf_pbr/audit";}
        std::filesystem::create_directories(directory_);start();
    }
    void finalize() override { if(scene_){scene_->finalize();scene_.reset();} }
    void update(float) override
    {
        if(advance_){advance_=false;scene_->finalize();scene_.reset();if(off_){off_=false;++index_;}else off_=true;
            if(index_>=catalog_.size()){PostQuitMessage(0);return;}start();}
        if(scene_)scene_->update(1.0f/60.0f);
    }
    void render(float) override
    {
        if(!scene_)return;scene_->render(1.0f/60.0f);
        if(++frames_<90)return;
        auto& g=Graphics_Core::instance();auto* c=g.get_device_context();
        Microsoft::WRL::ComPtr<ID3D11Resource> back;g.get_render_target_view()->GetResource(back.GetAddressOf());
        const std::string id=catalog_[index_].at("id").get<std::string>(),name=id+(off_?"_off":"_on");
        const auto file=std::filesystem::path(directory_)/(name+".png");
        Json record={{"id",id},{"enabled",!off_},{"error",scene_->error()},{"image",file.generic_string()}};
        record["pbr"]=catalog_[index_].value("pbr_asset",false);record["missing"]=catalog_[index_].value("missing",false);
        const HRESULT hr=DirectX::SaveWICTextureToFile(c,back.Get(),GUID_ContainerFormatPng,file.c_str());record["capture_hresult"]=static_cast<long>(hr);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;back.As(&texture);D3D11_TEXTURE2D_DESC d{};texture->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=d.MiscFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
        if(SUCCEEDED(g.get_device()->CreateTexture2D(&d,nullptr,staging.GetAddressOf()))){c->CopyResource(staging.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
            if(SUCCEEDED(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped))){double sum=0;size_t visible=0;unsigned char lo=255,hi=0;std::vector<unsigned char> pixels;pixels.reserve(d.Width*d.Height*3);
                for(UINT y=0;y<d.Height;++y){auto* row=static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch;for(UINT x=0;x<d.Width;++x){int light=0;for(int k=0;k<3;++k){auto v=row[x*4+k];sum+=v;lo=(std::min)(lo,v);hi=(std::max)(hi,v);light+=v;pixels.push_back(v);}if(light>24)++visible;}}
                c->Unmap(staging.Get(),0);record["mean_rgb"]=sum/(d.Width*d.Height*3.0);record["visible_fraction"]=visible/static_cast<double>(d.Width*d.Height);record["min_rgb"]=lo;record["max_rgb"]=hi;
                if(off_&&previous_.size()==pixels.size()){double delta=0;for(size_t i=0;i<pixels.size();++i)delta+=std::abs(int(pixels[i])-int(previous_[i]));record["mean_absolute_on_off_difference"]=delta/pixels.size();}else previous_=std::move(pixels);
            }}
        ID3D11ShaderResourceView* views[GBUFFER_COUNT]{};g.get_geometry_buffer()->GetShaderResourceViews(views);
        Microsoft::WRL::ComPtr<ID3D11Resource> pos;views[2]->GetResource(pos.GetAddressOf());pos.As(&texture);texture->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=d.MiscFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;staging.Reset();
        if(SUCCEEDED(g.get_device()->CreateTexture2D(&d,nullptr,staging.GetAddressOf()))){c->CopyResource(staging.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE mapped{};if(SUCCEEDED(c->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped))){size_t covered=0;for(UINT y=0;y<d.Height;++y){auto* row=reinterpret_cast<const float*>(static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch);for(UINT x=0;x<d.Width;++x)if(std::abs(row[x*4])+std::abs(row[x*4+1])+std::abs(row[x*4+2])>1e-5f)++covered;}record["geometry_fraction"]=covered/static_cast<double>(d.Width*d.Height);c->Unmap(staging.Get(),0);}}
        report_.push_back(record);std::ofstream(std::filesystem::path(directory_)/"report.json")<<report_.dump(2);advance_=true;
    }
    void draw_gui() override {}
private:
    void start(){frames_=0;scene_=std::make_unique<Scene_TechnologyShowcase>(catalog_[index_],true);scene_->initialize();scene_->set_enabled(!off_);}
    Json catalog_,report_=Json::array();size_t index_=0;int frames_=0;bool off_=false,advance_=false;std::vector<unsigned char> previous_;std::unique_ptr<Scene_TechnologyShowcase> scene_;
    bool pbr_only_=false;std::string directory_="data/showcases/audit";
};
