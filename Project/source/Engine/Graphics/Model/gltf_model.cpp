#include "gltf_model.h"

#include <stack>
#include "Engine/Utilities/misc.h"
#include "Engine/System/graphics_core.h"
#include "Engine/System/Manager/resource_manager.h"
#define TINYGLTF_IMPLEMENTATION


bool null_load_image_data(tinygltf::Image*, const int, std::string*, std::string*,
	int, int, const unsigned char*, int, void*)
{
	return true;
}

void Gltf_Model::compute_instance_aabb(
	const DirectX::XMFLOAT4X4& world,
	bool is_animation,
	float animation_time,
	animation_mode anim_mode,
	int animation_index,
	DirectX::XMFLOAT3& out_min,
	DirectX::XMFLOAT3& out_max) const
{
	using namespace DirectX;

	if (!has_bbox) {
		// Fallback: return identity bbox
		out_min = XMFLOAT3(-1, -1, -1);
		out_max = XMFLOAT3(1, 1, 1);
		return;
	}

	// Transform model-local bbox corners to world space
	XMMATRIX world_m = XMLoadFloat4x4(&world);

	// 8 corners of the local bbox
	XMVECTOR corners[8] = {
		XMVectorSet(bbox_min.x, bbox_min.y, bbox_min.z, 1.0f),
		XMVectorSet(bbox_max.x, bbox_min.y, bbox_min.z, 1.0f),
		XMVectorSet(bbox_min.x, bbox_max.y, bbox_min.z, 1.0f),
		XMVectorSet(bbox_max.x, bbox_max.y, bbox_min.z, 1.0f),
		XMVectorSet(bbox_min.x, bbox_min.y, bbox_max.z, 1.0f),
		XMVectorSet(bbox_max.x, bbox_min.y, bbox_max.z, 1.0f),
		XMVectorSet(bbox_min.x, bbox_max.y, bbox_max.z, 1.0f),
		XMVectorSet(bbox_max.x, bbox_max.y, bbox_max.z, 1.0f),
	};

	// Transform all corners
	XMVECTOR wmin = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 1.0f);
	XMVECTOR wmax = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 1.0f);

	for (int i = 0; i < 8; ++i) {
		XMVECTOR tv = XMVector3TransformCoord(corners[i], world_m);
		wmin = XMVectorMin(wmin, tv);
		wmax = XMVectorMax(wmax, tv);
	}

	XMStoreFloat3(&out_min, wmin);
	XMStoreFloat3(&out_max, wmax);
}
Gltf_Model::Gltf_Model(ID3D11Device* device, const std::string& filename) : filename(filename)
{

	tinygltf::TinyGLTF tiny_gltf;
	tiny_gltf.SetImageLoader(null_load_image_data, nullptr);

	tinygltf::Model Gltf_Model;
	std::string error, warning;
	bool succeeded{ false };
	if (filename.find(".glb") != std::string::npos)
	{
		succeeded = tiny_gltf.LoadBinaryFromFile(&Gltf_Model, &error, &warning, filename.c_str());
	}
	else if (filename.find(".gltf") != std::string::npos)
	{
		succeeded = tiny_gltf.LoadASCIIFromFile(&Gltf_Model, &error, &warning, filename.c_str());
	}

	_ASSERT_EXPR_A(warning.empty(), warning.c_str());
	_ASSERT_EXPR_A(error.empty(), error.c_str());
	_ASSERT_EXPR_A(succeeded, L"Failed to load glTF file");
	for (std::vector<tinygltf::Scene>::const_reference gltf_scene : Gltf_Model.scenes)
	{
		scene& scene{ scenes.emplace_back() };
		scene.name = Gltf_Model.scenes.at(0).name;
		scene.nodes = Gltf_Model.scenes.at(0).nodes;
	}
	fetch_nodes(Gltf_Model);
	fetch_meshes(Graphics_Core::instance().get_device(), Gltf_Model);
	fetch_materials(Graphics_Core::instance().get_device(), Gltf_Model);
	fetch_textures(Graphics_Core::instance().get_device(), Gltf_Model);
	fetch_animations(Gltf_Model);


	// TODO: This is a force-brute programming, may cause bugs.
	const std::map<std::string, buffer_view>& vertex_buffer_views{
	  meshes.at(0).primitives.at(0).vertex_buffer_views };
	D3D11_INPUT_ELEMENT_DESC input_element_desc[]
	{
	  { "POSITION", 0, vertex_buffer_views.at("POSITION").format, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	  { "NORMAL", 0, vertex_buffer_views.at("NORMAL").format, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	  { "TANGENT", 0, vertex_buffer_views.at("TANGENT").format, 2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	  { "TEXCOORD", 0, vertex_buffer_views.at("TEXCOORD_0").format, 3, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	  { "JOINTS", 0, vertex_buffer_views.at("JOINTS_0").format, 4, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	  { "WEIGHTS", 0,vertex_buffer_views.at("WEIGHTS_0").format, 5, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	;
	for (const D3D11_INPUT_ELEMENT_DESC& desc : input_element_desc)
	{
		log_printf("    SemanticName: %s, Format: %d, InputSlot: %d\n", LogLevel::Warning,
			desc.SemanticName, desc.Format, desc.InputSlot);
	}
	//create_vs_from_cso(device, "gltf_model_vs.cso", vertex_shader.ReleaseAndGetAddressOf(),
	//	input_layout.ReleaseAndGetAddressOf(), input_element_desc, _countof(input_element_desc));
	Resource_Manager::instance().shader_manager.Get<Vertex_Shader>("GLTF_VS")->create_input_layout(
		input_element_desc, _countof(input_element_desc));
	Resource_Manager::instance().shader_manager.Get<Vertex_Shader>("POINT_SHADOW_VS")->create_input_layout(
		input_element_desc, _countof(input_element_desc));
	Resource_Manager::instance().shader_manager.Get<Vertex_Shader>("DIRECTIONAL_SHADOW_VS")->create_input_layout(
		input_element_desc, _countof(input_element_desc));
	//create_ps_from_cso(device, "gltf_model_ps.cso", pixel_shader.ReleaseAndGetAddressOf());

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(primitive_constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	HRESULT hr;
	hr = device->CreateBuffer(&buffer_desc, nullptr, primitive_cbuffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(primitive_joint_constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&buffer_desc, NULL, primitive_joint_cbuffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));


}


//fetch node
void Gltf_Model::fetch_nodes(const tinygltf::Model& Gltf_Model)
{
	for (std::vector<tinygltf::Node>::const_reference gltf_node : Gltf_Model.nodes)
	{
		node& node{ nodes.emplace_back() };
		node.name = gltf_node.name;
		node.skin = gltf_node.skin;
		node.mesh = gltf_node.mesh;
		node.children = gltf_node.children;
		if (!gltf_node.matrix.empty())
		{
			DirectX::XMFLOAT4X4 matrix;
			for (size_t row = 0; row < 4; row++)
			{
				for (size_t column = 0; column < 4; column++)
				{
					matrix(row, column) = static_cast<float>(gltf_node.matrix.at(4 * row + column));
				}
			}

			DirectX::XMVECTOR S, T, R;
			bool succeed = DirectX::XMMatrixDecompose(&S, &R, &T, DirectX::XMLoadFloat4x4(&matrix));
			_ASSERT_EXPR(succeed, L"Failed to decompose matrix.");

			DirectX::XMStoreFloat3(&node.scale, S);
			DirectX::XMStoreFloat4(&node.rotation, R);
			DirectX::XMStoreFloat3(&node.translation, T);
		}
		else
		{
			if (gltf_node.scale.size() > 0)
			{
				node.scale.x = static_cast<float>(gltf_node.scale.at(0));
				node.scale.y = static_cast<float>(gltf_node.scale.at(1));
				node.scale.z = static_cast<float>(gltf_node.scale.at(2));
			}
			if (gltf_node.translation.size() > 0)
			{
				node.translation.x = static_cast<float>(gltf_node.translation.at(0));
				node.translation.y = static_cast<float>(gltf_node.translation.at(1));
				node.translation.z = static_cast<float>(gltf_node.translation.at(2));
			}
			if (gltf_node.rotation.size() > 0)
			{
				node.rotation.x = static_cast<float>(gltf_node.rotation.at(0));
				node.rotation.y = static_cast<float>(gltf_node.rotation.at(1));
				node.rotation.z = static_cast<float>(gltf_node.rotation.at(2));
				node.rotation.w = static_cast<float>(gltf_node.rotation.at(3));
			}
		}
	}
	cumulate_transforms(nodes);
}
Gltf_Model::buffer_view Gltf_Model::make_buffer_view(const tinygltf::Accessor& accessor)
{
	buffer_view buffer_view;
	switch (accessor.type)
	{
	case TINYGLTF_TYPE_SCALAR:
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			buffer_view.format = DXGI_FORMAT_R16_UINT;
			buffer_view.stride_in_bytes = sizeof(USHORT);
			break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			buffer_view.format = DXGI_FORMAT_R32_UINT;
			buffer_view.stride_in_bytes = sizeof(UINT);
			break;
		default:
			_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
			break;
		}
		break;
	case TINYGLTF_TYPE_VEC2:
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			buffer_view.format = DXGI_FORMAT_R32G32_FLOAT;
			buffer_view.stride_in_bytes = sizeof(FLOAT) * 2;
			break;
		default:
			_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
			break;
		}
		break;
	case TINYGLTF_TYPE_VEC3:
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			buffer_view.format = DXGI_FORMAT_R32G32B32_FLOAT;
			buffer_view.stride_in_bytes = sizeof(FLOAT) * 3;
			break;
		default:
			_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
			break;
		}
		break;
	case TINYGLTF_TYPE_VEC4:
		switch (accessor.componentType)
		{
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
			buffer_view.format = DXGI_FORMAT_R16G16B16A16_UINT;
			buffer_view.stride_in_bytes = sizeof(USHORT) * 4;
			break;
		case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			buffer_view.format = DXGI_FORMAT_R32G32B32A32_UINT;
			buffer_view.stride_in_bytes = sizeof(UINT) * 4;
			break;
		case TINYGLTF_COMPONENT_TYPE_FLOAT:
			buffer_view.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			buffer_view.stride_in_bytes = sizeof(FLOAT) * 4;
			break;
		default:
			_ASSERT_EXPR(FALSE, L"This accessor component type is not supported.");
			break;
		}
		break;
	default:
		_ASSERT_EXPR(FALSE, L"This accessor type is not supported.");
		break;
	}
	buffer_view.size_in_bytes = static_cast<UINT>(accessor.count * buffer_view.stride_in_bytes);
	return buffer_view;
}
void Gltf_Model::fetch_meshes(ID3D11Device* device, const tinygltf::Model& Gltf_Model)
{
	HRESULT hr;
	for (std::vector<tinygltf::Mesh>::const_reference gltf_mesh : Gltf_Model.meshes)
	{
		mesh& mesh{ meshes.emplace_back() };
		mesh.name = gltf_mesh.name;
		// Accumulate positions to compute model-local AABB
		for (std::vector<tinygltf::Primitive>::const_reference gltf_primitive : gltf_mesh.primitives)
		{
			auto it = gltf_primitive.attributes.find("POSITION");
			if (it != gltf_primitive.attributes.end())
			{
				const tinygltf::Accessor& pos_accessor = Gltf_Model.accessors.at(it->second);
				const tinygltf::BufferView& pos_bv = Gltf_Model.bufferViews.at(pos_accessor.bufferView);
				const tinygltf::Buffer& buffer = Gltf_Model.buffers.at(pos_bv.buffer);
				const size_t byteOffset = pos_bv.byteOffset + pos_accessor.byteOffset;
				const size_t count = pos_accessor.count;
				const float* data = reinterpret_cast<const float*>(buffer.data.data() + byteOffset);
				for (size_t i = 0; i < count; ++i)
				{
					float x = data[i * 3 + 0];
					float y = data[i * 3 + 1];
					float z = data[i * 3 + 2];
					bbox_min.x = std::min(bbox_min.x, x);
					bbox_min.y = std::min(bbox_min.y, y);
					bbox_min.z = std::min(bbox_min.z, z);
					bbox_max.x = std::max(bbox_max.x, x);
					bbox_max.y = std::max(bbox_max.y, y);
					bbox_max.z = std::max(bbox_max.z, z);
					has_bbox = true;
				}
			}
		}
		for (std::vector<tinygltf::Primitive>::const_reference gltf_primitive : gltf_mesh.primitives)
		{
			mesh::primitive& primitive{ mesh.primitives.emplace_back() };
			primitive.material = gltf_primitive.material;

			// Create index buffer
			const tinygltf::Accessor& gltf_accessor{ Gltf_Model.accessors.at(gltf_primitive.indices) };
			const tinygltf::BufferView& gltf_buffer_view{ Gltf_Model.bufferViews.at(gltf_accessor.bufferView) };

			primitive.index_buffer_view = make_buffer_view(gltf_accessor);

			D3D11_BUFFER_DESC buffer_desc{};
			buffer_desc.ByteWidth = static_cast<UINT>(primitive.index_buffer_view.size_in_bytes);
			buffer_desc.Usage = D3D11_USAGE_DEFAULT;
			buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			D3D11_SUBRESOURCE_DATA subresource_data{};
			subresource_data.pSysMem = Gltf_Model.buffers.at(gltf_buffer_view.buffer).data.data()
				+ gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
			hr = device->CreateBuffer(&buffer_desc, &subresource_data,
				primitive.index_buffer_view.buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

			// CPU-side copy of indices for collision triangle extraction
			{
				const void* raw = Gltf_Model.buffers.at(gltf_buffer_view.buffer).data.data()
					+ gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
				primitive.cpu_indices.resize(gltf_accessor.count);
				if (gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					const uint16_t* src = reinterpret_cast<const uint16_t*>(raw);
					for (size_t i = 0; i < gltf_accessor.count; ++i)
						primitive.cpu_indices[i] = static_cast<uint32_t>(src[i]);
				}
				else if (gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
				{
					const uint32_t* src = reinterpret_cast<const uint32_t*>(raw);
					for (size_t i = 0; i < gltf_accessor.count; ++i)
						primitive.cpu_indices[i] = src[i];
				}
			}

			// Create vertex buffers
			for (std::map<std::string, int>::const_reference gltf_attribute : gltf_primitive.attributes)
			{
				const tinygltf::Accessor& gltf_accessor{ Gltf_Model.accessors.at(gltf_attribute.second) };
				const tinygltf::BufferView& gltf_buffer_view{ Gltf_Model.bufferViews.at(gltf_accessor.bufferView) };

				buffer_view vertex_buffer_view{ make_buffer_view(gltf_accessor) };

				D3D11_BUFFER_DESC buffer_desc{};
				buffer_desc.ByteWidth = static_cast<UINT>(vertex_buffer_view.size_in_bytes);
				buffer_desc.Usage = D3D11_USAGE_DEFAULT;
				buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
				D3D11_SUBRESOURCE_DATA subresource_data{};
				subresource_data.pSysMem = Gltf_Model.buffers.at(gltf_buffer_view.buffer).data.data()
					+ gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
				hr = device->CreateBuffer(&buffer_desc, &subresource_data,
					vertex_buffer_view.buffer.ReleaseAndGetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

				// If this is POSITION attribute, keep a CPU copy for dynamic AABB computation
				if (gltf_attribute.first == "POSITION")
				{
					const size_t byteOffset = gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
					const tinygltf::Buffer& buffer = Gltf_Model.buffers.at(gltf_buffer_view.buffer);
					const float* data = reinterpret_cast<const float*>(buffer.data.data() + byteOffset);
					primitive.cpu_positions.resize(gltf_accessor.count);
					for (size_t i = 0; i < gltf_accessor.count; ++i)
					{
						primitive.cpu_positions[i].x = data[i * 3 + 0];
						primitive.cpu_positions[i].y = data[i * 3 + 1];
						primitive.cpu_positions[i].z = data[i * 3 + 2];
					}
				}

				primitive.vertex_buffer_views.emplace(std::make_pair(gltf_attribute.first, vertex_buffer_view));
			}

			// Add dummy attributes if any are missing.
			const std::unordered_map<std::string, buffer_view> attributes{
			  { "TANGENT", { DXGI_FORMAT_R32G32B32A32_FLOAT } },
			  { "TEXCOORD_0", { DXGI_FORMAT_R32G32_FLOAT } },
			  { "JOINTS_0", { DXGI_FORMAT_R16G16B16A16_UINT } },
			  { "WEIGHTS_0", { DXGI_FORMAT_R32G32B32A32_FLOAT } },
			};
			for (std::unordered_map<std::string, buffer_view>::const_reference attribute : attributes)
			{
				if (primitive.vertex_buffer_views.find(attribute.first) == primitive.vertex_buffer_views.end())
				{
					primitive.vertex_buffer_views.insert(std::make_pair(attribute.first, attribute.second));
				}
			}

		}
	}
}
void Gltf_Model::fetch_materials(ID3D11Device* device, const tinygltf::Model& Gltf_Model)
{
	for (std::vector<tinygltf::Material>::const_reference gltf_material : Gltf_Model.materials)
	{
		std::vector<material>::reference material = materials.emplace_back();

		material.name = gltf_material.name;

		material.data.emissive_factor[0] = static_cast<float>(gltf_material.emissiveFactor.at(0));
		material.data.emissive_factor[1] = static_cast<float>(gltf_material.emissiveFactor.at(1));
		material.data.emissive_factor[2] = static_cast<float>(gltf_material.emissiveFactor.at(2));

		material.data.alpha_mode = gltf_material.alphaMode == "OPAQUE" ?
			0 : gltf_material.alphaMode == "MASK" ? 1 : gltf_material.alphaMode == "BLEND" ? 2 : 0;
		material.data.alpha_cutoff = static_cast<float>(gltf_material.alphaCutoff);
		material.data.double_sided = gltf_material.doubleSided ? 1 : 0;

		material.data.pbr_metallic_roughness.basecolor_factor[0] =
			static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(0));
		material.data.pbr_metallic_roughness.basecolor_factor[1] =
			static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(1));
		material.data.pbr_metallic_roughness.basecolor_factor[2] =
			static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(2));
		material.data.pbr_metallic_roughness.basecolor_factor[3] =
			static_cast<float>(gltf_material.pbrMetallicRoughness.baseColorFactor.at(3));
		material.data.pbr_metallic_roughness.basecolor_texture.index =
			gltf_material.pbrMetallicRoughness.baseColorTexture.index;
		material.data.pbr_metallic_roughness.basecolor_texture.texcoord =
			gltf_material.pbrMetallicRoughness.baseColorTexture.texCoord;
		material.data.pbr_metallic_roughness.metallic_factor =
			static_cast<float>(gltf_material.pbrMetallicRoughness.metallicFactor);
		material.data.pbr_metallic_roughness.roughness_factor =
			static_cast<float>(gltf_material.pbrMetallicRoughness.roughnessFactor);
		material.data.pbr_metallic_roughness.metallic_roughness_texture.index =
			gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index;
		material.data.pbr_metallic_roughness.metallic_roughness_texture.texcoord =
			gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord;

		material.data.normal_texture.index = gltf_material.normalTexture.index;
		material.data.normal_texture.texcoord = gltf_material.normalTexture.texCoord;
		material.data.normal_texture.scale = static_cast<float>(gltf_material.normalTexture.scale);

		material.data.occlusion_texture.index = gltf_material.occlusionTexture.index;
		material.data.occlusion_texture.texcoord = gltf_material.occlusionTexture.texCoord;
		material.data.occlusion_texture.strength =
			static_cast<float>(gltf_material.occlusionTexture.strength);

		material.data.emissive_texture.index = gltf_material.emissiveTexture.index;
		material.data.emissive_texture.texcoord = gltf_material.emissiveTexture.texCoord;
	}

	// Create material data as shader resource view on GPU
	std::vector<material::cbuffer> material_data;
	for (std::vector<material>::const_reference material : materials)
	{
		material_data.emplace_back(material.data);
	}

	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D11Buffer> material_buffer;
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(material::cbuffer) * material_data.size());
	buffer_desc.StructureByteStride = sizeof(material::cbuffer);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	buffer_desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	D3D11_SUBRESOURCE_DATA subresource_data{};
	subresource_data.pSysMem = material_data.data();
	hr = device->CreateBuffer(&buffer_desc, &subresource_data, material_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
	D3D11_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc{};
	shader_resource_view_desc.Format = DXGI_FORMAT_UNKNOWN;
	shader_resource_view_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	shader_resource_view_desc.Buffer.NumElements = static_cast<UINT>(material_data.size());
	hr = device->CreateShaderResourceView(material_buffer.Get(),
		&shader_resource_view_desc, material_resource_view.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}
void Gltf_Model::fetch_textures(ID3D11Device* device, const tinygltf::Model& Gltf_Model)
{
	HRESULT hr{ S_OK };
	for (const tinygltf::Texture& gltf_texture : Gltf_Model.textures)
	{
		texture& texture{ textures.emplace_back() };
		texture.name = gltf_texture.name;
		texture.source = gltf_texture.source;
	}
	for (const tinygltf::Image& gltf_image : Gltf_Model.images)
	{
		image& image{ images.emplace_back() };
		image.name = gltf_image.name;
		image.width = gltf_image.width;
		image.height = gltf_image.height;
		image.component = gltf_image.component;
		image.bits = gltf_image.bits;
		image.pixel_type = gltf_image.pixel_type;
		image.buffer_view = gltf_image.bufferView;
		image.mime_type = gltf_image.mimeType;
		image.uri = gltf_image.uri;
		image.as_is = gltf_image.as_is;

		if (gltf_image.bufferView > -1)
		{
			const tinygltf::BufferView& buffer_view{ Gltf_Model.bufferViews.at(gltf_image.bufferView) };
			const tinygltf::Buffer& buffer{ Gltf_Model.buffers.at(buffer_view.buffer) };
			const ::byte* data = buffer.data.data() + buffer_view.byteOffset;

			ID3D11ShaderResourceView* texture_resource_view{};
			hr = load_texture_from_memory(device, data, buffer_view.byteLength, &texture_resource_view);
			if (hr == S_OK)
			{
				texture_resource_views.emplace_back().Attach(texture_resource_view);
			}
		}
		else
		{
			const std::filesystem::path path(filename);
			ID3D11ShaderResourceView* shader_resource_view{};
			D3D11_TEXTURE2D_DESC texture2d_desc;
			std::wstring filename{
			  path.parent_path().concat(L"/").wstring() +
			  std::wstring(gltf_image.uri.begin(), gltf_image.uri.end()) };
			hr = load_texture_from_file(device, filename.c_str(), &shader_resource_view, &texture2d_desc);
			if (hr == S_OK)
			{
				texture_resource_views.emplace_back().Attach(shader_resource_view);
			}
		}
	}
}
void Gltf_Model::fetch_animations(const tinygltf::Model& Gltf_Model)
{
	using namespace std;
	using namespace tinygltf;
	using namespace DirectX;

	for (vector<Skin>::const_reference transmission_skin : Gltf_Model.skins)
	{
		skin& skin{ skins.emplace_back() };
		const Accessor& gltf_accessor{ Gltf_Model.accessors.at(transmission_skin.inverseBindMatrices) };
		const BufferView& gltf_buffer_view{ Gltf_Model.bufferViews.at(gltf_accessor.bufferView) };
		skin.inverse_bind_matrices.resize(gltf_accessor.count);
		memcpy(skin.inverse_bind_matrices.data(), Gltf_Model.buffers.at(gltf_buffer_view.buffer).data.data() +
			gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(XMFLOAT4X4));
		skin.joints = transmission_skin.joints;
	}

	for (vector<Animation>::const_reference gltf_animation : Gltf_Model.animations)
	{
		animation& animation{ animations.emplace_back() };
		animation.name = gltf_animation.name;
		for (vector<AnimationSampler>::const_reference gltf_sampler : gltf_animation.samplers)
		{
			animation::sampler& sampler{ animation.samplers.emplace_back() };
			sampler.input = gltf_sampler.input;
			sampler.output = gltf_sampler.output;
			sampler.interpolation = gltf_sampler.interpolation;

			const Accessor& gltf_accessor{ Gltf_Model.accessors.at(gltf_sampler.input) };
			const BufferView& gltf_buffer_view{ Gltf_Model.bufferViews.at(gltf_accessor.bufferView) };
			const pair<unordered_map<int, vector<float>>::iterator, bool>& timelines{
				animation.timelines.emplace(gltf_sampler.input,gltf_accessor.count) };
			if (timelines.second)
			{
				memcpy(timelines.first->second.data(), Gltf_Model.buffers.at(gltf_buffer_view.buffer).data.data() +
					gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(FLOAT));
			}
		}

		for (vector<AnimationChannel>::const_reference gltf_channel : gltf_animation.channels)
		{
			animation::channel& channel{ animation.channels.emplace_back() };
			channel.sampler = gltf_channel.sampler;
			channel.target_node = gltf_channel.target_node;
			channel.target_path = gltf_channel.target_path;

			const AnimationSampler& gltf_sampler{ gltf_animation.samplers.at(gltf_channel.sampler) };
			const Accessor& gltf_accessor{ Gltf_Model.accessors.at(gltf_sampler.output) };
			const BufferView& gltf_buffer_view{ Gltf_Model.bufferViews.at(gltf_accessor.bufferView) };
			if (gltf_channel.target_path == "scale")
			{
				const pair<unordered_map<int, vector<XMFLOAT3>>::iterator, bool>& scales{
					animation.scales.emplace(gltf_sampler.output, gltf_accessor.count)
				};
				if (scales.second)
				{
					memcpy(scales.first->second.data(), Gltf_Model.buffers.at(gltf_buffer_view.buffer).data.data() +
						gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(XMFLOAT3));
				}
			}
			else if (gltf_channel.target_path == "rotation")
			{
				const pair<unordered_map<int, vector<XMFLOAT4>>::iterator, bool>& rotations{
					 animation.rotations.emplace(gltf_sampler.output, gltf_accessor.count) };
				if (rotations.second)
				{
					memcpy(rotations.first->second.data(), Gltf_Model.buffers.at(gltf_buffer_view.buffer).data.data() +
						gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(XMFLOAT4));
				}
			}
			else if (gltf_channel.target_path == "translation")
			{
				const pair<unordered_map<int, vector<XMFLOAT3>>::iterator, bool>& translations{
					 animation.translations.emplace(gltf_sampler.output, gltf_accessor.count) };
				if (translations.second)
				{
					memcpy(translations.first->second.data(), Gltf_Model.buffers.at(gltf_buffer_view.buffer).data.data() +
						gltf_buffer_view.byteOffset + gltf_accessor.byteOffset, gltf_accessor.count * sizeof(XMFLOAT3));
				}
			}
		}
	}
	// Find a longest animation duration in timeline of each channel.
	// 各チャンネルのタイムライン内で最も長いアニメーション時間を取得する。
	for (decltype(animations)::reference animation : animations)
	{
		for (decltype(animation.timelines)::reference timelines : animation.timelines)
		{
			animation.duration = std::max<float>(animation.duration, timelines.second.back());
		}
	}
}


void Gltf_Model::cumulate_transforms(std::vector<node>& nodes)
{
	using namespace DirectX;

	std::stack<XMFLOAT4X4> parent_global_transforms;
	std::function<void(int)> traverse{ [&](int node_index)->void
	{
	  node& node{nodes.at(node_index)};
	  XMMATRIX S{ XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z) };
	  XMMATRIX R{ XMMatrixRotationQuaternion(
		XMVectorSet(node.rotation.x, node.rotation.y, node.rotation.z, node.rotation.w)) };
	  XMMATRIX T{ XMMatrixTranslation(node.translation.x, node.translation.y, node.translation.z) };
	  XMStoreFloat4x4(&node.global_transform, S * R * T * XMLoadFloat4x4(&parent_global_transforms.top()));
	  for (int child_index : node.children)
	  {
		parent_global_transforms.push(node.global_transform);
		traverse(child_index);
		parent_global_transforms.pop();
	  }
	} };
	for (std::vector<int>::value_type node_index : scenes.at(0).nodes)
	{
		parent_global_transforms.push({ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 });
		traverse(node_index);
		parent_global_transforms.pop();
	}
}
void Gltf_Model::render(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world, pass_mode pass, bool is_animation, float delta_time, animation_mode anim_mode, int animation_index)
{

	if (pass == pass_mode::deferred_geometry)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("GLTF_DEFERRED_GEOMETRY_PS"), NULL, 0);
	}
	else if (pass == pass_mode::forward_opaque)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("GLTF_FORWARD_OPAQUE_PS"), NULL, 0);
	}
	else if (pass == pass_mode::forward_transparency)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("GLTF_FORWARD_TRANSPARENCY_PS"), NULL, 0);
		//immediate_context->PSSetShader(Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("GLTF_MODEL"), NULL, 0);
	}
	else if (pass == pass_mode::directional_shadow)
	{
		immediate_context->PSSetShader(nullptr, nullptr, 0);
	}
	else if (pass == pass_mode::shadow)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("POINT_SHADOW_PS"), NULL, 0);
	}
	else
	{
		_ASSERT_EXPR(FALSE, L"Unknown pass_mode");
	}
	std::vector<Gltf_Model::node> animated_nodes;

	if (is_animation && animations.size() > 0)
	{
		// アニメーションする場合のみノードをコピーする。
		// （以前はここが無条件のコピーになっており、非アニメーションモデルでも
		//   毎フレーム5パス分、無駄にノード配列全体をヒープコピーしていた）
		animated_nodes = nodes;

		time += delta_time;

		if (anim_mode == animation_mode::single)
		{
			// animations.at() は範囲外だと例外を投げてクラッシュする。
			// ModelInstance::animation_index はゲーム側から自由に設定されるため、
			// 範囲外や duration<=0 のクリップを指していても落ちないようにする。
			if (animation_index < 0 || animation_index >= static_cast<int>(animations.size()))
			{
				animated_nodes.clear(); // バインドポーズにフォールバック
			}
			else
			{
				current_animation_index = animation_index;
				const float duration = animations[current_animation_index].duration;

				if (duration <= 0.0f)
				{
					time = 0.0f;
				}
				else if (time >= duration)
				{
					time = 0.0f; // ループ
				}
				animate(current_animation_index, time, animated_nodes);
			}
		}
		else if (anim_mode == animation_mode::all)
		{
			animate_all(time, animated_nodes);
		}
	}

	using namespace DirectX;
	const std::vector<node>& nodes{ !animated_nodes.empty() ? animated_nodes : Gltf_Model::nodes };

	const char* vertex_shader_name;

	switch (pass)
	{
	case pass_mode::directional_shadow:

		vertex_shader_name = "DIRECTIONAL_SHADOW_VS";
		break;
	case pass_mode::shadow:
		vertex_shader_name = "POINT_SHADOW_VS";
		break;
	default:
		vertex_shader_name = "GLTF_VS";
		break;
	}
	auto vs = Resource_Manager::instance().shader_manager.Get<Vertex_Shader>(vertex_shader_name);
	//assert(vs != nullptr); // デバッグ用
	//immediate_context->VSSetShader(vertex_shader.Get(), nullptr, 0);
	immediate_context->VSSetShader(Resource_Manager::instance().shader_manager.GetNative<Vertex_Shader>(vertex_shader_name), nullptr, 0);
	//immediate_context->PSSetShader(Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("GLTF_PS"), nullptr, 0);
	//immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);
	immediate_context->PSSetShaderResources(0, 1, material_resource_view.GetAddressOf());
	immediate_context->IASetInputLayout(Resource_Manager::instance().shader_manager.Get<Vertex_Shader>(vertex_shader_name)->get_input_layout());
	//immediate_context->IASetInputLayout(input_layout.Get());
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	std::function<void(int)> traverse{ [&](int node_index)->void {
	  const node& node{nodes.at(node_index)};
		if (node.skin > -1)
	{
		const skin& skin{ skins.at(node.skin) };
		primitive_joint_constants primitive_joint_data{};
		// ノードのグローバル変換の逆行列はジョイント数に関係なく同じ値なので、
		// ループの外で1回だけ計算する（以前はジョイントの数だけ毎回計算していた）
		const XMMATRIX node_global_inverse{ XMMatrixInverse(NULL, XMLoadFloat4x4(&node.global_transform)) };
		for (size_t joint_index = 0; joint_index < skin.joints.size(); ++joint_index)
		{
			XMStoreFloat4x4(&primitive_joint_data.matrices[joint_index],
				XMLoadFloat4x4(&skin.inverse_bind_matrices.at(joint_index)) *
				XMLoadFloat4x4(&nodes.at(skin.joints.at(joint_index)).global_transform) *
				node_global_inverse
			);
		}
		immediate_context->UpdateSubresource(primitive_joint_cbuffer.Get(), 0, 0, &primitive_joint_data, 0, 0);
		immediate_context->VSSetConstantBuffers(3, 1, primitive_joint_cbuffer.GetAddressOf());
	}
	  if (node.mesh > -1)
	  {
		const mesh& mesh{ meshes.at(node.mesh) };
		for (std::vector<mesh::primitive>::const_reference primitive : mesh.primitives)
		{

  const material& material{ materials.at(primitive.material) };
  if (!is_material_in_pass(material.data.alpha_mode, pass))
  {
	  continue;
  }
  const int texture_indices[]
  {
	material.data.pbr_metallic_roughness.basecolor_texture.index,
	material.data.pbr_metallic_roughness.metallic_roughness_texture.index,
	material.data.normal_texture.index,
	material.data.emissive_texture.index,
	material.data.occlusion_texture.index,
  };
  ID3D11ShaderResourceView* null_shader_resource_view{};
  // texture_indices は要素数固定（5個）なので、プリミティブ描画のたびに
  // std::vector をヒープ確保する必要はない。固定長配列にして確保コストを消す。
  ID3D11ShaderResourceView* shader_resource_views[_countof(texture_indices)]{};
  for (int texture_index = 0; texture_index < static_cast<int>(_countof(texture_indices)); ++texture_index)
  {
	shader_resource_views[texture_index] = texture_indices[texture_index] > -1 ?
	  texture_resource_views.at(textures.at(texture_indices[texture_index]).source).Get() :
	  null_shader_resource_view;
  }
  immediate_context->PSSetShaderResources(1, static_cast<UINT>(_countof(shader_resource_views)), shader_resource_views);





		  ID3D11Buffer* vertex_buffers[]{
			primitive.vertex_buffer_views.at("POSITION").buffer.Get(),
			primitive.vertex_buffer_views.at("NORMAL").buffer.Get(),
			primitive.vertex_buffer_views.at("TANGENT").buffer.Get(),
			primitive.vertex_buffer_views.at("TEXCOORD_0").buffer.Get(),
			primitive.vertex_buffer_views.at("JOINTS_0").buffer.Get(),
			primitive.vertex_buffer_views.at("WEIGHTS_0").buffer.Get(),
		  };
		  UINT strides[]{
			static_cast<UINT>(primitive.vertex_buffer_views.at("POSITION").stride_in_bytes),
			static_cast<UINT>(primitive.vertex_buffer_views.at("NORMAL").stride_in_bytes),
			static_cast<UINT>(primitive.vertex_buffer_views.at("TANGENT").stride_in_bytes),
			static_cast<UINT>(primitive.vertex_buffer_views.at("TEXCOORD_0").stride_in_bytes),
			static_cast<UINT>(primitive.vertex_buffer_views.at("JOINTS_0").stride_in_bytes),
			static_cast<UINT>(primitive.vertex_buffer_views.at("WEIGHTS_0").stride_in_bytes),
		  };
		  UINT offsets[_countof(vertex_buffers)]{ 0 };
		  immediate_context->IASetVertexBuffers(0, _countof(vertex_buffers), vertex_buffers, strides, offsets);
		  immediate_context->IASetIndexBuffer(primitive.index_buffer_view.buffer.Get(),
			primitive.index_buffer_view.format, 0);

		  primitive_constants primitive_data{};
		  primitive_data.material = primitive.material;
		  primitive_data.has_tangent = primitive.vertex_buffer_views.at("TANGENT").buffer != NULL;
		  primitive_data.skin = node.skin;
		  XMStoreFloat4x4(&primitive_data.world,
			XMLoadFloat4x4(&node.global_transform) * XMLoadFloat4x4(&world));
		  immediate_context->UpdateSubresource(primitive_cbuffer.Get(), 0, 0, &primitive_data, 0, 0);
		  immediate_context->VSSetConstantBuffers(0, 1, primitive_cbuffer.GetAddressOf());
		  immediate_context->PSSetConstantBuffers(0, 1, primitive_cbuffer.GetAddressOf());
		  //primitveの情報をすべてログで出す
		  //log_printf("Rendering primitive with material index: %d, has_tangent: %d, skin index: %d\n",
			 // LogLevel::Info, primitive.material, primitive_data.has_tangent, primitive_data.skin);

		  immediate_context->DrawIndexed(static_cast<UINT>(primitive.index_buffer_view.count()), 0, 0);


		}
	  }
	  for (std::vector<int>::value_type child_index : node.children)
	  {
		traverse(child_index);
	  }
	} };

	for (std::vector<int>::value_type node_index : scenes.at(0).nodes)
	{
		traverse(node_index);
	}

}

// =============================================================================
// render_with_nodes()
// update() \u3067\u4e8b\u524d\u8a08\u7b97\u3057\u305f animated_nodes \u3092\u4f7f\u3044\u3001\u30a2\u30cb\u30e1\u30fc\u30b7\u30e7\u30f3\u518d\u8a08\u7b97\u3092\u30b9\u30ad\u30c3\u30d7\u3059\u308b\u9ad8\u901f\u7248\u3002
// =============================================================================
void Gltf_Model::render_with_nodes(
	ID3D11DeviceContext* immediate_context,
	const DirectX::XMFLOAT4X4& world,
	pass_mode pass,
	const std::vector<node>* precomputed_nodes,
	const std::unordered_map<int, primitive_joint_constants>* precomputed_joint_matrices)
{
	// precomputed_nodes \u304c null \u306a\u3089\u901a\u5e38\u7248\u306b\u30d5\u30a9\u30fc\u30eb\u30d0\u30c3\u30af
	if (!precomputed_nodes || precomputed_nodes->empty())
	{
		render(immediate_context, world, pass, false, 0.0f);
		return;
	}

	// --- \u30b7\u30a7\u30fc\u30c0\u30fc\u30bb\u30c3\u30c8 ---
	if (pass == pass_mode::deferred_geometry)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("GLTF_DEFERRED_GEOMETRY_PS"), NULL, 0);
	}
	else if (pass == pass_mode::forward_opaque)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("GLTF_FORWARD_OPAQUE_PS"), NULL, 0);
	}
	else if (pass == pass_mode::forward_transparency)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("GLTF_FORWARD_TRANSPARENCY_PS"), NULL, 0);
	}
	else if (pass == pass_mode::directional_shadow)
	{
		immediate_context->PSSetShader(nullptr, nullptr, 0);
	}
	else if (pass == pass_mode::shadow)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().shader_manager.GetNative<Pixel_Shader>("POINT_SHADOW_PS"), NULL, 0);
	}
	else
	{
		_ASSERT_EXPR(FALSE, L"Unknown pass_mode");
	}

	using namespace DirectX;

	const char* vertex_shader_name;
	switch (pass)
	{
	case pass_mode::directional_shadow:
		vertex_shader_name = "DIRECTIONAL_SHADOW_VS";
		break;
	case pass_mode::shadow:
		vertex_shader_name = "POINT_SHADOW_VS";
		break;
	default:
		vertex_shader_name = "GLTF_VS";
		break;
	}
	immediate_context->VSSetShader(Resource_Manager::instance().shader_manager.GetNative<Vertex_Shader>(vertex_shader_name), nullptr, 0);
	immediate_context->PSSetShaderResources(0, 1, material_resource_view.GetAddressOf());
	immediate_context->IASetInputLayout(Resource_Manager::instance().shader_manager.Get<Vertex_Shader>(vertex_shader_name)->get_input_layout());
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const std::vector<node>& nodes_ref = *precomputed_nodes;

	std::function<void(int)> traverse{ [&](int node_index)->void {
	  const node& node{nodes_ref.at(node_index)};
	  if (node.skin > -1)
	  {
		  // ModelManager::update() で1フレームに1回だけ計算済みのジョイント行列があれば、
		  // 描画パスごとに再計算せずそれをそのまま使う（deferred/shadow x2/directional/forward の
		  // 5パス分の重複計算とXMMatrixInverseの重複呼び出しを回避する）
		  const primitive_joint_constants* cached_data = nullptr;
		  if (precomputed_joint_matrices)
		  {
			  auto cached_it = precomputed_joint_matrices->find(node_index);
			  if (cached_it != precomputed_joint_matrices->end())
			  {
				  cached_data = &cached_it->second;
			  }
		  }

		  if (cached_data)
		  {
			  immediate_context->UpdateSubresource(primitive_joint_cbuffer.Get(), 0, 0, cached_data, 0, 0);
			  immediate_context->VSSetConstantBuffers(3, 1, primitive_joint_cbuffer.GetAddressOf());
		  }
		  else
		  {
			  // キャッシュが無い場合のフォールバック（従来通りその場で計算）
			  const skin& skin{ skins.at(node.skin) };
			  primitive_joint_constants primitive_joint_data{};
			  const XMMATRIX node_global_inverse{ XMMatrixInverse(NULL, XMLoadFloat4x4(&node.global_transform)) };
			  for (size_t joint_index = 0; joint_index < skin.joints.size(); ++joint_index)
			  {
				  XMStoreFloat4x4(&primitive_joint_data.matrices[joint_index],
					  XMLoadFloat4x4(&skin.inverse_bind_matrices.at(joint_index)) *
					  XMLoadFloat4x4(&nodes_ref.at(skin.joints.at(joint_index)).global_transform) *
					  node_global_inverse
				  );
			  }
			  immediate_context->UpdateSubresource(primitive_joint_cbuffer.Get(), 0, 0, &primitive_joint_data, 0, 0);
			  immediate_context->VSSetConstantBuffers(3, 1, primitive_joint_cbuffer.GetAddressOf());
		  }
		}
		if (node.mesh > -1)
		{
		  const mesh& mesh{ meshes.at(node.mesh) };
		  for (std::vector<mesh::primitive>::const_reference primitive : mesh.primitives)
		  {
			const material& material{ materials.at(primitive.material) };
			if (!is_material_in_pass(material.data.alpha_mode, pass))
			{
				continue;
			}
			const int texture_indices[]
			{
			  material.data.pbr_metallic_roughness.basecolor_texture.index,
			  material.data.pbr_metallic_roughness.metallic_roughness_texture.index,
			  material.data.normal_texture.index,
			  material.data.emissive_texture.index,
			  material.data.occlusion_texture.index,
			};
			ID3D11ShaderResourceView* null_shader_resource_view{};
			ID3D11ShaderResourceView* shader_resource_views[_countof(texture_indices)]{};
			for (int texture_index = 0; texture_index < static_cast<int>(_countof(texture_indices)); ++texture_index)
			{
			  shader_resource_views[texture_index] = texture_indices[texture_index] > -1 ?
				texture_resource_views.at(textures.at(texture_indices[texture_index]).source).Get() :
				null_shader_resource_view;
			}
			immediate_context->PSSetShaderResources(1, static_cast<UINT>(_countof(shader_resource_views)), shader_resource_views);

			ID3D11Buffer* vertex_buffers[]{
			  primitive.vertex_buffer_views.at("POSITION").buffer.Get(),
			  primitive.vertex_buffer_views.at("NORMAL").buffer.Get(),
			  primitive.vertex_buffer_views.at("TANGENT").buffer.Get(),
			  primitive.vertex_buffer_views.at("TEXCOORD_0").buffer.Get(),
			  primitive.vertex_buffer_views.at("JOINTS_0").buffer.Get(),
			  primitive.vertex_buffer_views.at("WEIGHTS_0").buffer.Get(),
			};
			UINT strides[]{
			  static_cast<UINT>(primitive.vertex_buffer_views.at("POSITION").stride_in_bytes),
			  static_cast<UINT>(primitive.vertex_buffer_views.at("NORMAL").stride_in_bytes),
			  static_cast<UINT>(primitive.vertex_buffer_views.at("TANGENT").stride_in_bytes),
			  static_cast<UINT>(primitive.vertex_buffer_views.at("TEXCOORD_0").stride_in_bytes),
			  static_cast<UINT>(primitive.vertex_buffer_views.at("JOINTS_0").stride_in_bytes),
			  static_cast<UINT>(primitive.vertex_buffer_views.at("WEIGHTS_0").stride_in_bytes),
			};
			UINT offsets[_countof(vertex_buffers)]{ 0 };
			immediate_context->IASetVertexBuffers(0, _countof(vertex_buffers), vertex_buffers, strides, offsets);
			immediate_context->IASetIndexBuffer(primitive.index_buffer_view.buffer.Get(),
			  primitive.index_buffer_view.format, 0);

			primitive_constants primitive_data{};
			primitive_data.material = primitive.material;
			primitive_data.has_tangent = primitive.vertex_buffer_views.at("TANGENT").buffer != NULL;
			primitive_data.skin = node.skin;
			XMStoreFloat4x4(&primitive_data.world,
			  XMLoadFloat4x4(&node.global_transform) * XMLoadFloat4x4(&world));
			immediate_context->UpdateSubresource(primitive_cbuffer.Get(), 0, 0, &primitive_data, 0, 0);
			immediate_context->VSSetConstantBuffers(0, 1, primitive_cbuffer.GetAddressOf());
			immediate_context->PSSetConstantBuffers(0, 1, primitive_cbuffer.GetAddressOf());

			immediate_context->DrawIndexed(static_cast<UINT>(primitive.index_buffer_view.count()), 0, 0);
		  }
		}
		for (std::vector<int>::value_type child_index : node.children)
		{
		  traverse(child_index);
		}
	  } };

	for (std::vector<int>::value_type node_index : scenes.at(0).nodes)
	{
		traverse(node_index);
	}
}


void Gltf_Model::animate(size_t animation_index, float time, std::vector<node>& animated_nodes)
{
	using namespace std;
	using namespace DirectX;

	function<size_t(const vector<float>&, float, float&)> indexof{
	  [](const vector<float>& timelines, float time, float& interpolation_factor)->size_t {
	  const size_t keyframe_count{ timelines.size() };
	  if (time > timelines.at(keyframe_count - 1))
	  {
		  interpolation_factor = 1.0f;
		  return keyframe_count - 2;
	  }
	  else if (time < timelines.at(0))
	  {
		interpolation_factor = 0.0f;
		return 0;
	  }
	  size_t keyframe_index{ 0 };
	  for (size_t time_index = 1; time_index < keyframe_count; ++time_index)
	  {
		if (time < timelines.at(time_index))
		{
		  keyframe_index = max<size_t>(0LL, time_index - 1);
		  break;
		}
	  }
	  interpolation_factor = (time - timelines.at(keyframe_index + 0)) /
		(timelines.at(keyframe_index + 1) - timelines.at(keyframe_index + 0));
	  return keyframe_index;
	} };

	if (animations.size() > 0)
	{
		const animation& animation{ animations.at(animation_index) };
		for (vector<animation::channel>::const_reference channel : animation.channels)
		{
			const animation::sampler& sampler{ animation.samplers.at(channel.sampler) };
			const vector<float>& timeline{ animation.timelines.at(sampler.input) };
			if (timeline.size() == 0)
			{
				continue;
			}
			float interpolation_factor{};
			size_t keyframe_index{ indexof(timeline, time, interpolation_factor) };
			if (channel.target_path == "scale")
			{
				const vector<XMFLOAT3>& scales{ animation.scales.at(sampler.output) };
				XMStoreFloat3(&animated_nodes.at(channel.target_node).scale,
					XMVectorLerp(XMLoadFloat3(&scales.at(keyframe_index + 0)),
						XMLoadFloat3(&scales.at(keyframe_index + 1)), interpolation_factor));
			}
			else if (channel.target_path == "rotation")
			{
				const vector<XMFLOAT4>& rotations{ animation.rotations.at(sampler.output) };
				XMStoreFloat4(&animated_nodes.at(channel.target_node).rotation,
					XMQuaternionNormalize(XMQuaternionSlerp(XMLoadFloat4(&rotations.at(keyframe_index + 0)),
						XMLoadFloat4(&rotations.at(keyframe_index + 1)), interpolation_factor)));
			}
			else if (channel.target_path == "translation")
			{
				const vector<XMFLOAT3>& translations{ animation.translations.at(sampler.output) };
				XMStoreFloat3(&animated_nodes.at(channel.target_node).translation,
					XMVectorLerp(XMLoadFloat3(&translations.at(keyframe_index + 0)),
						XMLoadFloat3(&translations.at(keyframe_index + 1)), interpolation_factor));
			}
		}
		cumulate_transforms(animated_nodes);
	}
}
void Gltf_Model::compute_joint_matrices(
	const std::vector<node>& animated_nodes,
	std::unordered_map<int, primitive_joint_constants>& out_joint_matrices) const
{
	using namespace DirectX;

	out_joint_matrices.clear();

	for (size_t node_index = 0; node_index < animated_nodes.size(); ++node_index)
	{
		const node& node{ animated_nodes[node_index] };
		if (node.skin < 0) continue;

		const skin& skin{ skins.at(node.skin) };
		primitive_joint_constants data{};

		// ノードのグローバル変換の逆行列はジョイント数に関係なく同じ値なので、
		// ループの外で1回だけ計算する
		const XMMATRIX node_global_inverse{ XMMatrixInverse(NULL, XMLoadFloat4x4(&node.global_transform)) };

		for (size_t joint_index = 0; joint_index < skin.joints.size(); ++joint_index)
		{
			XMStoreFloat4x4(&data.matrices[joint_index],
				XMLoadFloat4x4(&skin.inverse_bind_matrices.at(joint_index)) *
				XMLoadFloat4x4(&animated_nodes.at(skin.joints.at(joint_index)).global_transform) *
				node_global_inverse
			);
		}

		out_joint_matrices.emplace(static_cast<int>(node_index), data);
	}
}

void Gltf_Model::animate_all(float time, std::vector<node>& animated_nodes)
{
	using namespace std;
	using namespace DirectX;

	// keyframe検索ラムダ（animateと同じ）
	function<size_t(const vector<float>&, float, float&)> indexof{
	[](const vector<float>& timelines, float time, float& interpolation_factor)->size_t
	{
		const size_t keyframe_count{ timelines.size() };

		// ← 追加：キーフレームが1つ以下なら補間不可
		if (keyframe_count <= 1)
		{
			interpolation_factor = 0.0f;
			return 0;
		}

		if (time > timelines.at(keyframe_count - 1))
		{
			interpolation_factor = 1.0f;
			return keyframe_count - 2; // 最後の区間
		}
		else if (time < timelines.at(0))
		{
			interpolation_factor = 0.0f;
			return 0;
		}
		size_t keyframe_index{ 0 };
		for (size_t time_index = 1; time_index < keyframe_count; ++time_index)
		{
			if (time < timelines.at(time_index))
			{
				keyframe_index = max<size_t>(0LL, time_index - 1);
				break;
			}
		}
		interpolation_factor =
			(time - timelines.at(keyframe_index)) /
			(timelines.at(keyframe_index + 1) - timelines.at(keyframe_index));
		return keyframe_index;
	}
	};

	// 全アニメーションのチャンネルを順番に適用
	for (const animation& animation : animations)
	{
		// このアニメーションのtimeをdurationでラップ
		const float local_time = (animation.duration > 0.0f)
			? fmodf(time, animation.duration)
			: 0.0f;

		for (const animation::channel& channel : animation.channels)
		{
			const animation::sampler& sampler{ animation.samplers.at(channel.sampler) };
			const vector<float>& timeline{ animation.timelines.at(sampler.input) };

			if (timeline.size() == 0) continue;

			float interpolation_factor{};
			size_t keyframe_index{ indexof(timeline, local_time, interpolation_factor) };

			if (channel.target_path == "scale")
			{
				const vector<XMFLOAT3>& scales{ animation.scales.at(sampler.output) };
				if (scales.size() < 2 || keyframe_index + 1 >= scales.size()) continue; // ← 追加
				XMStoreFloat3(
					&animated_nodes.at(channel.target_node).scale,
					XMVectorLerp(
						XMLoadFloat3(&scales.at(keyframe_index)),
						XMLoadFloat3(&scales.at(keyframe_index + 1)),
						interpolation_factor));
			}
			else if (channel.target_path == "rotation")
			{
				const vector<XMFLOAT4>& rotations{ animation.rotations.at(sampler.output) };
				if (rotations.size() < 2 || keyframe_index + 1 >= rotations.size()) continue; // ← 追加
				XMStoreFloat4(
					&animated_nodes.at(channel.target_node).rotation,
					XMQuaternionNormalize(XMQuaternionSlerp(
						XMLoadFloat4(&rotations.at(keyframe_index)),
						XMLoadFloat4(&rotations.at(keyframe_index + 1)),
						interpolation_factor)));
			}
			else if (channel.target_path == "translation")
			{
				const vector<XMFLOAT3>& translations{ animation.translations.at(sampler.output) };
				if (translations.size() < 2 || keyframe_index + 1 >= translations.size()) continue; // ← 追加
				XMStoreFloat3(
					&animated_nodes.at(channel.target_node).translation,
					XMVectorLerp(
						XMLoadFloat3(&translations.at(keyframe_index)),
						XMLoadFloat3(&translations.at(keyframe_index + 1)),
						interpolation_factor));
			}
		}
	}

	cumulate_transforms(animated_nodes);
}

void Gltf_Model::extract_collision_triangles(
	const DirectX::XMFLOAT4X4& world_matrix,
	std::vector<DirectX::XMFLOAT3>& out_vertices) const
{
	using namespace DirectX;
	XMMATRIX world = XMLoadFloat4x4(&world_matrix);

	// ノードインデックス → グローバル変換のマップを構築
	// nodes[i].global_transform はすでに cumulate_transforms で計算済み
	for (size_t node_idx = 0; node_idx < nodes.size(); ++node_idx)
	{
		const auto& nd = nodes[node_idx];
		if (nd.mesh < 0 || nd.mesh >= static_cast<int>(meshes.size()))
			continue;

		// ノードローカル→ワールド変換
		XMMATRIX node_world = XMLoadFloat4x4(&nd.global_transform) * world;

		const auto& msh = meshes[nd.mesh];
		for (const auto& prim : msh.primitives)
		{
			const auto& pos = prim.cpu_positions;
			const auto& idx = prim.cpu_indices;
			if (pos.empty() || idx.size() < 3) continue;

			// 3インデックスで1三角形
			for (size_t i = 0; i + 2 < idx.size(); i += 3)
			{
				uint32_t i0 = idx[i + 0];
				uint32_t i1 = idx[i + 1];
				uint32_t i2 = idx[i + 2];
				if (i0 >= pos.size() || i1 >= pos.size() || i2 >= pos.size())
					continue;

				// モデルローカル座標をワールドへ変換
				XMVECTOR v0 = XMVector3TransformCoord(XMLoadFloat3(&pos[i0]), node_world);
				XMVECTOR v1 = XMVector3TransformCoord(XMLoadFloat3(&pos[i1]), node_world);
				XMVECTOR v2 = XMVector3TransformCoord(XMLoadFloat3(&pos[i2]), node_world);

				XMFLOAT3 w0, w1, w2;
				XMStoreFloat3(&w0, v0);
				XMStoreFloat3(&w1, v1);
				XMStoreFloat3(&w2, v2);

				out_vertices.push_back(w0);
				out_vertices.push_back(w1);
				out_vertices.push_back(w2);
			}
		}
	}
}