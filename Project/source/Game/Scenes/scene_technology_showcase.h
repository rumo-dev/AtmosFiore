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
#include "Engine/Graphics/Model/debug_primitive.h"
#include "Engine/Graphics/UI/text/text.h"
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
    void set_enabled(bool enabled) { enabled_=enabled;time_=0;capture_phase_=-1;occlusion_last_camera_x_=1e9f;apply_comparison(); }
    void set_capture_phase(float phase) { capture_phase_=phase; }
    const std::string& id() const { return id_; }
    const std::string& error() const { return error_; }
    dx::XMFLOAT3 observer_eye() const { return observer_eye_; }
    uint64_t occlusion_queries_issued() const { return Resource_Manager::instance().total_occlusion_queries_issued()-occlusion_query_baseline_; }
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
            ImGui::SameLine();
            const auto capture_label="Capture##"+entry.at("id").get<std::string>();
            if(ImGui::SmallButton(capture_label.c_str()))manager.capture_showcase(entry.at("id").get<std::string>());
            if(ImGui::IsItemHovered())ImGui::SetTooltip("Save ON/OFF PNGs%s",entry.value("capture",std::string("slider"))=="video"?" and video frames":"");
        }
        ImGui::EndChild();
    }
    void initialize() override
    {
        auto& g = Graphics_Core::instance();
        auto& r = Resource_Manager::instance();
        meshlets_was_=r.meshlet_culling_enabled(); occlusion_was_=r.occlusion_culling_enabled();
        culling_frozen_was_=r.camera_culling_frozen();culling_frustum_was_=r.frozen_camera_frustum();
        id_ = exhibit_.at("id").get<std::string>();
        if(id_=="sky")sky_look_was_=g.post_procss.GetSky().look();
        occlusion_query_baseline_=r.total_occlusion_queries_issued();
        if(id_=="occlusion")r.restore_camera_culling_frustum(false,culling_frustum_was_);
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
        auto* model = r.load_model(model_key_, exhibit_.value("display_model",exhibit_.at("model").get<std::string>()));
        if (!model) { error_ = "Could not load exhibit geometry."; return; }
        ModelInstance instance{};
        instance.model_key = model_key_;
        dx::XMStoreFloat4x4(&instance.world_transform, dx::XMMatrixIdentity());
        if(exhibit_.value("pbr_asset",false))
        {
            model->set_full_pbr_enabled(true);
            dx::XMFLOAT3 minimum,maximum;dx::XMFLOAT4X4 conversion;dx::XMStoreFloat4x4(&conversion,dx::XMMatrixScaling(1,1,-1));
            if(exhibit_.contains("display_bounds")){
                const auto lo=exhibit_.at("display_bounds").at("min").get<std::vector<float>>(),hi=exhibit_.at("display_bounds").at("max").get<std::vector<float>>();
                minimum={lo[0],lo[1],lo[2]};maximum={hi[0],hi[1],hi[2]};
            }else model->compute_instance_aabb(conversion,false,0,Gltf_Model::animation_mode::single,0,minimum,maximum);
            const float width=(std::max)(maximum.x-minimum.x,0.01f),height=(std::max)(maximum.y-minimum.y,0.01f),depth=(std::max)(maximum.z-minimum.z,0.01f);
            const float extent=(std::max)({width,height,depth});
            // Fit the selected specimen, excluding checker backdrops.
            const float distance=(std::max)(width/1.15f,height/0.9f)+depth*0.5f+extent*0.05f;
            dx::XMStoreFloat4x4(&instance.world_transform,dx::XMMatrixScaling(1,1,-1)*dx::XMMatrixTranslation(-(minimum.x+maximum.x)*0.5f,-minimum.y,-(minimum.z+maximum.z)*0.5f));
            exhibit_["camera"]={0,height*0.5f+extent*0.02f,-distance};exhibit_["target"]={0,height*0.5f,0};exhibit_["focus"]=distance;
            exhibit_["near"]=(std::max)(distance*0.001f,0.0001f);exhibit_["far"]=distance*10;exhibit_["move_speed"]=extent*0.7f;
            shadow.light_view_size=(std::max)(extent*6,10.0f);
        }
        r.add_model_instance("Showcase_Main", instance);
        if(id_=="reflection_probes") { probe_=std::make_unique<ShowcaseReflectionProbe>();if(!probe_->initialize(g.get_device())){probe_.reset();error_="Local probe initialization failed.";} }
        if(id_=="frustum")for(int i=-3;i<=3;++i)if(i!=0) {
            ModelInstance other=instance;
            dx::XMStoreFloat4x4(&other.world_transform,dx::XMMatrixTranslation(i*18.0f,0,0));
            const std::string key="Showcase_Court_"+std::to_string(i);extra_keys_.push_back(key);r.add_model_instance(key,other);
        }
        light_.illuminance_lux = exhibit_.value("sun", 6500.0f);
        if(id_=="sky")light_.ambientColor={0.42f,0.35f,0.45f,1};
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
        if(id_=="volumetric")
        {
            // Each pair of corridor columns aims into its shared center aisle.
            for(int row=0;row<10;++row)for(int side=0;side<2;++side){
                SpotLight s;const float z=row*3.0f;
                s.position={side?3.5f:-3.5f,3.25f,z};
                s.direction={side?-3.5f:3.5f,-1.65f,1.15f};
                s.radius=11.5f;s.intensity=65000.0f;
                s.diffuseColor=side?dx::XMFLOAT4{0.15f,0.5f,1,1}:dx::XMFLOAT4{1,0.3f,0.08f,1};
                s.innerAngle=0.18f;s.outerAngle=0.46f;
                g.get_spot_light_manager().add_light(s);
            }
        }
        if (id_ == "ssgi" || id_ == "spot")
        {
            for (int i=0;i<2;++i) { SpotLight s;
                s.position={i?2.5f:-2.5f,3.2f,-2.0f};
                s.direction={0,-0.55f,1};
                s.radius=30.0f;s.intensity=14000.0f;
                s.diffuseColor=i?dx::XMFLOAT4{0.15f,0.5f,1,1}:dx::XMFLOAT4{1,0.3f,0.08f,1}; s.innerAngle=0.12f; s.outerAngle=0.45f;
                g.get_spot_light_manager().add_light(s); }
        }
        if (id_ == "area") { AreaLight a; a.position={0,4,-1}; a.direction={0,-1,0}; a.width=5; a.height=3; a.intensity=8000; g.get_area_light_manager().add_light(a); }
        const bool supports_debug_geometry=!exhibit_.value("pbr_asset",false)&&
            (id_=="point_shadow"||id_=="spot"||id_=="area"||id_=="ssgi"||id_=="volumetric"||id_=="frustum"||id_=="meshlets"||id_=="occlusion"||id_=="sun_shadow"||id_=="sky"||id_=="ddgi"||id_=="voxel");
        show_debug_geometry_=supports_debug_geometry&&id_!="volumetric"&&id_!="sky";
        if(supports_debug_geometry){debug_cube_=std::make_unique<Debug_Cube>(g.get_device());debug_sphere_=std::make_unique<Debug_Sphere>(g.get_device(),16,8);}
        if(id_=="frustum"||id_=="meshlets"||id_=="occlusion")initialize_culling_inset(g.get_device());
        if(id_=="occlusion"){
            D3D11_TEXTURE2D_DESC depth_desc{};depth_desc.Width=static_cast<UINT>(g.get_screen_width());depth_desc.Height=static_cast<UINT>(g.get_screen_height());
            depth_desc.MipLevels=depth_desc.ArraySize=1;depth_desc.Format=DXGI_FORMAT_D32_FLOAT;depth_desc.SampleDesc.Count=1;depth_desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            if(SUCCEEDED(g.get_device()->CreateTexture2D(&depth_desc,nullptr,texture.GetAddressOf())))
                g.get_device()->CreateDepthStencilView(texture.Get(),nullptr,observer_depth_.GetAddressOf());
        }
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
        Resource_Manager::instance().restore_camera_culling_frustum(culling_frozen_was_,culling_frustum_was_);
        debug_cube_.reset();debug_sphere_.reset();
        inset_srv_.Reset();inset_rtv_.Reset();inset_depth_.Reset();inset_texture_.Reset();observer_depth_.Reset();
        clear_lights();
        if(id_=="sky")g.post_procss.GetSky().set_look(sky_look_was_);
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
        if(capture_phase_>=0)time_=capture_phase_*dx::XM_2PI;
        else if (animate_) time_+=elapsed;
        if(!automated_)CameraManager::instance().update(elapsed,g.get_aspect_ratio(),g.get_screen_height());
        else {
            if(id_=="volumetric") {
                const auto p=exhibit_.at("camera").get<std::vector<float>>(),t=exhibit_.at("target").get<std::vector<float>>();
                const float x=p[0]+std::sin(time_*(capture_phase_>=0?1.0f:0.55f))*1.8f;
                const float pitch=dx::XMConvertToDegrees(std::atan2(t[1]-p[1],std::hypot(t[0]-x,t[2]-p[2])));
                const float yaw=dx::XMConvertToDegrees(std::atan2(t[2]-p[2],t[0]-x));
                camera_->set_rotation(yaw,pitch,dx::XMVectorSet(x,p[1],p[2],1),dx::XMVectorSet(t[0],t[1],t[2],1));
            }
            auto& lens=camera_->get_camera();lens.update_derived_parameters(g.get_aspect_ratio(),g.get_screen_height());lens.identity();
        }
        if(id_=="occlusion"&&(animate_||capture_phase_>=0)){
            const float phase=capture_phase_>=0?capture_phase_:std::fmod(time_/dx::XM_2PI,1.0f);
            const auto smooth=[](float t){t=(std::clamp)(t,0.0f,1.0f);return t*t*(3.0f-2.0f*t);};
            const float side=phase<0.30f?0.0f:phase<0.40f?smooth((phase-0.30f)/0.10f):phase<0.70f?1.0f:phase<0.80f?1.0f-smooth((phase-0.70f)/0.10f):0.0f;
            const float x=7.0f*side;
            if(std::abs(x-occlusion_last_camera_x_)>1e-4f){
                const auto eye=dx::XMVectorSet(x,2.35f,-7.0f,1),target=dx::XMVectorSet(0,1.5f,5,1);
                const float pitch=dx::XMConvertToDegrees(std::atan2(1.5f-2.35f,std::hypot(x,12.0f)));
                const float yaw=dx::XMConvertToDegrees(std::atan2(12.0f,-x));
                camera_->set_rotation(yaw,pitch,eye,target);
                occlusion_last_camera_x_=x;
                auto& lens=camera_->get_camera();lens.update_derived_parameters(g.get_aspect_ratio(),g.get_screen_height());lens.identity();
            }
        }
        if(id_=="frustum"||id_=="meshlets"){
            culling_eye_={std::sin(time_)*1.7f,2.6f,1.0f};
            const auto view=dx::XMMatrixLookAtLH(dx::XMLoadFloat3(&culling_eye_),dx::XMVectorSet(0,1.4f,9,1),dx::XMVectorSet(0,1,0,0));
            const auto projection=dx::XMMatrixPerspectiveFovLH(dx::XMConvertToRadians(42.0f),1.25f,0.3f,15.0f);
            dx::XMStoreFloat4x4(&culling_view_,view);dx::XMStoreFloat4x4(&culling_projection_,projection);
            dx::XMStoreFloat4x4(&culling_view_projection_,view*projection);
            Resource_Manager::instance().set_camera_culling_frustum(ExtractFrustum(view,projection));
        }
        if(id_=="sky"){
            const float hour=enabled_?(capture_phase_>=0||animate_?12.0f+4.5f*std::cos(time_):sky_hour_):14.0f;
            if(capture_phase_>=0||animate_)sky_hour_=hour;
            light_.set_time_of_day(hour,false);
            light_.direction.z=-2.2f;
            dx::XMStoreFloat4(&light_.direction,dx::XMVector3Normalize(dx::XMLoadFloat4(&light_.direction)));
            light_.update();
            configure_sky_look(hour);
        }
        if(id_=="adaptation")light_.illuminance_lux=capture_phase_>=0?80.0f*std::pow(25000.0f/80.0f,0.5f+0.5f*std::cos(time_)):(std::sin(time_*0.5f)>0?25000.0f:80.0f);
        if(id_=="ibl") light_.illuminance_lux=0;
        light_.update();
        if(auto* m=Resource_Manager::instance().get_model_instance("Showcase_Moving")) {
            dx::XMStoreFloat4x4(&m->world_transform,dx::XMMatrixTranslation(id_=="animation"?0:std::sin(time_)*2.5f,1.5f,0));
            if(id_=="animation"&&capture_phase_>=0){if(auto* actor=Resource_Manager::instance().get_model("Showcase_Actor"))if(!actor->animations.empty())m->animation_time=actor->animations[0].duration*capture_phase_;}
        }
        auto& points=g.get_point_light_manager().get_lights();
        if(id_=="point_shadow" && !points.empty()) { points[0].position.x=std::sin(time_*(capture_phase_>=0?1.0f:0.5f))*2; points[0].cast_shadows=enabled_; }
        g.get_point_light_manager().update(); g.get_point_light_manager().upload_to_gpu(g.get_device_context());
        g.get_spot_light_manager().update(); g.get_spot_light_manager().upload_to_gpu(g.get_device_context());
        g.get_area_light_manager().update(); g.get_area_light_manager().upload_to_gpu(g.get_device_context());
        Resource_Manager::instance().update_models(capture_phase_>=0?0:animate_?elapsed:0);
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
        r.begin_model_profile(c); r.render_models(pass_mode::deferred_geometry);
        if(id_=="occlusion")if(auto* instance=r.get_model_instance("Showcase_Main"))occlusion_frame_state_=instance->occlusion;
        r.issue_model_occlusion_queries(c); r.end_model_profile(c);
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
        if(id_=="occlusion")render_occlusion_observer(c);
        else if(show_debug_geometry_)render_debug_geometry(c);
        if(id_=="meshlets"&&enabled_)render_culled_debug_boxes(c,false);
        if(inset_rtv_)render_culling_inset(c);
        if((id_=="meshlets"||id_=="occlusion")&&enabled_&&Text::text){
            auto font=Text::text_data;font.fontSize=24;font.font=Text::text->get_font_name(Font_Name::Font_rounded_x_mgenplus_1c_regular);
            font.Color=D2D1::ColorF(1,0.05f,0.65f,1);font.shadowColor=D2D1::ColorF(0,0,0,1);Text::text->set_font(font);
            Text::draw(L"MAGENTA WIRE = CULLED GEOMETRY",dx::XMFLOAT2{28,26},D2D1_DRAW_TEXT_OPTIONS_NONE,true);
        }
    }
    void draw_gui() override
    {
        ImGui::SeparatorText(exhibit_.at("title").get<std::string>().c_str());
        ImGui::TextWrapped("%s",exhibit_.at("description").get<std::string>().c_str());
        if(!error_.empty())ImGui::TextWrapped("%s",error_.c_str());
        const char* comparison=id_=="sun_shadow"?"PCSS (off = PCF reference)":id_=="sky"?"Animate sun / sky":"Technique enabled";
        if(ImGui::Checkbox(comparison,&enabled_))apply_comparison();
        ImGui::SameLine(); if(ImGui::Button("Reset camera"))reset_camera();
        ImGui::Checkbox("Animate demonstration",&animate_);
        if(id_=="sky"){
            ImGui::Combo("Sky mood",&sky_preset_,"Gilded dusk\0Storm blue\0Violet twilight\0");
            if(ImGui::SliderFloat("Sky time (hours)",&sky_hour_,0,24,"%.1f h"))animate_=false;
            ImGui::SliderFloat("Painted color",&sky_art_strength_,0,1);
            ImGui::SliderFloat("Cloud cover",&sky_cloud_coverage_,0.25f,0.8f);
            ImGui::SliderFloat("Horizon fog blend",&sky_fog_blend_,0,0.8f);
            ImGui::SliderFloat("Sky vignette",&sky_vignette_,0,0.6f);
        }
        if(debug_cube_)ImGui::Checkbox("Show technique geometry",&show_debug_geometry_);
        if(id_=="frustum"||id_=="meshlets")ImGui::TextDisabled("Cyan: separate culling camera and its actual frustum. View camera is independent.");
        if(id_=="occlusion")ImGui::TextDisabled("Cyan: moving GPU query camera and frustum. Main view: fixed side observer. Magenta wire: GPU-culled groups.");
        if(id_=="point_shadow")for(const auto& light:Graphics_Core::instance().get_point_light_manager().get_lights())
            ImGui::Text("Point light: (%.1f, %.1f, %.1f), radius %.1f",light.position.x,light.position.y,light.position.z,light.radius);
        if(id_=="spot"||id_=="ssgi"||id_=="volumetric")for(const auto& light:Graphics_Core::instance().get_spot_light_manager().get_lights())
            ImGui::Text("Spot: (%.1f, %.1f, %.1f), range %.1f, outer angle %.2f rad",light.position.x,light.position.y,light.position.z,light.radius,light.outerAngle);
        if(id_=="spot"||id_=="ssgi"||id_=="volumetric")ImGui::TextDisabled("Cone cross-section is drawn at up to 6.5 m; the range is listed above.");
        if(id_=="area")for(const auto& light:Graphics_Core::instance().get_area_light_manager().get_lights())
            ImGui::Text("Area: (%.1f, %.1f, %.1f), %.1f x %.1f, range %.1f",light.position.x,light.position.y,light.position.z,light.width,light.height,light.range);
        ImGui::TextDisabled("Move: W/A/S/D, Q/E. Look: right mouse. Shift: faster.");
        ImGui::Text("%.1f FPS / %.2f ms",ImGui::GetIO().Framerate,1000.0f/(std::max)(ImGui::GetIO().Framerate,1.0f));
        if(id_=="dlss") { auto& d=Graphics_Core::instance().post_procss.GetDLSS(); if(!d.IsAvailable())ImGui::TextWrapped("DLSS unavailable: %s. Native TAA remains active.",d.GetUnavailableReason().c_str()); }
        if(id_=="frustum"||id_=="meshlets"||id_=="occlusion")Resource_Manager::instance().draw_model_debug();
        if(id_=="sun_shadow"||id_=="point_shadow")Graphics_Core::instance().post_procss.GetShadow().shadow_gui();
        if(voxel_)voxel_->draw_gui(); if(ddgi_)ddgi_->draw_gui();
        if(id_!="sky")light_.draw_imgui("Exhibit sun");
    }
private:
    void configure_sky_look(float hour)
    {
        auto look=sky_look_was_;
        look.art_strength=enabled_?sky_art_strength_:0.0f;
        const float dusk=(std::clamp)(std::exp(-std::pow((hour-18.0f)/3.4f,2.0f))+
            std::exp(-std::pow((hour-6.0f)/3.4f,2.0f)),0.0f,1.0f);
        const auto mix=[dusk](dx::XMFLOAT4 day,dx::XMFLOAT4 evening){return dx::XMFLOAT4{
            day.x+(evening.x-day.x)*dusk,day.y+(evening.y-day.y)*dusk,day.z+(evening.z-day.z)*dusk,1};};
        if(sky_preset_==1){
            look.zenith_tint=mix({0.25f,0.37f,0.52f,1},{0.13f,0.18f,0.30f,1});
            look.horizon_tint=mix({0.38f,0.45f,0.54f,1},{0.28f,0.30f,0.42f,1});
            look.directional_tint={0.08f,0.15f,0.26f,1};look.fog_tint={0.38f,0.47f,0.58f,1};
            look.cloud_tint={0.36f,0.44f,0.60f,1};look.cloud_controls={0.72f,1.45f,0,0};
        }else if(sky_preset_==2){
            look.zenith_tint=mix({0.31f,0.24f,0.60f,1},{0.20f,0.11f,0.43f,1});
            look.horizon_tint=mix({0.75f,0.48f,0.73f,1},{1.15f,0.32f,0.46f,1});
            look.directional_tint={0.40f,0.05f,0.18f,1};look.fog_tint={0.58f,0.29f,0.49f,1};
            look.cloud_tint={0.94f,0.58f,0.88f,1};look.cloud_controls={sky_cloud_coverage_,0.85f,0,0};
        }else{
            look.zenith_tint=mix({0.30f,0.50f,0.85f,1},{0.12f,0.15f,0.50f,1});
            look.horizon_tint=mix({0.72f,0.72f,0.83f,1},{1.35f,0.36f,0.13f,1});
            look.directional_tint={0.52f,0.15f,0.045f,1};look.fog_tint={0.82f,0.53f,0.39f,1};
            look.cloud_tint={1.12f,0.84f,0.64f,1};look.cloud_controls={sky_cloud_coverage_,1.0f,0,0};
        }
        look.directional_strength=0.38f+0.55f*dusk;
        look.fog_blend=sky_fog_blend_;
        look.paint_direction_vignette={-1,0,0.25f,sky_vignette_};
        Graphics_Core::instance().post_procss.GetSky().set_look(look);
        auto& distance_fog=Graphics_Core::instance().post_procss.GetDistanceFog().config;
        distance_fog.fog_color[0]=look.fog_tint.x*0.58f;
        distance_fog.fog_color[1]=look.fog_tint.y*0.58f;
        distance_fog.fog_color[2]=look.fog_tint.z*0.58f;
    }
    void render_culled_debug_boxes(ID3D11DeviceContext* context,bool occlusion)
    {
        auto& resources=Resource_Manager::instance();
        auto* model=resources.get_model(model_key_);auto* instance=resources.get_model_instance("Showcase_Main");
        if(!model||!instance||!debug_cube_)return;
        const Frustum* frustum=occlusion?nullptr:&resources.frozen_camera_frustum();
        const auto* state=occlusion?&occlusion_frame_state_:&instance->occlusion;
        const auto boxes=model->collect_culled_debug_boxes(instance->world_transform,frustum,state,!occlusion,occlusion&&enabled_);
        Render_State::instance().set_render_states(context,Depth_State::Test_Disable_Write_Disable,Blend_State::Alpha,Rasterizer_State::Wireframe_CCW);
        for(const auto& box:boxes)draw_box(context,box.minimum,box.maximum,{1.0f,0.02f,0.65f,0.9f});
    }
    void render_occlusion_observer(ID3D11DeviceContext* context)
    {
        if(!observer_depth_)return;
        auto& graphics=Graphics_Core::instance();auto& resources=Resource_Manager::instance();
        auto* model=resources.get_model(model_key_);auto* instance=resources.get_model_instance("Showcase_Main");
        if(!model||!instance)return;
        observer_eye_={8.5f,5.2f,-5.0f};
        const auto eye=dx::XMLoadFloat3(&observer_eye_);
        const auto view=dx::XMMatrixLookAtLH(eye,dx::XMVectorSet(0,1.5f,5,1),dx::XMVectorSet(0,1,0,0));
        const auto projection=dx::XMMatrixPerspectiveFovLH(dx::XMConvertToRadians(56.0f),graphics.get_aspect_ratio(),0.1f,80.0f);
        Camera_Constants constants{};dx::XMStoreFloat4(&constants.camera_position,eye);
        dx::XMStoreFloat4x4(&constants.view,view);dx::XMStoreFloat4x4(&constants.projection,projection);
        dx::XMStoreFloat4x4(&constants.view_projection,view*projection);constants.previous_view_projection=constants.view_projection;
        dx::XMStoreFloat4x4(&constants.inv_view,dx::XMMatrixInverse(nullptr,view));
        dx::XMStoreFloat4x4(&constants.inv_projection,dx::XMMatrixInverse(nullptr,projection));
        dx::XMStoreFloat4x4(&constants.inv_view_projection,dx::XMMatrixInverse(nullptr,view*projection));
        constants.light_view_projection=graphics.post_procss.GetShadow().light_view_projection;constants.near_z=0.1f;constants.far_z=80;
        auto* camera_buffer=graphics.get_constant_buffer(Graphics_Core::Conastant_Buffer_Type::Camera);
        context->UpdateSubresource(camera_buffer,0,nullptr,&constants,0,0);
        context->VSSetConstantBuffers(1,1,&camera_buffer);context->PSSetConstantBuffers(1,1,&camera_buffer);
        auto* target=graphics.get_render_target_view().Get();context->OMSetRenderTargets(1,&target,observer_depth_.Get());
        const float background[]{0.04f,0.055f,0.075f,1};context->ClearRenderTargetView(target,background);
        context->ClearDepthStencilView(observer_depth_.Get(),D3D11_CLEAR_DEPTH,1,0);
        Render_State::instance().set_render_states(context,Depth_State::Test_Enable_Write_Enable,Blend_State::Opaque,Rasterizer_State::Cull_None_CW);
        const auto& query_camera=CameraManager::instance().get_active_camera()->get_camera();
        const auto query_frustum=ExtractFrustum(query_camera.view,query_camera.projection);
        model->render(context,instance->world_transform,pass_mode::forward_opaque,false,0,Gltf_Model::animation_mode::single,0,&query_frustum,nullptr,&occlusion_frame_state_,enabled_&&occlusion_frame_state_.valid);
        context->OMSetRenderTargets(1,&target,nullptr);
        render_culled_debug_boxes(context,true);
        render_occlusion_camera_debug(context,query_camera);
        if(show_debug_geometry_)draw_box(context,{-2.8f,0,-0.35f},{2.8f,3.4f,0.35f},{1,0.7f,0.1f,0.5f});
        graphics.restore_camera_constants();
    }
    void render_occlusion_camera_debug(ID3D11DeviceContext* context,const Camera& camera)
    {
        if(!debug_cube_||!debug_sphere_)return;
        Render_State::instance().set_render_states(context,Depth_State::Test_Disable_Write_Disable,Blend_State::Alpha,Rasterizer_State::Wireframe_CCW);
        const dx::XMFLOAT4 cyan{0.02f,0.9f,1,0.55f},marker{0.02f,0.9f,1,1};
        const auto eye=camera.position;
        const auto inverse=dx::XMMatrixInverse(nullptr,camera.view*camera.projection);
        dx::XMVECTOR corners[8];
        for(int i=0;i<4;++i){
            const auto sample=dx::XMVector3TransformCoord(dx::XMVectorSet(i&1?1.0f:-1.0f,i&2?1.0f:-1.0f,camera.isReversed_Z?0.0f:1.0f,1),inverse);
            const auto ray=dx::XMVector3Normalize(dx::XMVectorSubtract(sample,eye));
            corners[i]=dx::XMVectorAdd(eye,dx::XMVectorScale(ray,0.6f));
            corners[i+4]=dx::XMVectorAdd(eye,dx::XMVectorScale(ray,11.5f));
        }
        for(int i=0;i<4;++i){draw_segment(context,corners[i],corners[i^1],cyan);draw_segment(context,corners[i],corners[i^2],cyan);draw_segment(context,corners[i+4],corners[(i^1)+4],cyan);draw_segment(context,corners[i+4],corners[(i^2)+4],cyan);draw_segment(context,corners[i],corners[i+4],cyan);}
        draw_sphere(context,eye,0.23f,marker);
    }
    void initialize_culling_inset(ID3D11Device* device)
    {
        D3D11_TEXTURE2D_DESC d{};d.Width=512;d.Height=288;d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.SampleDesc.Count=1;
        d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(device->CreateTexture2D(&d,nullptr,inset_texture_.GetAddressOf()))||
           FAILED(device->CreateRenderTargetView(inset_texture_.Get(),nullptr,inset_rtv_.GetAddressOf()))||
           FAILED(device->CreateShaderResourceView(inset_texture_.Get(),nullptr,inset_srv_.GetAddressOf()))){error_="Culling camera inset color target creation failed.";return;}
        d.Format=DXGI_FORMAT_D32_FLOAT;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depth;
        if(FAILED(device->CreateTexture2D(&d,nullptr,depth.GetAddressOf()))||
           FAILED(device->CreateDepthStencilView(depth.Get(),nullptr,inset_depth_.GetAddressOf()))){error_="Culling camera inset depth target creation failed.";inset_rtv_.Reset();}
    }
    void render_culling_inset(ID3D11DeviceContext* context)
    {
        if(!inset_rtv_||!inset_depth_)return;
        auto& graphics=Graphics_Core::instance();auto& resources=Resource_Manager::instance();
        auto* model=resources.get_model(model_key_);auto* instance=resources.get_model_instance("Showcase_Main");
        if(!model||!instance)return;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> old_target;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> old_depth;
        context->OMGetRenderTargets(1,old_target.GetAddressOf(),old_depth.GetAddressOf());
        UINT viewport_count=1;D3D11_VIEWPORT old_viewport{};context->RSGetViewports(&viewport_count,&old_viewport);
        const auto& camera=CameraManager::instance().get_active_camera()->get_camera();
        const bool view_camera=id_=="occlusion";
        const auto view=view_camera?camera.view:dx::XMLoadFloat4x4(&culling_view_);
        const auto projection=view_camera?camera.projection:dx::XMLoadFloat4x4(&culling_projection_);
        const auto eye=view_camera?camera.position:dx::XMLoadFloat3(&culling_eye_);
        const auto frustum=ExtractFrustum(view,projection);
        Camera_Constants constants{};
        dx::XMStoreFloat4(&constants.camera_position,eye);
        dx::XMStoreFloat4x4(&constants.view,view);dx::XMStoreFloat4x4(&constants.projection,projection);
        dx::XMStoreFloat4x4(&constants.view_projection,view*projection);
        constants.previous_view_projection=constants.view_projection;
        dx::XMStoreFloat4x4(&constants.inv_view,dx::XMMatrixInverse(nullptr,view));
        dx::XMStoreFloat4x4(&constants.inv_projection,dx::XMMatrixInverse(nullptr,projection));
        dx::XMStoreFloat4x4(&constants.inv_view_projection,dx::XMMatrixInverse(nullptr,view*projection));
        constants.light_view_projection=graphics.post_procss.GetShadow().light_view_projection;
        constants.near_z=view_camera?camera.near_z:0.3f;constants.far_z=view_camera?camera.far_z:15.0f;
        constants.isReserved=view_camera&&camera.isReversed_Z?1:0;
        auto* camera_buffer=graphics.get_constant_buffer(Graphics_Core::Conastant_Buffer_Type::Camera);
        context->UpdateSubresource(camera_buffer,0,nullptr,&constants,0,0);
        context->VSSetConstantBuffers(1,1,&camera_buffer);context->PSSetConstantBuffers(1,1,&camera_buffer);
        auto* target=inset_rtv_.Get();context->OMSetRenderTargets(1,&target,inset_depth_.Get());
        const float background[]{0.035f,0.05f,0.08f,1};context->ClearRenderTargetView(target,background);
        const bool reversed=view_camera&&camera.isReversed_Z;
        context->ClearDepthStencilView(inset_depth_.Get(),D3D11_CLEAR_DEPTH,reversed?0.0f:1.0f,0);
        D3D11_VIEWPORT inset_viewport{0,0,512,288,0,1};context->RSSetViewports(1,&inset_viewport);
        Render_State::instance().set_render_states(context,reversed?Depth_State::Reversed_Z_Test_Enable_Write_Enable:Depth_State::Test_Enable_Write_Enable,Blend_State::Opaque,Rasterizer_State::Cull_None_CW);
        const Frustum* meshlet_frustum=resources.meshlet_culling_enabled()?&frustum:nullptr;
        const bool use_occlusion=id_=="occlusion"&&enabled_&&instance->occlusion.valid;
        if(id_=="frustum")resources.render_models(pass_mode::forward_opaque);
        else model->render(context,instance->world_transform,pass_mode::forward_opaque,false,0,Gltf_Model::animation_mode::single,0,meshlet_frustum,nullptr,id_=="occlusion"?&occlusion_frame_state_:&instance->occlusion,use_occlusion);
        context->OMSetRenderTargets(1,old_target.GetAddressOf(),old_depth.Get());
        if(viewport_count)context->RSSetViewports(1,&old_viewport);
        graphics.restore_camera_constants();
        const float width=graphics.get_screen_width(),height=graphics.get_screen_height();
        const float display_width=(std::min)(width*0.30f,512.0f),display_height=display_width*288.0f/512.0f;
        D3D11_VIEWPORT display{width-display_width-24.0f,24.0f,display_width,display_height,0,1};
        context->RSSetViewports(1,&display);
        Render_State::instance().set_render_states(context,Depth_State::Test_Disable_Write_Disable,Blend_State::Opaque,Rasterizer_State::Cull_None_CW);
        auto* image=inset_srv_.Get();graphics.get_fullscreen_quad()->Blit(context,&image,0,1);
        ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);
        if(viewport_count)context->RSSetViewports(1,&old_viewport);
        if(Text::text){
            auto font=Text::text_data;font.fontSize=(std::max)(16.0f,height*0.019f);
            font.font=Text::text->get_font_name(Font_Name::Font_rounded_x_mgenplus_1c_regular);
            font.Color=D2D1::ColorF(1,1,1,1);font.shadowColor=D2D1::ColorF(0,0,0,1);
            Text::text->set_font(font);
            const auto& stats=resources.camera_meshlet_stats();
            const std::wstring title=(id_=="occlusion"?L"OCCLUSION CAMERA":id_=="meshlets"?L"MESHLET CULL CAMERA":L"FRUSTUM CULL CAMERA")+std::wstring(enabled_?L"  ON":L"  OFF");
            const std::wstring detail=id_=="occlusion"?
                L"Groups culled: "+std::to_wstring(stats.occlusion_culled)+L" / "+std::to_wstring(stats.occlusion_tested):
                id_=="frustum"?L"Instances culled: "+std::to_wstring(resources.camera_instances_culled())+L" / "+std::to_wstring(resources.camera_instances_total()):
                L"Meshlets culled: "+std::to_wstring(stats.culled)+L" / "+std::to_wstring(stats.tested);
            const float x=display.TopLeftX+10.0f,y=display.TopLeftY+8.0f;
            Text::draw(title,dx::XMFLOAT2{x,y},D2D1_DRAW_TEXT_OPTIONS_NONE,true);
            Text::draw(detail,dx::XMFLOAT2{x,display.TopLeftY+display.Height-32.0f},D2D1_DRAW_TEXT_OPTIONS_NONE,true);
        }
    }
    void draw_segment(ID3D11DeviceContext* context,dx::XMVECTOR a,dx::XMVECTOR b,const dx::XMFLOAT4& color)
    {
        const auto delta=dx::XMVectorSubtract(b,a);
        const float length=dx::XMVectorGetX(dx::XMVector3Length(delta));
        if(length<0.0001f)return;
        const auto direction=dx::XMVectorScale(delta,1.0f/length);
        const auto up=dx::XMVectorSet(0,1,0,0);
        const float dot=dx::XMVectorGetX(dx::XMVector3Dot(up,direction));
        const auto rotation=dot>0.999f?dx::XMMatrixIdentity():dot< -0.999f?
            dx::XMMatrixRotationX(dx::XM_PI):
            dx::XMMatrixRotationAxis(dx::XMVector3Normalize(dx::XMVector3Cross(up,direction)),std::acos((std::clamp)(dot,-1.0f,1.0f)));
        dx::XMFLOAT4X4 world;
        dx::XMStoreFloat4x4(&world,dx::XMMatrixScaling(0.025f,length,0.025f)*rotation*dx::XMMatrixTranslationFromVector(dx::XMVectorScale(dx::XMVectorAdd(a,b),0.5f)));
        debug_cube_->Render(context,world,color);
    }
    void draw_sphere(ID3D11DeviceContext* context,dx::XMVECTOR position,float radius,const dx::XMFLOAT4& color)
    {
        dx::XMFLOAT4X4 world;
        dx::XMStoreFloat4x4(&world,dx::XMMatrixScaling(radius,radius,radius)*dx::XMMatrixTranslationFromVector(position));
        debug_sphere_->Render(context,world,color);
    }
    void draw_box(ID3D11DeviceContext* context,const dx::XMFLOAT3& minimum,const dx::XMFLOAT3& maximum,const dx::XMFLOAT4& color)
    {
        dx::XMVECTOR corners[8];
        for(int i=0;i<8;++i)corners[i]=dx::XMVectorSet(i&1?maximum.x:minimum.x,i&2?maximum.y:minimum.y,i&4?maximum.z:minimum.z,1);
        for(int i=0;i<8;++i)for(int bit:{1,2,4})if(!(i&bit))draw_segment(context,corners[i],corners[i|bit],color);
    }
    void render_debug_geometry(ID3D11DeviceContext* context)
    {
        if(!debug_cube_||!debug_sphere_)return;
        Render_State::instance().set_render_states(context,Depth_State::Test_Disable_Write_Disable,Blend_State::Alpha,Rasterizer_State::Wireframe_CCW);
        const dx::XMFLOAT4 amber{1.0f,0.75f,0.12f,0.8f},cyan{0.1f,0.95f,1.0f,0.85f},magenta{1.0f,0.25f,0.8f,0.8f};
        auto& graphics=Graphics_Core::instance();
        if(id_=="point_shadow")for(const auto& light:graphics.get_point_light_manager().get_lights()){
            auto center=dx::XMLoadFloat3(&light.position);
            draw_sphere(context,center,0.18f,amber);
            draw_segment(context,dx::XMVectorAdd(center,dx::XMVectorSet(-0.45f,0,0,0)),dx::XMVectorAdd(center,dx::XMVectorSet(0.45f,0,0,0)),amber);
            draw_segment(context,dx::XMVectorAdd(center,dx::XMVectorSet(0,-0.45f,0,0)),dx::XMVectorAdd(center,dx::XMVectorSet(0,0.45f,0,0)),amber);
            for(const auto direction:{dx::XMVectorSet(1,0,0,0),dx::XMVectorSet(0,0,1,0)})
                draw_segment(context,center,dx::XMVectorAdd(center,dx::XMVectorScale(direction,light.radius)),{0.75f,0.45f,0.08f,0.25f});
        }
        if(id_=="spot"||id_=="ssgi"||id_=="volumetric")for(const auto& light:graphics.get_spot_light_manager().get_lights()){
            const auto origin=dx::XMLoadFloat3(&light.position);
            const auto direction=dx::XMVector3Normalize(dx::XMLoadFloat3(&light.direction));
            const auto reference=std::abs(dx::XMVectorGetY(direction))>0.95f?dx::XMVectorSet(1,0,0,0):dx::XMVectorSet(0,1,0,0);
            const auto right=dx::XMVector3Normalize(dx::XMVector3Cross(reference,direction));
            const auto up=dx::XMVector3Cross(direction,right);
            const float distance=(std::min)(light.radius,6.5f);
            const auto center=dx::XMVectorAdd(origin,dx::XMVectorScale(direction,distance));
            draw_sphere(context,origin,0.14f,amber);
            for(int ring=0;ring<2;++ring){
                const float radius=distance*std::tan(ring?light.outerAngle:light.innerAngle);
                for(int i=0;i<12;++i){
                    const float a=dx::XM_2PI*i/12.0f,b=dx::XM_2PI*(i+1)/12.0f;
                    const auto point=[&](float angle){return dx::XMVectorAdd(center,dx::XMVectorAdd(dx::XMVectorScale(right,std::cos(angle)*radius),dx::XMVectorScale(up,std::sin(angle)*radius)));};
                    draw_segment(context,point(a),point(b),ring?amber:cyan);
                    if(ring&&i%3==0)draw_segment(context,origin,point(a),amber);
                }
            }
        }
        if(id_=="area")for(const auto& light:graphics.get_area_light_manager().get_lights()){
            const auto origin=dx::XMLoadFloat3(&light.position),right=dx::XMVector3Normalize(dx::XMLoadFloat3(&light.right));
            const auto up=dx::XMVector3Normalize(dx::XMVector3Cross(dx::XMLoadFloat3(&light.direction),right));
            dx::XMVECTOR corners[4];
            for(int i=0;i<4;++i)corners[i]=dx::XMVectorAdd(origin,dx::XMVectorAdd(dx::XMVectorScale(right,(i&1?1.0f:-1.0f)*light.width*0.5f),dx::XMVectorScale(up,(i&2?1.0f:-1.0f)*light.height*0.5f)));
            for(int i=0;i<4;++i){draw_segment(context,corners[i],corners[i^1],amber);draw_segment(context,corners[i],corners[i^2],amber);}
            draw_sphere(context,origin,0.14f,amber);
            draw_segment(context,origin,dx::XMVectorAdd(origin,dx::XMVectorScale(dx::XMLoadFloat3(&light.direction),2.0f)),cyan);
        }
        if(id_=="sun_shadow"||id_=="sky"){
            const auto origin=dx::XMVectorSet(0,3,2,1);
            draw_sphere(context,origin,0.2f,amber);
            draw_segment(context,origin,dx::XMVectorAdd(origin,dx::XMVectorScale(dx::XMLoadFloat4(&light_.direction),3)),amber);
        }
        if(id_=="frustum"||id_=="meshlets"){
            const auto inverse=dx::XMMatrixInverse(nullptr,dx::XMLoadFloat4x4(&culling_view_projection_));
            dx::XMVECTOR corners[8];
            for(int i=0;i<8;++i)corners[i]=dx::XMVector3TransformCoord(dx::XMVectorSet(i&1?1.0f:-1.0f,i&2?1.0f:-1.0f,i&4?1.0f:0.0f,1),inverse);
            for(int i=0;i<8;++i)for(int bit:{1,2,4})if(!(i&bit))draw_segment(context,corners[i],corners[i|bit],cyan);
            draw_sphere(context,dx::XMLoadFloat3(&culling_eye_),0.2f,magenta);
        }
        if(id_=="ddgi")draw_box(context,{-3.2f,0,-3},{3.2f,4,5},cyan);
        if(id_=="occlusion")draw_box(context,{-2.8f,0,-0.35f},{2.8f,3.4f,0.35f},amber);
    }
    void clear_lights() { auto& g=Graphics_Core::instance();g.get_point_light_manager().clear_light();g.get_spot_light_manager().clear_light();g.get_area_light_manager().clear_light(); }
    void reset_camera()
    {
        auto& g=Graphics_Core::instance(); camera_=std::make_shared<SpectatorCamera>(g.get_window_handle());
        const auto p=exhibit_.at("camera").get<std::vector<float>>(), t=exhibit_.at("target").get<std::vector<float>>();
        const float pitch=dx::XMConvertToDegrees(std::atan2(t[1]-p[1],std::hypot(t[0]-p[0],t[2]-p[2])));
        const float yaw=dx::XMConvertToDegrees(std::atan2(t[2]-p[2],t[0]-p[0]));
        camera_->set_rotation(yaw,pitch,dx::XMVectorSet(p[0],p[1],p[2],1),dx::XMVectorSet(t[0],t[1],t[2],1));
        auto& lens=camera_->get_camera(); lens.FocalLength=28; lens.SensorSize=36; lens.FNumber=id_=="dof"?1.4f:8; lens.FocusDist=exhibit_.value("focus",9.0f); lens.ISO=400; lens.shutterDenominator=60; lens.EV_Compensation=0; lens.autoSettingsEnabled=false; lens.near_z=0.08f; lens.far_z=120;
        lens.near_z=exhibit_.value("near",0.08f);lens.far_z=exhibit_.value("far",120.0f);camera_->set_move_speed(exhibit_.value("move_speed",10.0f));
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
        if(exhibit_.value("pbr_asset",false))if(auto* model=resources.get_model(model_key_))model->set_pbr_feature_override(enabled_?0:exhibit_.value("pbr_feature",0));
        if(id_=="pbr")if(auto* model=resources.get_model(model_key_)){model->set_full_pbr_enabled(true);model->set_pbr_feature_override(enabled_?0:18);}
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
    float capture_phase_=-1;
    int point_was_=1; float shadow_distance_was_=60,time_=0;
    DLSSManager::Mode dlss_mode_was_=DLSSManager::Mode::Quality;
    bool fog_enabled_was_[4]{true,true,true,true};
    bool meshlets_was_=true,occlusion_was_=true;
    std::vector<std::string> extra_keys_;
    std::unique_ptr<ShowcaseReflectionProbe> probe_;
    bool automated_=false;
    float sun_radius_was_=0.025f;
    std::unique_ptr<Debug_Cube> debug_cube_;
    std::unique_ptr<Debug_Sphere> debug_sphere_;
    bool show_debug_geometry_=false,culling_frozen_was_=false;
    Frustum culling_frustum_was_{};
    dx::XMFLOAT3 culling_eye_{};
    dx::XMFLOAT4X4 culling_view_projection_{};
    dx::XMFLOAT4X4 culling_view_{},culling_projection_{};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> inset_texture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> inset_rtv_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> inset_srv_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> inset_depth_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> observer_depth_;
    dx::XMFLOAT3 observer_eye_{};
    Gltf_Model::occlusion_state occlusion_frame_state_{};
    float occlusion_last_camera_x_=1e9f;
    Sky::Constants sky_look_was_{};
    int sky_preset_=0;
    float sky_art_strength_=1.0f,sky_cloud_coverage_=0.72f,sky_fog_blend_=0.22f,sky_vignette_=0.24f,sky_hour_=17.5f;
    uint64_t occlusion_query_baseline_=0;
};
