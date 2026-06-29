#include "exponential_fog.h"
#include "Engine/system/render_state.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"

ExponentialFog::ExponentialFog(ID3D11Device* device, uint32_t width, uint32_t height)
{
	blit_ = std::make_unique<FullscreenQuad>(device);
	target_ = std::make_unique<Framebuffer>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, false);

	create_ps_from_cso(device, "exponential_fog_ps.cso", ps_.GetAddressOf());

	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.ByteWidth = sizeof(Config);
	device->CreateBuffer(&desc, nullptr, cb_.GetAddressOf());
}

void ExponentialFog::make(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src_srv)
{
	ID3D11ShaderResourceView* null_srv = nullptr;
	ctx->PSSetShaderResources(0, 1, &null_srv);

	Render_State::instance().set_2d_render_states(ctx);

	config.is_enabled = is_enabled ? 1 : 0;

	ctx->UpdateSubresource(cb_.Get(), 0, nullptr, &config, 0, 0);
	ctx->PSSetConstantBuffers(8, 1, cb_.GetAddressOf());

	target_->Clear(ctx, 0, 0, 0, 1);
	target_->Activate(ctx);
	{
		blit_->Blit(ctx, &src_srv, 4, 1, ps_.Get());
	}
	target_->Deactivate(ctx);

	ctx->PSSetShaderResources(0, 1, &null_srv);
}

ID3D11ShaderResourceView* ExponentialFog::get_color_map() const
{
	return target_->GetColorMap();
}

ID3D11ShaderResourceView** ExponentialFog::get_color_map_address() const
{
	return target_->GetColorMapAddress();
}

void ExponentialFog::DrawDebugUI()
{

	ImGui::Checkbox("Enable##ExpFog", &is_enabled);
	if (!is_enabled) return;

	ImGui::SeparatorText("Exponential Settings");
	ImGui::SliderFloat("Density", &config.density, 0.0f, 0.1f, "%.4f");
	ImGui::Combo("Mode", &config.mode, "Exponential\0Exponential 2\0\0");

	ImGui::SeparatorText("Appearance");
	ImGui::ColorEdit3("Fog Color", config.fog_color);
	ImGui::SliderFloat("Intensity", &config.intensity, 0.0f, 5.0f);

}