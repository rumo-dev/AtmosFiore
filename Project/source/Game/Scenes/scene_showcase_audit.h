#pragma once
#include "scene_technology_showcase.h"
#include "scene_indoor.h"
#include <ScreenGrab.h>
#include <wincodec.h>

// Reproducible GPU integration check: capture every generated exhibit, both
// enabled and disabled, without desktop input or GUI obscuring the result.
class Scene_ShowcaseAudit final : public Scene_Base
{
    using Json=nlohmann::json;
public:
    explicit Scene_ShowcaseAudit(bool pbrOnly=false,bool captureAll=false,bool exitWhenDone=true,bool indoorOnly=false,std::string selectedId={}):pbr_only_(pbrOnly),capture_all_(captureAll),exit_when_done_(exitWhenDone),indoor_only_(indoorOnly),selected_id_(std::move(selectedId)){}
    static bool capture_active(){return capture_active_;}
    void initialize() override {
        catalog_=Scene_TechnologyShowcase::read_catalog();
        if(!selected_id_.empty()){Json selected=Json::array();for(const auto& entry:catalog_)if(entry.value("id",std::string{})==selected_id_)selected.push_back(entry);catalog_=std::move(selected);if(catalog_.empty()){PostQuitMessage(2);return;}}
        if(indoor_only_)catalog_=Json::array();
        if(capture_all_){auto& post=Graphics_Core::instance().post_procss;dlss_was_=post.GetDLSS().IsEnabled();dlss_mode_was_=post.GetDLSS().GetMode();point_shadow_was_=post.GetShadow().point_shadow_enabled;pcss_was_=post.GetShadow().directional_pcss_enabled;}
        if(capture_all_){capture_active_=true;meshlets_was_=Resource_Manager::instance().meshlet_culling_enabled();occlusion_was_=Resource_Manager::instance().occlusion_culling_enabled();directory_="data/capture/screenshots";if(selected_id_.empty())catalog_.push_back({{"id","Indoor"},{"title","Indoor"},{"indoor",true}});}
        if(pbr_only_){Json selected=Json::array();for(const auto& entry:catalog_)if(entry.value("pbr_asset",false))selected.push_back(entry);
            const auto normal=selected;for(auto entry:normal){entry["id"]=entry.at("id").get<std::string>()+"_missing";entry["model"]=entry.at("missing_model");entry.erase("display_model");entry["pbr_feature"]=0;entry["missing"]=true;selected.push_back(entry);}
            auto defaults=normal[4];defaults["id"]="gltf_pbr_default_material";defaults["model"]="data/gltf_pbr/default_material_missing.glb";defaults.erase("display_model");defaults["pbr_feature"]=0;defaults["missing"]=true;selected.push_back(defaults);catalog_=selected;directory_="data/gltf_pbr/audit";}
        if(indoor_only_)directory_="data/capture/indoor";
        std::filesystem::create_directories(directory_);std::ofstream(std::filesystem::path(directory_)/"report.json")<<"[]";start();
    }
    void finalize() override { if(scene_){scene_->finalize();scene_.reset();} if(capture_all_){Resource_Manager::instance().set_meshlet_culling_enabled(meshlets_was_);Resource_Manager::instance().set_occlusion_culling_enabled(occlusion_was_);auto& post=Graphics_Core::instance().post_procss;post.GetDLSS().SetMode(dlss_mode_was_);post.GetDLSS().SetEnabled(dlss_was_);post.GetShadow().point_shadow_enabled=point_shadow_was_;post.GetShadow().directional_pcss_enabled=pcss_was_;capture_active_=false;} }
    void update(float) override
    {
        if(finished_)return;
        if(advance_){advance_=false;scene_->finalize();scene_.reset();if(off_){off_=false;++index_;}else off_=true;
            if(index_>=catalog_.size()){finished_=true;if(exit_when_done_)PostQuitMessage(0);else if(!selected_id_.empty())Scene_Manager::instance().change_scene(new Scene_TechnologyShowcase(catalog_[0]));else Scene_Manager::instance().change_scene(new Scene_Indoor());return;}start();}
        if(scene_){
            if(capture_all_&&!off_&&catalog_[index_].value("capture",std::string("slider"))=="video")
                if(auto* exhibit=dynamic_cast<Scene_TechnologyShowcase*>(scene_.get())){
                    float phase=frames_<91?0.0f:std::fmod((static_cast<float>(frames_+1)-92.0f)/120.0f,1.0f);
                    if(phase<0)phase+=1.0f;
                    exhibit->set_capture_phase(phase);
                }
            scene_->update(1.0f/60.0f);updated_=true;
        }
    }
    void render(float) override
    {
        if(!scene_)return;scene_->render(1.0f/60.0f);
        if(advance_||!updated_)return;
        ++frames_;
        const bool video=capture_all_&&!off_&&catalog_[index_].value("capture",std::string("slider"))=="video";
        if(video&&frames_>90&&frames_<=210&&frames_%2==0)
        {
            auto& graphics=Graphics_Core::instance();
            Microsoft::WRL::ComPtr<ID3D11Resource> frame;
            graphics.get_render_target_view()->GetResource(frame.GetAddressOf());
            const auto folder=std::filesystem::path(directory_)/"video"/catalog_[index_].at("id").get<std::string>();
            std::filesystem::create_directories(folder);
            const auto path=folder/(std::to_string(10000+(frames_-90)/2).substr(1)+".png");
            const HRESULT frame_hr=DirectX::SaveWICTextureToFile(graphics.get_device_context(),frame.Get(),GUID_ContainerFormatPng,path.c_str());
            if(SUCCEEDED(frame_hr))video_frames_.push_back(path.generic_string());
            else video_error_=static_cast<long>(frame_hr);
        }
        if(frames_<(catalog_[index_].value("indoor",false)?180:video?210:90))return;
        auto& g=Graphics_Core::instance();auto* c=g.get_device_context();
        Microsoft::WRL::ComPtr<ID3D11Resource> back;g.get_render_target_view()->GetResource(back.GetAddressOf());
        const std::string id=catalog_[index_].at("id").get<std::string>(),name=id+(off_?"_off":"_on");
        const auto file=std::filesystem::path(directory_)/(name+".png");
        const auto* exhibit=dynamic_cast<Scene_TechnologyShowcase*>(scene_.get());
        Json record={{"id",id},{"title",catalog_[index_].value("title",id)},{"enabled",!off_},{"error",exhibit?exhibit->error():""},{"image",file.generic_string()},{"imgui",false}};
        record["capture"]=catalog_[index_].value("capture",std::string("slider"));
        if(id=="frustum"||id=="meshlets"||id=="occlusion"){
            const auto& resources=Resource_Manager::instance();const auto& stats=resources.camera_meshlet_stats();
            record["culling"]={{"instances_total",resources.camera_instances_total()},{"instances_culled",resources.camera_instances_culled()},{"meshlets_tested",stats.tested},{"meshlets_culled",stats.culled},{"frustum_skipped_triangles",stats.skipped_triangles},{"occlusion_groups_tested",stats.occlusion_tested},{"occlusion_groups_culled",stats.occlusion_culled},{"occlusion_skipped_triangles",stats.occlusion_skipped_triangles},{"queries_issued_since_scene_start",exhibit?exhibit->occlusion_queries_issued():0},{"stationary_camera_frames",resources.stationary_camera_frames()}};
        }
        if(video){record["video_frames"]=video_frames_;record["video_fps"]=30;record["video_capture_hresult"]=video_error_;record["video_loop"]=true;}
        const auto& lens=CameraManager::instance().get_active_camera()->get_camera();DirectX::XMFLOAT3 eye,target;DirectX::XMStoreFloat3(&eye,lens.position);DirectX::XMStoreFloat3(&target,lens.target);record["camera_position"]={eye.x,eye.y,eye.z};record["camera_target"]={target.x,target.y,target.z};
        if(id=="occlusion"&&exhibit){const auto observer=exhibit->observer_eye();record["culling_camera_position"]=record["camera_position"];record["camera_position"]={observer.x,observer.y,observer.z};record["camera_target"]={0,1.5,5};}
        if(catalog_[index_].value("indoor",false)){record["mode"]=off_?"render_only":"all_features";record["dlss_available"]=g.post_procss.GetDLSS().IsAvailable();record["dlss_enabled"]=g.post_procss.GetDLSS().IsEnabled();}
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
        if(video){
            const auto folder=std::filesystem::path(directory_)/"video"/id;
            const auto output=std::filesystem::path(directory_)/"video"/(id+".mp4");
            const DWORD encode_code=video_frames_.size()==60&&video_error_==0?encode_video(folder,output):ERROR_INVALID_DATA;
            record["video_encode_exit_code"]=encode_code;
            if(encode_code==0)record["video"]=output.generic_string();
        }
        report_.push_back(record);std::ofstream(std::filesystem::path(directory_)/"report.json")<<report_.dump(2);advance_=true;
    }
    void draw_gui() override {}
private:
    static DWORD encode_video(const std::filesystem::path& folder,const std::filesystem::path& output)
    {
        wchar_t found[MAX_PATH]{};
        std::wstring executable;
        const DWORD length=SearchPathW(nullptr,L"ffmpeg.exe",nullptr,MAX_PATH,found,nullptr);
        if(length>0&&length<MAX_PATH)executable=found;
        if(executable.empty()){
            wchar_t profile[MAX_PATH]{};
            const DWORD size=GetEnvironmentVariableW(L"USERPROFILE",profile,MAX_PATH);
            if(size>0&&size<MAX_PATH){const auto shim=std::filesystem::path(profile)/L"scoop"/L"shims"/L"ffmpeg.exe";if(std::filesystem::exists(shim))executable=shim.wstring();}
        }
        if(executable.empty())return ERROR_FILE_NOT_FOUND;
        std::error_code ec;std::filesystem::remove(output,ec);
        const auto input=(std::filesystem::absolute(folder)/L"%04d.png").wstring();
        const auto target=std::filesystem::absolute(output).wstring();
        std::wstring command=L"\""+executable+L"\" -hide_banner -loglevel error -y -framerate 30 -start_number 1 -i \""+input+L"\" -c:v libx264 -pix_fmt yuv420p -crf 18 -movflags +faststart \""+target+L"\"";
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
        PROCESS_INFORMATION process{};
        const auto cwd=std::filesystem::current_path().wstring();
        if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,cwd.c_str(),&startup,&process))return GetLastError();
        const DWORD wait=WaitForSingleObject(process.hProcess,120000);
        DWORD code=ERROR_GEN_FAILURE;
        if(wait==WAIT_TIMEOUT){TerminateProcess(process.hProcess,WAIT_TIMEOUT);code=WAIT_TIMEOUT;}
        else if(wait==WAIT_OBJECT_0)GetExitCodeProcess(process.hProcess,&code);
        else code=GetLastError();
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
        if(code!=0)std::filesystem::remove(output,ec);
        return code;
    }
    void start(){frames_=0;updated_=false;video_frames_=Json::array();video_error_=0;if(catalog_[index_].value("indoor",false))scene_=std::make_unique<Scene_Indoor>(true,!off_);else scene_=std::make_unique<Scene_TechnologyShowcase>(catalog_[index_],true);scene_->initialize();if(auto* exhibit=dynamic_cast<Scene_TechnologyShowcase*>(scene_.get()))exhibit->set_enabled(!off_);}
    Json catalog_,report_=Json::array(),video_frames_=Json::array();size_t index_=0;int frames_=0;long video_error_=0;bool off_=false,advance_=false,finished_=false;std::vector<unsigned char> previous_;std::unique_ptr<Scene_Base> scene_;
    bool pbr_only_=false;std::string directory_="data/showcases/audit";
    bool capture_all_=false,exit_when_done_=true;
    bool meshlets_was_=true,occlusion_was_=true;
    bool indoor_only_=false,updated_=false;
    std::string selected_id_;
    bool dlss_was_=false,pcss_was_=true;int point_shadow_was_=1;
    DLSSManager::Mode dlss_mode_was_=DLSSManager::Mode::Quality;
    inline static bool capture_active_=false;
};
