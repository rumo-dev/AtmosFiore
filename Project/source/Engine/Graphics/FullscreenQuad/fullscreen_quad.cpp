#include "fullscreen_quad.h"
#include "Engine/Graphics/Shader/shader.h"
#include "Engine/Utilities/misc.h"

FullscreenQuad::FullscreenQuad(ID3D11Device* device)
{
	create_vs_from_cso(device, "fullscreen_quad_vs.cso", m_embeddedVertexShader.ReleaseAndGetAddressOf(),
		nullptr, nullptr, 0);
	create_ps_from_cso(device, "fullscreen_quad_ps.cso", m_embeddedPixelShader.ReleaseAndGetAddressOf());
}
void FullscreenQuad::Blit(ID3D11DeviceContext* immediate_context,
	ID3D11ShaderResourceView** shader_resource_view, uint32_t start_slot, uint32_t num_views,
	ID3D11PixelShader* replaced_pixel_shader)
{
	immediate_context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	immediate_context->IASetInputLayout(nullptr);

	immediate_context->VSSetShader(m_embeddedVertexShader.Get(), 0, 0);
	replaced_pixel_shader ? immediate_context->PSSetShader(replaced_pixel_shader, 0, 0) :
		immediate_context->PSSetShader(m_embeddedPixelShader.Get(), 0, 0);
	for (int i = 0; i < num_views; i++) {
		_ASSERT_EXPR(shader_resource_view[i] != nullptr, L"shader_resource_view is null!");
	}

	immediate_context->PSSetShaderResources(start_slot, num_views, shader_resource_view);

	immediate_context->Draw(4, 0);
}
