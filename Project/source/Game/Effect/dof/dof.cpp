#include "dof.h"
#include "Engine/System/Manager/resource_manager.h"

dof::dof(ID3D11Device* device, uint32_t width, uint32_t height) {
	bit_block_transfer = std::make_unique<FullscreenQuad>(device);
	dof_final = std::make_unique<Framebuffer>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, false);


}

void dof::make(ID3D11DeviceContext* immediate_context, ID3D11ShaderResourceView* color_map) {
	ID3D11ShaderResourceView* null_srvs[2] = { nullptr, nullptr };
	ID3D11UnorderedAccessView* null_uav = nullptr;
	dof_final->Clear(Graphics_Core::instance().get_device_context(), 1, 1, 1, 1);
	dof_final->Activate(immediate_context);

	// カラーマップ(t0)と深度バッファ(t1)を同時に渡す
	ID3D11ShaderResourceView* resources[] = { color_map, Graphics_Core::instance().get_geometry_buffer()->GetDepth() };
	bit_block_transfer->Blit(immediate_context, resources, 0, 2, Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("DOF_PS"));

	dof_final->Deactivate(immediate_context);

	immediate_context->PSSetShaderResources(0, 2, null_srvs);
}