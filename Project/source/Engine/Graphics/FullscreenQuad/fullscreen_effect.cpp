#include "fullscreen_effect.h"
#include "Engine/Graphics/Shader/shader.h"
#include "Engine/System/graphics_core.h"
#include "Engine/System/render_state.h"

bool FullscreenEffect::initialize(ID3D11Device* device, const char* pixelShaderCso, UINT constantBufferSize)
{
	if (is_initialized() && m_constantBufferSize == constantBufferSize) return true;
	if (!device || constantBufferSize == 0 || constantBufferSize % 16 != 0) return false;
	if (!create_ps_from_cso(device, pixelShaderCso, m_pixelShader.ReleaseAndGetAddressOf())) return false;

	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = constantBufferSize;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (FAILED(device->CreateBuffer(&desc, nullptr, m_constantBuffer.ReleaseAndGetAddressOf()))) return false;
	m_constantBufferSize = constantBufferSize;
	return true;
}

void FullscreenEffect::draw(const void* constants, UINT constantBufferSize)
{
	if (!is_initialized() || !constants || constantBufferSize != m_constantBufferSize) return;
	auto& graphics = Graphics_Core::instance();
	ID3D11DeviceContext* context = graphics.get_device_context();
	graphics.set_render_targets();
	Render_State::instance().set_2d_render_states(context);
	context->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, constants, 0, 0);
	ID3D11Buffer* buffer = m_constantBuffer.Get();
	context->PSSetConstantBuffers(11, 1, &buffer);
	graphics.get_fullscreen_quad()->Draw(context, m_pixelShader.Get());
}
