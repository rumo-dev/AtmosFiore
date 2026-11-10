#pragma once

#include "scene_base.h"
#include "scene_manager.h"
#include "Engine/System/graphics_core.h"
#include "Engine/System/Manager/resource_manager.h"
#include "Engine/Graphics/Model/Tiny/json.hpp"
#include "Engine/Graphics/IBL/ibl.h"
#include "Game/World/camera/spectator_camera.h"
#include "Game/World/camera/camera_manager.h"
#include "Game/Effect/VoxelGI/VoxelGI.h"
#include "Game/Effect/DDGI/DDGI.h"
#include "Game/World/Light/directional_light.h"
#include "showcase_reflection_probe.h"
#include <filesystem>
#include <fstream>
#include <cmath>

// Self-contained exhibits share the production rendering path. Scene-specific
// geometry, camera, lighting and comparison profiles live in data/showcases.
class Scene_TechnologyShowcase final : public Scene_Base
{
    using Json = nlohmann::json;
public:
    explicit Scene_TechnologyShowcase(Json exhibit, bool automated=false) : exhibit_(std::move(exhibit)),automated_(automated) {}
    static Json read_catalog() { std::ifstream input("data/showcases/catalog.json");Json result;input>>result;return result; }
    void set_enabled(bool enabled) { enabled_=enabled;time_=0;apply_comparison(); }
    const std::string& error() const { return error_; }
    static void draw_selector(Scene_Manager& manager)
    {
        static const Json catalog = [] {
            std::ifstream input("data/showcases/catalog.json");
            try { Json result; input >> result; return result; }
            catch (...) { return Json::array(); }
        }();
        if (catalog.empty()) { ImGui::TextWrapped("Showcase catalog is missing. Run node tools/generate_technology_showcases.js."); return; }
        ImGui::TextWrapped("Each exhibit has a fixed camera, observation guide and technique comparison.");
        static ImGuiTextFilter filter;
        filter.Draw("Find technique");
        ImGui::BeginChild("Technology catalog", ImVec2(0, 300), true);
        std::string category;
        for (const auto& entry : catalog)
        {
            if (!filter.PassFilter(entry.at("title").get<std::string>().c_str())) continue;
            const auto next = entry.at("category").get<std::string>();
            if (next != category) { ImGui::SeparatorText(next.c_str()); category = next; }
            if (ImGui::Button(entry.at("title").get<std::string>().c_str()))
            {
                manager.change_scene(new Scene_TechnologyShowcase(entry));
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.at("description").get<std::string>().c_str());
        }
        ImGui::EndChild();
    }
    void initialize() override
    {
        auto& g = Graphics_Core::instance();
        auto& r = Resource_Manager::instance();
        meshlets_was_=r.meshlet_culling_enabled(); occlusion_was_=r.occlusion_culling_enabled();
        id_ = exhibit_.at("id").get<std::string>();
        original_settings_ = Post_Process_Manager::GetSettingsFile();
        snapshot_ = (std::filesystem::temp_directory_path() /
            ("AtmosFiore_showcase_" + std::to_string(GetCurrentProcessId()) + ".json")).string();
        snapshot_valid_ = Post_Process_Manager::WriteSettingsSnapshot(snapshot_);
        dlss_was_ = g.post_procss.GetDLSS().IsEnabled();
        dlss_mode_was_ = g.post_procss.GetDLSS().GetMode();
        g.post_procss.GetDLSS().SetEnabled(false);
        auto& shadow = g.post_procss.GetShadow();
        pcss_was_ = shadow.directional_pcss_enabled;
        sun_radius_was_=shadow.directional_light_angular_radius;
        point_was_ = shadow.point_shadow_enabled;
        shadow_distance_was_ = shadow.light_view_size;
        auto& post=g.post_procss;
        fog_enabled_was_[0]=post.GetVolumetricFog().is_enabled;
        fog_enabled_was_[1]=post.GetHeightFog().is_enabled;
        fog_enabled_was_[2]=post.GetDistanceFog().is_enabled;
        fog_enabled_was_[3]=post.GetExponentialFog().is_enabled;
        shadow.light_view_size = 40;
        if(id_=="sun_shadow")shadow.directional_light_angular_radius=0.05f;
        for (const char* key : { "Library", "Indoor", "Player", "RenderTest_Main", "RenderTest_Secondary" }) r.remove_model_instance(key);
        clear_lights();
        model_key_ = "Showcase_" + id_;
        auto* model = r.load_model(model_key_, exhibit_.at("model").get<std::string>());
        if (!model) { error_ = "Could not load exhibit geometry."; return; }
        ModelInstance instance{};
        instance.model_key = model_key_;
        dx::XMStoreFloat4x4(&instance.world_transform, dx::XMMatrixIdentity());
        r.add_model_instance("Showcase_Main", instance);
        if(id_=="reflection_probes") { probe_=std::make_unique<ShowcaseReflectionProbe>();if(!probe_->initialize(g.get_device())){probe_.reset();error_="Local probe initialization failed.";} }
        if(id_=="frustum")for(int i=-3;i<=3;++i)if(i!=0) {
            ModelInstance other=instance;
            dx::XMStoreFloat4x4(&other.world_transform,dx::XMMatrixTranslation(i*18.0f,0,0));
            const std::string key="Showcase_Court_"+std::to_string(i);extra_keys_.push_back(key);r.add_model_instance(key,other);
        }
        light_.illuminance_lux = exhibit_.value("sun", 6500.0f);
        light_.set_time_of_day(14.0f, id_ == "sky");
        if (exhibit_.value("gi", false))
        {
            voxel_ = std::make_unique<VoxelGI>();
            voxel_->set_resolution(96);
            voxel_->set_emissive_multiplier(6.0f);
            voxel_->set_strength(1.0f);
            if (!voxel_->initialize(g.get_device(), model, instance.world_transform)) { voxel_.reset(); error_ = "Voxel volume initialization failed."; }
            if (voxel_ && id_ != "voxel")
            {
                ddgi_ = std::make_unique<DDGI>();
                ddgi_->set_probe_bounds({-3.2f,0.0f,-3.0f},{3.2f,4.0f,5.0f});
                if (!ddgi_->initialize(g.get_device())) { ddgi_.reset(); error_ = "DDGI initialization failed."; }
            }
        }
        if (id_ == "point_shadow") { PointLight p; p.position={0,2.7f,-2}; p.radius=15; p.intensity=18000; p.cast_shadows=true; g.get_point_light_manager().add_light(p); }
        if (id_ == "ssgi" || id_ == "spot" || id_ == "volumetric")
        {
            for (int i=0;i<2;++i) { SpotLight s; s.position={i?2.5f:-2.5f,3.2f,-2}; s.direction={0,-0.55f,1}; s.radius=30; s.intensity=14000; s.diffuseColor=i?dx::XMFLOAT4{0.15f,0.5f,1,1}:dx::XMFLOAT4{1,0.3f,0.08f,1}; s.innerAngle=0.18f; s.outerAngle=0.45f; g.get_spot_light_manager().add_light(s); }
        }
        if (id_ == "area") { AreaLight a; a.position={0,4,-1}; a.direction={0,-1,0}; a.width=5; a.height=3; a.intensity=8000; g.get_area_light_manager().add_light(a); }
        if (id_ == "taa" || id_ == "dlss" || id_ == "animation")
        {
            r.load_model("Showcase_Actor", "data/showcases/actor.gltf");
            ModelInstance moving{}; moving.model_key="Showcase_Actor"; moving.is_animation=true;
            dx::XMStoreFloat4x4(&moving.world_transform, dx::XMMatrixTranslation(0,1.5f,0));
            r.add_model_instance("Showcase_Moving", moving);
        }
        IBL::Initialize(g.get_device(), L"data/ibl");
        reset_camera();
        apply_comparison();
    }
    void finalize() override
    {
        auto& g = Graphics_Core::instance(); auto* context=g.get_device_context();
        g.post_procss.GetVolumetricFog().set_gi_sources(nullptr,nullptr);
        g.post_procss.GetVolumetricFog().set_gi_only_debug(false);
        ID3D11ShaderResourceView* empty[2]{}; ID3D11Buffer* buffers[2]{};
        context->PSSetShaderResources(13,2,empty); context->PSSetConstantBuffers(8,2,buffers);
        ID3D11ShaderResourceView* no_probes[3]{};ID3D11Buffer* no_cb=nullptr;context->PSSetShaderResources(36,3,no_probes);context->PSSetConstantBuffers(10,1,&no_cb);probe_.reset();
        ddgi_.reset(); voxel_.reset();
        Resource_Manager::instance().remove_model_instance("Showcase_Main");
        Resource_Manager::instance().remove_model_instance("Showcase_Moving");
        for(const auto& key:extra_keys_)Resource_Manager::instance().remove_model_instance(key);
        Resource_Manager::instance().set_meshlet_culling_enabled(meshlets_was_);
        Resource_Manager::instance().set_occlusion_culling_enabled(occlusion_was_);
        clear_lights();
        auto& s=g.post_procss.GetShadow(); s.directional_pcss_enabled=pcss_was_; s.point_shadow_enabled=point_was_; s.light_view_size=shadow_distance_was_;s.directional_light_angular_radius=sun_radius_was_;
        if(snapshot_valid_) { Post_Process_Manager::RestoreSettingsSnapshot(snapshot_, original_settings_); std::error_code ec; std::filesystem::remove(snapshot_,ec); }
        else Post_Process_Manager::UseSettingsFile(original_settings_);
        auto& post=g.post_procss;
        post.GetVolumetricFog().is_enabled=fog_enabled_was_[0];
        post.GetHeightFog().is_enabled=fog_enabled_was_[1];
        post.GetDistanceFog().is_enabled=fog_enabled_was_[2];
        post.GetExponentialFog().is_enabled=fog_enabled_was_[3];
        g.post_procss.GetDLSS().SetMode(dlss_mode_was_); g.post_procss.GetDLSS().SetEnabled(dlss_was_);
        IBL::Bind(context);
    }
    void update(float elapsed) override
    {
        auto& g=Graphics_Core::instance();
        if (animate_) time_+=elapsed;
        if(!automated_)CameraManager::instance().update(elapsed,g.get_aspect_ratio(),g.get_screen_height());
        else {auto& lens=camera_->get_camera();lens.update_derived_parameters(g.get_aspect_ratio(),g.get_screen_height());lens.identity();}
        if(id_=="sky") light_.tick(animate_?elapsed:0);
        if(id_=="adaptation") light_.illuminance_lux=std::sin(time_*0.5f)>0?25000.0f:80.0f;
        if(id_=="ibl") light_.illuminance_lux=0;
        light_.update();
        if(auto* m=Resource_Manager::instance().get_model_instance("Showcase_Moving")) {
            dx::XMStoreFloat4x4(&m->world_transform,dx::XMMatrixTranslation(id_=="animation"?0:std::sin(time_)*2.5f,1.5f,0));
        }
        auto& points=g.get_point_light_manager().get_lights();
        if(id_=="point_shadow" && !points.empty()) { points[0].position.x=std::sin(time_*0.5f)*2; points[0].cast_shadows=enabled_; }
        g.get_point_light_manager().update(); g.get_point_light_manager().upload_to_gpu(g.get_device_context());
        g.get_spot_light_manager().update(); g.get_spot_light_manager().upload_to_gpu(g.get_device_context());
        g.get_area_light_manager().update(); g.get_area_light_manager().upload_to_gpu(g.get_device_context());
        Resource_Manager::instance().update_models(animate_?elapsed:0);
        IBL::Bind(g.get_device_context());
        if((id_=="ibl"&&!enabled_)||!exhibit_.value("environment",true)) { ID3D11ShaderResourceView* empty[3]{}; g.get_device_context()->PSSetShaderResources(32,3,empty); }
    }
    void render(float) override
    {
        auto& g=Graphics_Core::instance(); auto* c=g.get_device_context(); auto& r=Resource_Manager::instance(); auto& s=g.post_procss.GetShadow();
        Render_State::instance().set_3d_render_states(c,Rasterizer_State::Cull_None_CW);
        s.make_directional_shadow_begin(); g.update_constants(); r.render_models(pass_mode::directional_shadow); s.make_shadow_end();
        if(probe_)if(auto* m=r.get_model_instance("Showcase_Main"))probe_->capture(c,r.get_model(model_key_),m->world_transform);
        if(probe_)probe_->bind(c,enabled_);
        bool clear=true;
        const auto& lights=g.get_point_light_manager().get_lights();
        for(int i=0;i<static_cast<int>(lights.size());++i) if(s.point_shadow_enabled&&lights[i].shadow_tile>=0) {
            for(auto face:{shadow::PointShadowFace::Front,shadow::PointShadowFace::Back}) { s.make_shadow_begin(face,i,clear); r.render_models(pass_mode::shadow); s.make_shadow_end(); }
            clear=false;
        }
        if(voxel_) { if(auto* m=r.get_model_instance("Showcase_Main")) voxel_->render(c,r.get_model(model_key_),m->world_transform); if(ddgi_)ddgi_->update(c,*voxel_); }
        g.get_geometry_buffer()->Clear(c); g.get_geometry_buffer()->Activate(c);
        // These generated meshes use identity world transforms in LH space.
        // Their winding needs CW front faces; CCW inverted the G-buffer normals.
        Render_State::instance().set_deferred_render_states(c,Rasterizer_State::Cull_None_CW);
        r.begin_model_profile(c); r.render_models(pass_mode::deferred_geometry); r.issue_model_occlusion_queries(c); r.end_model_profile(c);
        g.get_geometry_buffer()->Deactivate(c);
        ID3D11ShaderResourceView* empty[2]{}; ID3D11Buffer* buffers[2]{};
        c->PSSetShaderResources(13,2,empty); c->PSSetConstantBuffers(8,2,buffers);
        if(enabled_&&voxel_&&id_=="voxel")voxel_->bind_lighting(c);
        if(enabled_&&ddgi_)ddgi_->bind_lighting(c);
        c->OMSetRenderTargets(1,g.get_render_target_view().GetAddressOf(),nullptr);
        Render_State::instance().set_2d_render_states(c); g.clear({0.015f,0.02f,0.03f,1});
        g.post_procss.begin();
        Render_State::instance().set_3d_render_states(c,Rasterizer_State::Cull_None_CW,Depth_State::Test_Enable_Write_Disable);
        r.render_models(pass_mode::forward_transparency); g.post_procss.end();
        auto& fog=g.post_procss.GetVolumetricFog();
        fog.set_gi_sources(enabled_?ddgi_.get():nullptr,enabled_?voxel_.get():nullptr);
        fog.set_gi_only_debug(id_=="ddgi_fog");
        g.post_procss.draw(); g.post_procss.render();
        if(voxel_)voxel_->render_debug_3d(c); if(ddgi_)ddgi_->render_debug(c);
    }
    void draw_gui() override
    {
        ImGui::SeparatorText(exhibit_.at("title").get<std::string>().c_str());
        ImGui::TextWrapped("%s",exhibit_.at("description").get<std::string>().c_str());
        if(!error_.empty())ImGui::TextWrapped("%s",error_.c_str());
        const char* comparison=id_=="sun_shadow"?"PCSS (off = PCF reference)":id_=="sky"?"Animate sun / sky":"Technique enabled";
        if(id_!="pbr" && ImGui::Checkbox(comparison,&enabled_))apply_comparison();
        ImGui::SameLine(); if(ImGui::Button("Reset camera"))reset_camera();
        ImGui::Checkbox("Animate demonstration",&animate_);
        ImGui::TextDisabled("Move: W/A/S/D, Q/E. Look: right mouse. Shift: faster.");
        ImGui::Text("%.1f FPS / %.2f ms",ImGui::GetIO().Framerate,1000.0f/(std::max)(ImGui::GetIO().Framerate,1.0f));
        if(id_=="dlss") { auto& d=Graphics_Core::instance().post_procss.GetDLSS(); if(!d.IsAvailable())ImGui::TextWrapped("DLSS unavailable: %s. Native TAA remains active.",d.GetUnavailableReason().c_str()); }
        if(id_=="frustum"||id_=="meshlets"||id_=="occlusion")Resource_Manager::instance().draw_model_debug();
        if(id_=="sun_shadow"||id_=="point_shadow")Graphics_Core::instance().post_procss.GetShadow().shadow_gui();
        if(voxel_)voxel_->draw_gui(); if(ddgi_)ddgi_->draw_gui();
        light_.draw_imgui("Exhibit sun");
    }
private:
    void clear_lights() { auto& g=Graphics_Core::instance();g.get_point_light_manager().clear_light();g.get_spot_light_manager().clear_light();g.get_area_light_manager().clear_light(); }
    void reset_camera()
    {
        auto& g=Graphics_Core::instance(); camera_=std::make_shared<SpectatorCamera>(g.get_window_handle());
        const auto p=exhibit_.at("camera").get<std::vector<float>>(), t=exhibit_.at("target").get<std::vector<float>>();
        const float pitch=dx::XMConvertToDegrees(std::atan2(t[1]-p[1],std::hypot(t[0]-p[0],t[2]-p[2])));
        const float yaw=dx::XMConvertToDegrees(std::atan2(t[2]-p[2],t[0]-p[0]));
        camera_->set_rotation(yaw,pitch,dx::XMVectorSet(p[0],p[1],p[2],1),dx::XMVectorSet(t[0],t[1],t[2],1));
        auto& lens=camera_->get_camera(); lens.FocalLength=28; lens.SensorSize=36; lens.FNumber=id_=="dof"?1.4f:8; lens.FocusDist=exhibit_.value("focus",9.0f); lens.ISO=400; lens.shutterDenominator=60; lens.EV_Compensation=0; lens.autoSettingsEnabled=false; lens.near_z=0.08f; lens.far_z=120;
        CameraManager::instance().register_camera("Technology Showcase",camera_); CameraManager::instance().switch_camera("Technology Showcase"); Post_Process_Manager::ResetTemporalHistory();
    }
    void apply_comparison()
    {
        if(!Post_Process_Manager::UseSettingsFile(exhibit_.at(enabled_?"profile":"off_profile").get<std::string>()))error_="Could not load comparison profile.";
        auto& g=Graphics_Core::instance(); auto& s=g.post_procss.GetShadow();
        g.post_procss.GetVolumetricFog().is_enabled=true;
        g.post_procss.GetHeightFog().is_enabled=true;
        g.post_procss.GetDistanceFog().is_enabled=true;
        g.post_procss.GetExponentialFog().is_enabled=true;
        s.directional_pcss_enabled=id_=="sun_shadow"?enabled_:true; s.point_shadow_enabled=1;
        if(id_=="area")for(auto& a:g.get_area_light_manager().get_lights())a.intensity=enabled_?8000.0f:0;
        if(id_=="spot")for(auto& a:g.get_spot_light_manager().get_lights())a.intensity=enabled_?14000.0f:0;
        auto& resources=Resource_Manager::instance();
        if(id_=="frustum") {
            resources.set_meshlet_culling_enabled(false);resources.set_occlusion_culling_enabled(false);
            if(auto* m=resources.get_model_instance("Showcase_Main"))m->disable_frustum_culling=!enabled_;
            for(const auto& key:extra_keys_)if(auto* m=resources.get_model_instance(key))m->disable_frustum_culling=!enabled_;
        }
        if(id_=="meshlets") { resources.set_meshlet_culling_enabled(enabled_);resources.set_occlusion_culling_enabled(false); }
        if(id_=="occlusion") { resources.set_meshlet_culling_enabled(true);resources.set_occlusion_culling_enabled(enabled_); }
        if(id_=="sky") { light_.set_time_of_day(14,enabled_); }
        if(id_=="animation")animate_=enabled_;
        if(id_=="dlss") { auto& d=g.post_procss.GetDLSS(); d.SetMode(DLSSManager::Mode::Quality);d.SetEnabled(enabled_&&d.IsAvailable()); }
        Post_Process_Manager::ResetTemporalHistory();
    }
    Json exhibit_; std::string id_,model_key_,snapshot_,original_settings_,error_;
    DirectionalLight light_; std::shared_ptr<SpectatorCamera> camera_;
    std::unique_ptr<VoxelGI> voxel_; std::unique_ptr<DDGI> ddgi_;
    bool enabled_=true,animate_=true,snapshot_valid_=false,pcss_was_=true,dlss_was_=false;
    int point_was_=1; float shadow_distance_was_=60,time_=0;
    DLSSManager::Mode dlss_mode_was_=DLSSManager::Mode::Quality;
    bool fog_enabled_was_[4]{true,true,true,true};
    bool meshlets_was_=true,occlusion_was_=true;
    std::vector<std::string> extra_keys_;
    std::unique_ptr<ShowcaseReflectionProbe> probe_;
    bool automated_=false;
    float sun_radius_was_=0.025f;
};
