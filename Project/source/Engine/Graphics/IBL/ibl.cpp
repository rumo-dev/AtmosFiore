#include "IBL.h"
#include "Engine/Graphics/Texture/texture.h"   
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> IBL::m_srvs[TextureCount];
HRESULT IBL::Initialize(ID3D11Device* device, const std::wstring& basePath)
{
	D3D11_TEXTURE2D_DESC desc = {};

	HRESULT hr{ S_OK };

	hr = load_texture_from_file(
		device,
		(basePath + L"/sunset_jhbcentral_4k/sunset_jhbcentral_4k.dds").c_str(),
		m_srvs[Environment].GetAddressOf(),
		&desc
	);

	hr = load_texture_from_file(
		device,
		(basePath + L"/sunset_jhbcentral_4k/diffuse_iem.dds").c_str(),
		m_srvs[DiffuseIEM].GetAddressOf(),
		&desc
	);

	hr = load_texture_from_file(
		device,
		(basePath + L"/sunset_jhbcentral_4k/specular_pmrem.dds").c_str(),
		m_srvs[SpecularPMREM].GetAddressOf(),
		&desc
	);

	hr = load_texture_from_file(
		device,
		(basePath + L"/lut_ggx.dds").c_str(),
		m_srvs[LUT_GGX].GetAddressOf(),
		&desc
	);

	return hr;
}

void IBL::Bind(ID3D11DeviceContext* context)
{
	// シェーダスロットにセット（32-35）
	context->PSSetShaderResources(32, 1, m_srvs[Environment].GetAddressOf());
	context->PSSetShaderResources(33, 1, m_srvs[DiffuseIEM].GetAddressOf());
	context->PSSetShaderResources(34, 1, m_srvs[SpecularPMREM].GetAddressOf());
	context->PSSetShaderResources(35, 1, m_srvs[LUT_GGX].GetAddressOf());
}
