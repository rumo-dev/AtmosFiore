#pragma once
#include "Engine/System/graphics_core.h"
#include "Engine/Graphics/Model/gltf_model.h"

// The same three-probe shader layout used by Indoor, with one generated room.
class ShowcaseReflectionProbe
{
    struct Constants { DirectX::XMFLOAT4 positions[3]{},minima[3]{},maxima[3]{},settings{}; };
public:
    bool initialize(ID3D11Device* device)
    {
        D3D11_TEXTURE2D_DESC d{};d.Width=d.Height=128;d.MipLevels=0;d.ArraySize=6;d.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;d.MiscFlags=D3D11_RESOURCE_MISC_TEXTURECUBE|D3D11_RESOURCE_MISC_GENERATE_MIPS;
        if(FAILED(device->CreateTexture2D(&d,nullptr,texture_.GetAddressOf())))return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=d.Format;sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURECUBE;sd.TextureCube.MipLevels=UINT(-1);
        if(FAILED(device->CreateShaderResourceView(texture_.Get(),&sd,srv_.GetAddressOf())))return false;
        for(UINT i=0;i<6;++i){D3D11_RENDER_TARGET_VIEW_DESC rd{};rd.Format=d.Format;rd.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2DARRAY;rd.Texture2DArray.ArraySize=1;rd.Texture2DArray.FirstArraySlice=i;if(FAILED(device->CreateRenderTargetView(texture_.Get(),&rd,faces_[i].GetAddressOf())))return false;}
        d.MipLevels=d.ArraySize=1;d.Format=DXGI_FORMAT_D32_FLOAT;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;d.MiscFlags=0;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depth;
        if(FAILED(device->CreateTexture2D(&d,nullptr,depth.GetAddressOf()))||FAILED(device->CreateDepthStencilView(depth.Get(),nullptr,depth_.GetAddressOf())))return false;
        D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(Constants);cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        return SUCCEEDED(device->CreateBuffer(&cb,nullptr,cb_.GetAddressOf()));
    }
    void bind(ID3D11DeviceContext* c,bool enabled)
    {
        Constants data{};data.positions[0]={0,2,-1.5f,1};data.minima[0]={-7,0,-7,0};data.maxima[0]={7,5,9,0};data.settings={enabled&&ready_?1.0f:0.0f,1.0f,1.0f,0};
        c->UpdateSubresource(cb_.Get(),0,nullptr,&data,0,0);auto* cb=cb_.Get();c->PSSetConstantBuffers(10,1,&cb);
        ID3D11ShaderResourceView* views[3]={enabled&&ready_?srv_.Get():nullptr,nullptr,nullptr};c->PSSetShaderResources(36,3,views);
    }
    void capture(ID3D11DeviceContext* c,Gltf_Model* model,const DirectX::XMFLOAT4X4& world)
    {
        if(ready_||!model)return;
        bind(c,false);
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> oldRT;Microsoft::WRL::ComPtr<ID3D11DepthStencilView> oldDepth;
        c->OMGetRenderTargets(1,oldRT.GetAddressOf(),oldDepth.GetAddressOf());UINT count=1;D3D11_VIEWPORT oldVP{};c->RSGetViewports(&count,&oldVP);
        D3D11_VIEWPORT vp{0,0,128,128,0,1};c->RSSetViewports(1,&vp);
        using namespace DirectX;
        const XMVECTOR directions[]={XMVectorSet(1,0,0,0),XMVectorSet(-1,0,0,0),XMVectorSet(0,1,0,0),XMVectorSet(0,-1,0,0),XMVectorSet(0,0,1,0),XMVectorSet(0,0,-1,0)};
        const XMVECTOR ups[]={XMVectorSet(0,1,0,0),XMVectorSet(0,1,0,0),XMVectorSet(0,0,-1,0),XMVectorSet(0,0,1,0),XMVectorSet(0,1,0,0),XMVectorSet(0,1,0,0)};
        auto& g=Graphics_Core::instance();auto* cb=g.get_constant_buffer(Graphics_Core::Conastant_Buffer_Type::Camera);
        const auto eye=XMVectorSet(0,2,-1.5f,1);
        const auto projection=XMMatrixPerspectiveFovLH(XM_PIDIV2,1,0.05f,80);
        Camera_Constants camera{};camera.camera_position={0,2,-1.5f,1};
        const auto view=XMMatrixLookAtLH(eye,eye+directions[next_face_],ups[next_face_]);
        XMStoreFloat4x4(&camera.view,view);XMStoreFloat4x4(&camera.projection,projection);XMStoreFloat4x4(&camera.view_projection,view*projection);camera.previous_view_projection=camera.view_projection;
        XMStoreFloat4x4(&camera.inv_view,XMMatrixInverse(nullptr,view));XMStoreFloat4x4(&camera.inv_projection,XMMatrixInverse(nullptr,projection));XMStoreFloat4x4(&camera.inv_view_projection,XMMatrixInverse(nullptr,view*projection));camera.light_view_projection=g.post_procss.GetShadow().light_view_projection;camera.near_z=0.05f;camera.far_z=80;
        c->UpdateSubresource(cb,0,nullptr,&camera,0,0);c->VSSetConstantBuffers(1,1,&cb);c->PSSetConstantBuffers(1,1,&cb);
        auto* face=faces_[next_face_].Get();c->OMSetRenderTargets(1,&face,depth_.Get());const float clear[]={0.02f,0.025f,0.04f,1};c->ClearRenderTargetView(face,clear);c->ClearDepthStencilView(depth_.Get(),D3D11_CLEAR_DEPTH,1,0);
        Render_State::instance().set_render_states(c,Depth_State::Test_Enable_Write_Enable,Blend_State::Opaque,Rasterizer_State::Cull_None_CW);
        model->render(c,world,pass_mode::forward_opaque,false,0,Gltf_Model::animation_mode::single,0);
        c->OMSetRenderTargets(0,nullptr,nullptr);
        if(++next_face_==6){c->GenerateMips(srv_.Get());ready_=true;}
        c->OMSetRenderTargets(1,oldRT.GetAddressOf(),oldDepth.Get());if(count)c->RSSetViewports(1,&oldVP);g.restore_camera_constants();
    }
private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> faces_[6];
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> cb_;
    UINT next_face_=0;bool ready_=false;
};
