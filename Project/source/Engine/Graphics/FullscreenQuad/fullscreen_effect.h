#pragma once

#include <d3d11.h>
#include <wrl.h>

/// Constant-buffer付きの画面全体HLSLエフェクト。
class FullscreenEffect
{
public:
	bool initialize(ID3D11Device* device, const char* pixelShaderCso, UINT constantBufferSize);

	bool is_initialized() const { return m_pixelShader && m_constantBuffer; }

	void draw(const void* constants, UINT constantBufferSize);

private:
	Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;
	UINT m_constantBufferSize = 0;
};
