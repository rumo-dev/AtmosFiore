#include "height_fog.h"
#include "Engine/system/render_state.h"
#include "Engine/Graphics/shader/shader.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"

HeightFog::HeightFog(ID3D11Device* device, uint32_t width, uint32_t height)
{
	blit_ = std::make_unique<FullscreenQuad>(device);
	target_ = std::make_unique<Framebuffer>(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, false);

	create_ps_from_cso(device, "height_fog_ps.cso", ps_.GetAddressOf());

	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.ByteWidth = sizeof(Config);
	device->CreateBuffer(&desc, nullptr, cb_.GetAddressOf());
}

void HeightFog::make(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src_srv)
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

ID3D11ShaderResourceView* HeightFog::get_color_map() const
{
	return target_->GetColorMap();
}

ID3D11ShaderResourceView** HeightFog::get_color_map_address() const
{
	return target_->GetColorMapAddress();
}

void HeightFog::DrawDebugUI()
{

	ImGui::Checkbox("Enable##HFog", &is_enabled);
	if (!is_enabled) return;

	ImGui::SeparatorText("Height Settings");
	ImGui::SliderFloat("Base Height", &config.base_height, -50.0f, 50.0f);
	ImGui::SliderFloat("Falloff", &config.falloff, 0.001f, 2.0f);
	ImGui::SliderFloat("Density Max", &config.density_max, 0.0f, 1.0f);

	ImGui::SeparatorText("Appearance");
	ImGui::ColorEdit3("Fog Color", &config.fog_color.x);
	ImGui::SliderFloat("Intensity", &config.intensity, 0.0f, 5.0f);

	// ── 追加：ノイズとアニメーションのデバッグUI ──
	ImGui::SeparatorText("Noise & Animation");
	ImGui::SliderFloat("Noise Scale", &config.noise_scale, 0.001f, 1.0f, "%.4f");
	ImGui::SliderFloat("Noise Strength", &config.noise_strength, 0.0f, 5.0f);
	ImGui::SliderFloat3("Wind Velocity", &config.wind_velocity.x, -5.0f, 5.0f);

	// ※ config.time は C++ の Update() 等で毎フレーム自動更新される前提のため、
	// 入力用のUIからは省いています。

}
