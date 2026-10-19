#include "shader.h"
#include "Engine/System/Manager/resource_manager.h"

bool create_vs_from_cso(ID3D11Device* device, const char* cso_name,
	ID3D11VertexShader** vertex_shader, ID3D11InputLayout** input_layout,
	D3D11_INPUT_ELEMENT_DESC* input_element_desc, UINT num_elements)
{
	(void)device;
	if (!vertex_shader || !cso_name) return false;
	auto shader = Resource_Manager::instance().get_shader_file<Vertex_Shader>(cso_name);
	if (!shader) return false;
	if (input_layout && !shader->create_input_layout(input_element_desc, num_elements)) return false;
	*vertex_shader = shader->get();
	if (*vertex_shader) (*vertex_shader)->AddRef();
	if (input_layout) {
		*input_layout = shader->get_input_layout();
		if (*input_layout) (*input_layout)->AddRef();
	}
	return *vertex_shader != nullptr;
}

bool create_ps_from_cso(ID3D11Device* device, const char* cso_name,
	ID3D11PixelShader** pixel_shader)
{
	(void)device;
	if (!pixel_shader || !cso_name) return false;
	auto shader = Resource_Manager::instance().get_shader_file<Pixel_Shader>(cso_name);
	if (!shader || !shader->get()) return false;
	*pixel_shader = shader->get();
	(*pixel_shader)->AddRef();
	return true;
}

bool create_cs_from_cso(ID3D11Device* device, const char* cso_name,
	ID3D11ComputeShader** compute_shader)
{
	(void)device;
	if (!compute_shader || !cso_name) return false;
	auto shader = Resource_Manager::instance().get_shader_file<Compute_Shader>(cso_name);
	if (!shader || !shader->get()) return false;
	*compute_shader = shader->get();
	(*compute_shader)->AddRef();
	return true;
}
