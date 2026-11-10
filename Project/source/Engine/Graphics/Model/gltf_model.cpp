#include "gltf_model.h"

#include <cstring>
#include <cmath>
#include <stack>
#include <algorithm>
#include <unordered_set>
#include "Engine/Utilities/misc.h"
#include "Engine/Utilities/math_util.h"
#include "Engine/System/graphics_core.h"
#include "Engine/System/render_state.h"
#include <WICTextureLoader.h>
#include "Engine/System/Manager/resource_manager.h"
#define TINYGLTF_IMPLEMENTATION

namespace
{
	nlohmann::json pbr_json(const tinygltf::Value& v)
	{
		using J=nlohmann::json;
		if(v.IsObject()){J j=J::object();for(const auto& kv:v.Get<tinygltf::Value::Object>())j[kv.first]=pbr_json(kv.second);return j;}
		if(v.IsArray()){J j=J::array();for(const auto& x:v.Get<tinygltf::Value::Array>())j.push_back(pbr_json(x));return j;}
		if(v.IsNumber())return v.GetNumberAsDouble();if(v.IsBool())return v.Get<bool>();if(v.IsString())return v.Get<std::string>();return nullptr;
	}
	template<class T> nlohmann::json pbr_texture_json(const T& t)
	{
		nlohmann::json j={{"index",t.index},{"texCoord",t.texCoord}};j["extensions"]=nlohmann::json::object();
		for(const auto& kv:t.extensions)j["extensions"][kv.first]=pbr_json(kv.second);return j;
	}
	GltfPbr::Material pbr_material(const tinygltf::Material& m)
	{
		using J=nlohmann::json;const auto& p=m.pbrMetallicRoughness;
		J j={{"alphaMode",m.alphaMode},{"alphaCutoff",m.alphaCutoff},{"doubleSided",m.doubleSided},{"emissiveFactor",m.emissiveFactor},
			{"pbrMetallicRoughness",{{"baseColorFactor",p.baseColorFactor},{"metallicFactor",p.metallicFactor},{"roughnessFactor",p.roughnessFactor},{"baseColorTexture",pbr_texture_json(p.baseColorTexture)},{"metallicRoughnessTexture",pbr_texture_json(p.metallicRoughnessTexture)}}},
			{"normalTexture",pbr_texture_json(m.normalTexture)},{"occlusionTexture",pbr_texture_json(m.occlusionTexture)},{"emissiveTexture",pbr_texture_json(m.emissiveTexture)}};
		j["normalTexture"]["scale"]=m.normalTexture.scale;j["occlusionTexture"]["strength"]=m.occlusionTexture.strength;
		j["extensions"]=J::object();for(const auto& kv:m.extensions)j["extensions"][kv.first]=pbr_json(kv.second);return GltfPbr::parse(j);
	}
	constexpr size_t meshlet_max_vertices = 64;
	constexpr size_t meshlet_max_triangles = 126;
	constexpr size_t occlusion_meshlets_per_group = 64;

	void transform_bounds(
		const DirectX::XMFLOAT3& bounds_min, const DirectX::XMFLOAT3& bounds_max,
		DirectX::FXMMATRIX transform,
		DirectX::XMVECTOR& world_min, DirectX::XMVECTOR& world_max)
	{
		using namespace DirectX;
		const XMVECTOR center = XMVectorSet(
			(bounds_min.x + bounds_max.x) * 0.5f,
			(bounds_min.y + bounds_max.y) * 0.5f,
			(bounds_min.z + bounds_max.z) * 0.5f, 1.0f);
		const float hx = (bounds_max.x - bounds_min.x) * 0.5f;
		const float hy = (bounds_max.y - bounds_min.y) * 0.5f;
		const float hz = (bounds_max.z - bounds_min.z) * 0.5f;
		XMFLOAT4X4 m;
		XMStoreFloat4x4(&m, transform);
		const XMVECTOR extent = XMVectorSet(
			std::abs(m._11) * hx + std::abs(m._21) * hy + std::abs(m._31) * hz,
			std::abs(m._12) * hx + std::abs(m._22) * hy + std::abs(m._32) * hz,
			std::abs(m._13) * hx + std::abs(m._23) * hy + std::abs(m._33) * hz, 0.0f);
		const XMVECTOR world_center = XMVector3TransformCoord(center, transform);
		world_min = XMVectorSubtract(world_center, extent);
		world_max = XMVectorAdd(world_center, extent);
	}

	// 0: outside, 1: intersects, 2: fully inside.
	uint8_t classify_frustum_bounds(const Frustum& frustum,
		DirectX::FXMVECTOR world_min, DirectX::FXMVECTOR world_max)
	{
		using namespace DirectX;
		bool fully_inside = true;
		for (const auto& plane : frustum.planes)
		{
			const XMVECTOR positive = XMVectorSelect(world_min, world_max,
				XMVectorGreater(plane.normal, XMVectorZero()));
			if (XMVectorGetX(XMVector3Dot(plane.normal, positive)) + plane.distance < 0.0f)
				return 0;
			const XMVECTOR negative = XMVectorSelect(world_max, world_min,
				XMVectorGreater(plane.normal, XMVectorZero()));
			if (XMVectorGetX(XMVector3Dot(plane.normal, negative)) + plane.distance < 0.0f)
				fully_inside = false;
		}
		return fully_inside ? 2 : 1;
	}

	bool meshlet_intersects_frustum(const Frustum& frustum,
		const DirectX::XMFLOAT3& bounds_min, const DirectX::XMFLOAT3& bounds_max,
		DirectX::FXMMATRIX transform)
	{
		DirectX::XMVECTOR world_min, world_max;
		transform_bounds(bounds_min, bounds_max, transform, world_min, world_max);
		return classify_frustum_bounds(frustum, world_min, world_max) != 0;
	}

	uint32_t morton_key(uint32_t x, uint32_t y, uint32_t z)
	{
		uint32_t key = 0;
		for (uint32_t bit = 0; bit < 10; ++bit)
		{
			key |= ((x >> bit) & 1u) << (bit * 3);
			key |= ((y >> bit) & 1u) << (bit * 3 + 1);
			key |= ((z >> bit) & 1u) << (bit * 3 + 2);
		}
		return key;
	}

	bool spatially_order_triangles(std::vector<uint32_t>& indices,
		const std::vector<DirectX::XMFLOAT3>& positions)
	{
		if (indices.size() < 12288 || indices.size() % 3 != 0 || positions.empty()) return false;
		std::vector<DirectX::XMFLOAT3> centers;
		centers.reserve(indices.size() / 3);
		DirectX::XMFLOAT3 minimum{ FLT_MAX, FLT_MAX, FLT_MAX };
		DirectX::XMFLOAT3 maximum{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (size_t first = 0; first < indices.size(); first += 3)
		{
			if (indices[first] >= positions.size() || indices[first + 1] >= positions.size() ||
				indices[first + 2] >= positions.size()) return false;
			const auto& a = positions[indices[first]];
			const auto& b = positions[indices[first + 1]];
			const auto& c = positions[indices[first + 2]];
			const DirectX::XMFLOAT3 center{
				(a.x + b.x + c.x) / 3.0f,
				(a.y + b.y + c.y) / 3.0f,
				(a.z + b.z + c.z) / 3.0f };
			if (!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)) return false;
			centers.push_back(center);
			minimum.x = std::min(minimum.x, center.x);
			minimum.y = std::min(minimum.y, center.y);
			minimum.z = std::min(minimum.z, center.z);
			maximum.x = std::max(maximum.x, center.x);
			maximum.y = std::max(maximum.y, center.y);
			maximum.z = std::max(maximum.z, center.z);
		}
		struct triangle_key { uint32_t morton; uint32_t triangle; };
		std::vector<triangle_key> order;
		order.reserve(centers.size());
		auto quantize = [](float value, float min_value, float max_value) -> uint32_t
		{
			if (max_value <= min_value) return 0;
			return static_cast<uint32_t>(std::clamp((value - min_value) / (max_value - min_value),
				0.0f, 1.0f) * 1023.0f);
		};
		for (uint32_t triangle = 0; triangle < centers.size(); ++triangle)
		{
			const auto& p = centers[triangle];
			order.push_back({ morton_key(
				quantize(p.x, minimum.x, maximum.x),
				quantize(p.y, minimum.y, maximum.y),
				quantize(p.z, minimum.z, maximum.z)), triangle });
		}
		std::sort(order.begin(), order.end(), [](const triangle_key& a, const triangle_key& b)
		{
			return a.morton < b.morton || (a.morton == b.morton && a.triangle < b.triangle);
		});
		std::vector<uint32_t> sorted(indices.size());
		for (size_t triangle = 0; triangle < order.size(); ++triangle)
		{
			const size_t source = static_cast<size_t>(order[triangle].triangle) * 3;
			const size_t destination = triangle * 3;
			sorted[destination] = indices[source];
			sorted[destination + 1] = indices[source + 1];
			sorted[destination + 2] = indices[source + 2];
		}
		indices.swap(sorted);
		return true;
	}
}

bool null_load_image_data(tinygltf::Image* image, const int, std::string*, std::string*,
	int, int, const unsigned char* bytes, int count, void*)
{
	image->image.assign(bytes,bytes+count);image->as_is=true;
	return true;
}

void Gltf_Model::compute_instance_aabb(
	const DirectX::XMFLOAT4X4& world,
	bool is_animation,
	float animation_time,
	animation_mode anim_mode,
	int animation_index,
	DirectX::XMFLOAT3& out_min,
	DirectX::XMFLOAT3& out_max,
	const char* bounds_root_name) const
{
	using namespace DirectX;

	if (!has_bbox) {
		out_min = XMFLOAT3(-1, -1, -1);
		out_max = XMFLOAT3(1, 1, 1);
		return;
	}

	const XMMATRIX world_m = XMLoadFloat4x4(&world);
	XMVECTOR wmin = XMVectorReplicate(FLT_MAX);
	XMVECTOR wmax = XMVectorReplicate(-FLT_MAX);
	bool has_geometry = false;
	std::vector<size_t> node_indices;
	if (bounds_root_name)
	{
		for (size_t i = 0; i < nodes.size(); ++i)
			if (nodes[i].name == bounds_root_name) { node_indices.push_back(i); break; }
		for (size_t i = 0; i < node_indices.size(); ++i)
			for (int child : nodes[node_indices[i]].children)
				if (child >= 0 && static_cast<size_t>(child) < nodes.size())
					node_indices.push_back(static_cast<size_t>(child));
	}
	if (node_indices.empty())
		for (size_t i = 0; i < nodes.size(); ++i) node_indices.push_back(i);
	for (size_t node_index : node_indices)
	{
		const auto& node = nodes[node_index];
		if (node.mesh < 0 || static_cast<size_t>(node.mesh) >= meshes.size()) continue;
		const XMMATRIX transform = XMLoadFloat4x4(&node.global_transform) * world_m;
		for (const auto& primitive : meshes[node.mesh].primitives)
		{
			XMFLOAT3 local_min = primitive.meshlet_bounds_min;
			XMFLOAT3 local_max = primitive.meshlet_bounds_max;
			if (primitive.meshlets.empty() || local_min.x > local_max.x)
			{
				if (primitive.cpu_positions.empty()) continue;
				local_min = { FLT_MAX, FLT_MAX, FLT_MAX };
				local_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
				for (const auto& position : primitive.cpu_positions)
				{
					local_min.x = (std::min)(local_min.x, position.x);
					local_min.y = (std::min)(local_min.y, position.y);
					local_min.z = (std::min)(local_min.z, position.z);
					local_max.x = (std::max)(local_max.x, position.x);
					local_max.y = (std::max)(local_max.y, position.y);
					local_max.z = (std::max)(local_max.z, position.z);
				}
			}
			XMVECTOR primitive_min, primitive_max;
			transform_bounds(local_min, local_max, transform, primitive_min, primitive_max);
			wmin = XMVectorMin(wmin, primitive_min);
			wmax = XMVectorMax(wmax, primitive_max);
			has_geometry = true;
		}
	}
	if (!has_geometry)
		transform_bounds(bbox_min, bbox_max, world_m, wmin, wmax);

	XMStoreFloat3(&out_min, wmin);
	XMStoreFloat3(&out_max, wmax);
}

void Gltf_Model::collect_emissive_surface_samples(
	const DirectX::XMFLOAT4X4& world, float voxel_size,
	std::vector<emissive_sample>& out_samples) const
{
	using namespace DirectX;
	const XMMATRIX instance_world = XMLoadFloat4x4(&world);
	for (const auto& node : nodes)
	{
		if (node.mesh < 0 || static_cast<size_t>(node.mesh) >= meshes.size()) continue;
		// The same bulb is exported at several LODs. Only the visible LOD0 is needed.
		const size_t lod = node.name.find("_LOD");
		if (lod != std::string::npos && node.name.compare(lod, 5, "_LOD0") != 0) continue;
		const XMMATRIX transform = XMLoadFloat4x4(&node.global_transform) * instance_world;
		for (const auto& primitive : meshes[node.mesh].primitives)
		{
			const auto& emissive = materials[primitive.material].data.emissive_factor;
			if ((std::max)({ emissive[0], emissive[1], emissive[2] }) <= 0.0f ||
				primitive.cpu_indices.empty() || primitive.cpu_positions.empty()) continue;
			const bool strip = materials[primitive.material].name.rfind("EmissiveStripLight_", 0) == 0;
			for (size_t i = 0; i + 2 < primitive.cpu_indices.size(); i += 3)
			{
				const uint32_t ia = primitive.cpu_indices[i];
				const uint32_t ib = primitive.cpu_indices[i + 1];
				const uint32_t ic = primitive.cpu_indices[i + 2];
				if (ia >= primitive.cpu_positions.size() || ib >= primitive.cpu_positions.size() ||
					ic >= primitive.cpu_positions.size()) continue;
				const XMVECTOR a = XMVector3TransformCoord(XMLoadFloat3(&primitive.cpu_positions[ia]), transform);
				const XMVECTOR b = XMVector3TransformCoord(XMLoadFloat3(&primitive.cpu_positions[ib]), transform);
				const XMVECTOR c = XMVector3TransformCoord(XMLoadFloat3(&primitive.cpu_positions[ic]), transform);
				const float ab = XMVectorGetX(XMVector3Length(b - a));
				const float ac = XMVectorGetX(XMVector3Length(c - a));
				const float bc = XMVectorGetX(XMVector3Length(c - b));
				const int steps = strip ? (std::clamp)(
					static_cast<int>(std::ceil((std::max)({ ab, ac, bc }) /
						(std::max)(voxel_size * 0.5f, 0.01f))), 1, 32) : 1;
				for (int u = 0; u <= steps; ++u)
					for (int v = 0; v <= steps - u; ++v)
					{
						const float fu = static_cast<float>(u) / steps;
						const float fv = static_cast<float>(v) / steps;
						emissive_sample sample{};
						XMStoreFloat3(&sample.position, a + (b - a) * fu + (c - a) * fv);
						sample.radiance = { emissive[0], emissive[1], emissive[2] };
						out_samples.push_back(sample);
					}
			}
		}
	}
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
	fetch_textures(Graphics_Core::instance().get_device(), Gltf_Model);
	fetch_materials(Graphics_Core::instance().get_device(), Gltf_Model); // textures の後に実行
	fetch_meshes(Graphics_Core::instance().get_device(), Gltf_Model);
	for (auto& node : nodes)
	{
		if (node.mesh < 0) continue;
		node.occlusion_slot_base = total_occlusion_slots_;
		total_occlusion_slots_ += meshes[node.mesh].occlusion_group_count;
	}
	fetch_animations(Gltf_Model);

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
	  { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 6, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	  { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 7, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	Resource_Manager::instance().get_shader<Vertex_Shader>("GLTF_VS")->create_input_layout(
		input_element_desc, _countof(input_element_desc));
	Resource_Manager::instance().get_shader<Vertex_Shader>("POINT_SHADOW_VS")->create_input_layout(
		input_element_desc, _countof(input_element_desc));
	Resource_Manager::instance().get_shader<Vertex_Shader>("DIRECTIONAL_SHADOW_VS")->create_input_layout(
		input_element_desc, _countof(input_element_desc));

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(primitive_constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	HRESULT hr = device->CreateBuffer(&buffer_desc, nullptr, primitive_cbuffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	buffer_desc.ByteWidth = sizeof(primitive_joint_constants);
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&buffer_desc, NULL, primitive_joint_cbuffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

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
		mesh.occlusion_slot_base = occlusion_group_count_;

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
				const size_t stride = pos_bv.byteStride ? pos_bv.byteStride : sizeof(float) * 3;
				for (size_t i = 0; i < count; ++i)
				{
					float position[3];
					std::memcpy(position, buffer.data.data() + byteOffset + i * stride, sizeof(position));
					const float x = position[0];
					const float y = position[1];
					const float z = position[2];
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
			if (primitive.material < 0 ||
				static_cast<size_t>(primitive.material) >= materials.size() - 1)
			{
				// The final entry is the explicit glTF default material added by
				// fetch_materials(). In particular, -1 means the asset omitted it.
				primitive.material = static_cast<int>(materials.size()) - 1;
			}

			const tinygltf::Accessor& gltf_accessor{ Gltf_Model.accessors.at(gltf_primitive.indices) };
			const tinygltf::BufferView& gltf_buffer_view{ Gltf_Model.bufferViews.at(gltf_accessor.bufferView) };

			primitive.index_buffer_view = make_buffer_view(gltf_accessor);

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

			for (std::map<std::string, int>::const_reference gltf_attribute : gltf_primitive.attributes)
			{
				const tinygltf::Accessor& gltf_accessor{ Gltf_Model.accessors.at(gltf_attribute.second) };
				const tinygltf::BufferView& gltf_buffer_view{ Gltf_Model.bufferViews.at(gltf_accessor.bufferView) };

				buffer_view vertex_buffer_view = gltf_attribute.first=="COLOR_0"?buffer_view{}:make_buffer_view(gltf_accessor);
				std::vector<float> color_values;
				if(gltf_attribute.first=="COLOR_0")
				{
					const int components=gltf_accessor.type==TINYGLTF_TYPE_VEC3?3:4;
					const auto* raw=Gltf_Model.buffers.at(gltf_buffer_view.buffer).data.data()+gltf_buffer_view.byteOffset+gltf_accessor.byteOffset;
					const size_t bytes=gltf_accessor.componentType==TINYGLTF_COMPONENT_TYPE_FLOAT?4:gltf_accessor.componentType==TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT?2:1;
					const size_t stride=gltf_buffer_view.byteStride?gltf_buffer_view.byteStride:bytes*components;
					color_values.resize(gltf_accessor.count*4,1);
					for(size_t i=0;i<gltf_accessor.count;++i)for(int k=0;k<components;++k){float value=0;const auto* src=raw+i*stride+k*bytes;if(bytes==4)std::memcpy(&value,src,4);else if(bytes==2){uint16_t x;std::memcpy(&x,src,2);value=gltf_accessor.normalized?x/65535.0f:float(x);}else value=gltf_accessor.normalized?*src/255.0f:float(*src);color_values[i*4+k]=value;}
					vertex_buffer_view.format=DXGI_FORMAT_R32G32B32A32_FLOAT;vertex_buffer_view.stride_in_bytes=16;vertex_buffer_view.size_in_bytes=color_values.size()*sizeof(float);
				}

				D3D11_BUFFER_DESC buffer_desc{};
				buffer_desc.ByteWidth = static_cast<UINT>(vertex_buffer_view.size_in_bytes);
				buffer_desc.Usage = D3D11_USAGE_DEFAULT;
				buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
				D3D11_SUBRESOURCE_DATA subresource_data{};
				const auto* source = Gltf_Model.buffers.at(gltf_buffer_view.buffer).data.data()
					+ gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
				const size_t element_size = vertex_buffer_view.stride_in_bytes;
				const size_t source_stride = gltf_buffer_view.byteStride ? gltf_buffer_view.byteStride : element_size;
				std::vector<uint8_t> packed;
				if (source_stride != element_size && color_values.empty())
				{
					packed.resize(gltf_accessor.count * element_size);
					for (size_t i = 0; i < gltf_accessor.count; ++i)
						std::memcpy(packed.data() + i * element_size, source + i * source_stride, element_size);
					subresource_data.pSysMem = packed.data();
				}
				else subresource_data.pSysMem = source;
				if(!color_values.empty())subresource_data.pSysMem=color_values.data();
				hr = device->CreateBuffer(&buffer_desc, &subresource_data,
					vertex_buffer_view.buffer.ReleaseAndGetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

				if (gltf_attribute.first == "POSITION")
				{
					const size_t byteOffset = gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
					const tinygltf::Buffer& buffer = Gltf_Model.buffers.at(gltf_buffer_view.buffer);
					const size_t stride = gltf_buffer_view.byteStride ? gltf_buffer_view.byteStride : sizeof(float) * 3;
					primitive.cpu_positions.resize(gltf_accessor.count);
					for (size_t i = 0; i < gltf_accessor.count; ++i)
					{
						std::memcpy(&primitive.cpu_positions[i], buffer.data.data() + byteOffset + i * stride,
							sizeof(primitive.cpu_positions[i]));
					}
				}

				primitive.vertex_buffer_views.emplace(std::make_pair(gltf_attribute.first, vertex_buffer_view));
			}

			const std::unordered_map<std::string, buffer_view> attributes{
			  { "TANGENT", { DXGI_FORMAT_R32G32B32A32_FLOAT } },
			  { "TEXCOORD_0", { DXGI_FORMAT_R32G32_FLOAT } },
			  { "JOINTS_0", { DXGI_FORMAT_R16G16B16A16_UINT } },
			  { "WEIGHTS_0", { DXGI_FORMAT_R32G32B32A32_FLOAT } },
			  { "TEXCOORD_1", { DXGI_FORMAT_R32G32_FLOAT } },
			  { "COLOR_0", { DXGI_FORMAT_R32G32B32A32_FLOAT } },
			};
			for (std::unordered_map<std::string, buffer_view>::const_reference attribute : attributes)
			{
				if (primitive.vertex_buffer_views.find(attribute.first) == primitive.vertex_buffer_views.end())
				{
					primitive.vertex_buffer_views.insert(std::make_pair(attribute.first, attribute.second));
				}
			}

			// IASetVertexBuffers用キャッシュの登録
			const std::string attribute_keys[] = {
				"POSITION", "NORMAL", "TANGENT", "TEXCOORD_0", "JOINTS_0", "WEIGHTS_0", "TEXCOORD_1", "COLOR_0"
			};

			primitive.cached_vertex_buffers.clear();
			primitive.cached_strides.clear();
			primitive.cached_offsets.clear();

			for (const auto& key : attribute_keys)
			{
				const auto& view = primitive.vertex_buffer_views.at(key);
				primitive.cached_vertex_buffers.push_back(view.buffer.Get());
				primitive.cached_strides.push_back(static_cast<UINT>(view.stride_in_bytes));
				primitive.cached_offsets.push_back(0);
			}

			// 【追加】has_tangent の初期判定とキャッシュ
			primitive.has_tangent = (primitive.vertex_buffer_views.at("TANGENT").buffer != nullptr);
			primitive.has_color = (primitive.vertex_buffer_views.at("COLOR_0").buffer != nullptr);

			// Opaque triangles can be reordered without changing blending order.
			// Nearby triangle centroids then produce tighter meshlet/group bounds.
			const bool reordered = gltf_primitive.mode == TINYGLTF_MODE_TRIANGLES &&
				(gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ||
				 gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) &&
				materials[primitive.material].data.alpha_mode != 2 &&
				spatially_order_triangles(primitive.cpu_indices, primitive.cpu_positions);
			D3D11_BUFFER_DESC index_desc{};
			index_desc.ByteWidth = static_cast<UINT>(primitive.index_buffer_view.size_in_bytes);
			index_desc.Usage = D3D11_USAGE_DEFAULT;
			index_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			D3D11_SUBRESOURCE_DATA index_data{};
			index_data.pSysMem = Gltf_Model.buffers.at(gltf_buffer_view.buffer).data.data() +
				gltf_buffer_view.byteOffset + gltf_accessor.byteOffset;
			std::vector<uint16_t> reordered_16_bit_indices;
			if (reordered && gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
			{
				reordered_16_bit_indices.reserve(primitive.cpu_indices.size());
				for (uint32_t index : primitive.cpu_indices)
					reordered_16_bit_indices.push_back(static_cast<uint16_t>(index));
				index_data.pSysMem = reordered_16_bit_indices.data();
			}
			else if (reordered && gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
				index_data.pSysMem = primitive.cpu_indices.data();
			hr = device->CreateBuffer(&index_desc, &index_data,
				primitive.index_buffer_view.buffer.ReleaseAndGetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

			// Partition indexed triangles into contiguous meshlets. A malformed or
			// unsupported primitive keeps the original whole-primitive draw path.
			if (gltf_primitive.mode == TINYGLTF_MODE_TRIANGLES &&
				(gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ||
				 gltf_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) &&
				!primitive.cpu_positions.empty() && !primitive.cpu_indices.empty() &&
				primitive.cpu_indices.size() % 3 == 0)
			{
				std::unordered_set<uint32_t> vertices;
				mesh::primitive::meshlet current{};
				current.bounds_min = { FLT_MAX, FLT_MAX, FLT_MAX };
				current.bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
				bool valid = true;
				for (size_t first = 0; first < primitive.cpu_indices.size(); first += 3)
				{
					const uint32_t a = primitive.cpu_indices[first];
					const uint32_t b = primitive.cpu_indices[first + 1];
					const uint32_t c = primitive.cpu_indices[first + 2];
					if (a >= primitive.cpu_positions.size() || b >= primitive.cpu_positions.size() ||
						c >= primitive.cpu_positions.size()) { valid = false; break; }
					const size_t extra = static_cast<size_t>(vertices.count(a) == 0) +
						static_cast<size_t>(b != a && vertices.count(b) == 0) +
						static_cast<size_t>(c != a && c != b && vertices.count(c) == 0);
					if (current.index_count &&
						(current.index_count / 3 == meshlet_max_triangles ||
						vertices.size() + extra > meshlet_max_vertices))
					{
						primitive.meshlets.push_back(current);
						current = {};
						current.bounds_min = { FLT_MAX, FLT_MAX, FLT_MAX };
						current.bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
						vertices.clear();
					}
					if (!current.index_count) current.first_index = static_cast<UINT>(first);
					current.index_count += 3;
					for (uint32_t index : { a, b, c })
					{
						vertices.insert(index);
						const auto& p = primitive.cpu_positions[index];
						current.bounds_min.x = std::min(current.bounds_min.x, p.x);
						current.bounds_min.y = std::min(current.bounds_min.y, p.y);
						current.bounds_min.z = std::min(current.bounds_min.z, p.z);
						current.bounds_max.x = std::max(current.bounds_max.x, p.x);
						current.bounds_max.y = std::max(current.bounds_max.y, p.y);
						current.bounds_max.z = std::max(current.bounds_max.z, p.z);
					}
				}
				if (valid && current.index_count) primitive.meshlets.push_back(current);
				if (!valid) primitive.meshlets.clear();
				if (!primitive.meshlets.empty())
				{
					primitive.meshlet_bounds_min = { FLT_MAX, FLT_MAX, FLT_MAX };
					primitive.meshlet_bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
					for (const auto& part : primitive.meshlets)
					{
						primitive.meshlet_bounds_min.x = std::min(primitive.meshlet_bounds_min.x, part.bounds_min.x);
						primitive.meshlet_bounds_min.y = std::min(primitive.meshlet_bounds_min.y, part.bounds_min.y);
						primitive.meshlet_bounds_min.z = std::min(primitive.meshlet_bounds_min.z, part.bounds_min.z);
						primitive.meshlet_bounds_max.x = std::max(primitive.meshlet_bounds_max.x, part.bounds_max.x);
						primitive.meshlet_bounds_max.y = std::max(primitive.meshlet_bounds_max.y, part.bounds_max.y);
						primitive.meshlet_bounds_max.z = std::max(primitive.meshlet_bounds_max.z, part.bounds_max.z);
					}
				}
				if (primitive.meshlets.size() >= 8)
				{
					for (size_t i = 0; i < primitive.meshlets.size(); ++i)
					{
						if (i % occlusion_meshlets_per_group == 0)
						{
							mesh::primitive::occlusion_group group{};
							group.slot = occlusion_group_count_++;
							group.first_meshlet = static_cast<UINT>(i);
							group.bounds_min = { FLT_MAX, FLT_MAX, FLT_MAX };
							group.bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
							primitive.occlusion_groups.push_back(group);
						}
						auto& part = primitive.meshlets[i];
						part.occlusion_group = static_cast<UINT>(primitive.occlusion_groups.size() - 1);
						auto& group = primitive.occlusion_groups.back();
						++group.meshlet_count;
						group.bounds_min.x = std::min(group.bounds_min.x, part.bounds_min.x);
						group.bounds_min.y = std::min(group.bounds_min.y, part.bounds_min.y);
						group.bounds_min.z = std::min(group.bounds_min.z, part.bounds_min.z);
						group.bounds_max.x = std::max(group.bounds_max.x, part.bounds_max.x);
						group.bounds_max.y = std::max(group.bounds_max.y, part.bounds_max.y);
						group.bounds_max.z = std::max(group.bounds_max.z, part.bounds_max.z);
					}
				}
			}
		}
		mesh.occlusion_group_count = occlusion_group_count_ - mesh.occlusion_slot_base;
	}
}

void Gltf_Model::fetch_materials(ID3D11Device* device, const tinygltf::Model& Gltf_Model)
{
	for (std::vector<tinygltf::Material>::const_reference gltf_material : Gltf_Model.materials)
	{
		std::vector<material>::reference material = materials.emplace_back();

		material.name = gltf_material.name;
		material.pbr = pbr_material(gltf_material);
		std::vector<ID3D11SamplerState*> material_samplers{pbr_samplers_[0].Get()};
		for(int slot=0;slot<GltfPbr::TextureCount;++slot)
		{
			auto& t=material.pbr.textures[slot];const int index=t.info[0];
			if(index>=0&&index<static_cast<int>(Gltf_Model.textures.size())){
				const auto& texture=Gltf_Model.textures[index];const int source=texture.source;
				if(source>=0&&source<static_cast<int>(texture_resource_views.size()))material.pbr_srvs[slot]=texture_resource_views[source].Get();else t.info[0]=-1;
				// Map the samplers actually used by this material, rather than
				// clamping indices in a potentially much larger model-wide table.
				const size_t sampler_index=texture.sampler>=0&&texture.sampler<static_cast<int>(Gltf_Model.samplers.size())?texture.sampler+1:0;
				auto* sampler=pbr_samplers_[sampler_index].Get();
				auto found=std::find(material_samplers.begin(),material_samplers.end(),sampler);
				if(found==material_samplers.end()&&material_samplers.size()<15){material_samplers.push_back(sampler);found=material_samplers.end()-1;}
				t.info[1]=found==material_samplers.end()?0:static_cast<int>(found-material_samplers.begin());
				t.info[2]=static_cast<int>(sampler_index);
				if(texture.sampler>=0&&texture.sampler<static_cast<int>(Gltf_Model.samplers.size())){const auto& sampler=Gltf_Model.samplers[texture.sampler];if(sampler.wrapS!=10497||sampler.wrapT!=10497||(sampler.magFilter>=0&&sampler.magFilter!=9729)||(sampler.minFilter>=0&&sampler.minFilter!=9987))material.pbr.flags[3]=1;}
			}else t.info[0]=-1;
		}

		material.data.emissive_factor[0] = static_cast<float>(gltf_material.emissiveFactor.at(0));
		material.data.emissive_factor[1] = static_cast<float>(gltf_material.emissiveFactor.at(1));
		material.data.emissive_factor[2] = static_cast<float>(gltf_material.emissiveFactor.at(2));
		const auto emissive_extension = gltf_material.extensions.find("KHR_materials_emissive_strength");
		if (emissive_extension != gltf_material.extensions.end() && emissive_extension->second.IsObject())
		{
			const auto& strength_value = emissive_extension->second.Get("emissiveStrength");
			if (strength_value.IsNumber())
			{
				const float strength = static_cast<float>(strength_value.GetNumberAsDouble());
				for (float& channel : material.data.emissive_factor) channel *= strength;
			}
		}

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

		// 【追加】描画用 SRV ポインタをあらかじめキャッシュ
		const int texture_indices[5] = {
			material.data.pbr_metallic_roughness.basecolor_texture.index,
			material.data.pbr_metallic_roughness.metallic_roughness_texture.index,
			material.data.normal_texture.index,
			material.data.emissive_texture.index,
			material.data.occlusion_texture.index,
		};

		for (int i = 0; i < 5; ++i)
		{
			if (texture_indices[i] > -1 && texture_indices[i] < static_cast<int>(textures.size()))
			{
				int source_index = textures.at(texture_indices[i]).source;
				material.cached_srvs[i] = source_index>=0&&source_index<static_cast<int>(texture_resource_views.size())?texture_resource_views[source_index].Get():nullptr;
			}
			else
			{
				material.cached_srvs[i] = nullptr;
			}
		}
	}

	// glTF permits primitives to omit the material property. Such a primitive
	// uses the glTF default material, so keep an explicit fallback in the GPU
	// material table for those primitives (and for malformed out-of-range IDs).
	material default_material;
	default_material.name = "__gltf_default_material";
	materials.emplace_back(std::move(default_material));

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
	std::vector<GltfPbr::Material> pbr_data;for(const auto& m:materials)pbr_data.push_back(m.pbr);
	buffer_desc.ByteWidth=static_cast<UINT>(sizeof(GltfPbr::Material)*pbr_data.size());buffer_desc.StructureByteStride=sizeof(GltfPbr::Material);subresource_data.pSysMem=pbr_data.data();material_buffer.Reset();
	hr=device->CreateBuffer(&buffer_desc,&subresource_data,material_buffer.GetAddressOf());_ASSERT_EXPR(SUCCEEDED(hr),hr_trace(hr));
	shader_resource_view_desc.Buffer.NumElements=static_cast<UINT>(pbr_data.size());
	hr=device->CreateShaderResourceView(material_buffer.Get(),&shader_resource_view_desc,pbr_material_resource_view_.GetAddressOf());_ASSERT_EXPR(SUCCEEDED(hr),hr_trace(hr));
}

void Gltf_Model::set_pbr_feature_override(int feature)
{
	if(!pbr_material_resource_view_)return;
	std::vector<GltfPbr::Material> data;for(const auto& m:materials){data.push_back(m.pbr);data.back().alpha[2]=static_cast<float>(feature);}
	Microsoft::WRL::ComPtr<ID3D11Resource> buffer;pbr_material_resource_view_->GetResource(buffer.GetAddressOf());
	Graphics_Core::instance().get_device_context()->UpdateSubresource(buffer.Get(),0,nullptr,data.data(),0,0);
}

void Gltf_Model::fetch_textures(ID3D11Device* device, const tinygltf::Model& Gltf_Model)
{
	HRESULT hr{ S_OK };
	// One default sampler, then the glTF samplers. s15 remains the IBL sampler.
	for(int index=-1;index<static_cast<int>(Gltf_Model.samplers.size());++index){D3D11_SAMPLER_DESC d{};d.MaxAnisotropy=1;d.MaxLOD=D3D11_FLOAT32_MAX;d.AddressU=d.AddressV=d.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
		if(index>=0){const auto& s=Gltf_Model.samplers[index];auto address=[](int x){return x==33071?D3D11_TEXTURE_ADDRESS_CLAMP:x==33648?D3D11_TEXTURE_ADDRESS_MIRROR:D3D11_TEXTURE_ADDRESS_WRAP;};d.AddressU=address(s.wrapS);d.AddressV=address(s.wrapT);const int min=s.minFilter<0?9987:s.minFilter;d.Filter=static_cast<D3D11_FILTER>(((min==9729||min==9985||min==9987)?16:0)|(s.magFilter==9728?0:4)|((min==9986||min==9987)?1:0));if(min==9728||min==9729)d.MaxLOD=0;}
		else d.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;device->CreateSamplerState(&d,sampler.GetAddressOf());pbr_samplers_.push_back(std::move(sampler));}
	for (const tinygltf::Texture& gltf_texture : Gltf_Model.textures)
	{
		texture& texture{ textures.emplace_back() };
		texture.name = gltf_texture.name;
		texture.source = gltf_texture.source;
	}
	for (const tinygltf::Image& gltf_image : Gltf_Model.images)
	{
		texture_resource_views.emplace_back();
		pbr_mip_sources_.emplace_back();
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

		if (!gltf_image.image.empty())
		{
			hr=DirectX::CreateWICTextureFromMemoryEx(device,gltf_image.image.data(),gltf_image.image.size(),0,D3D11_USAGE_DEFAULT,D3D11_BIND_SHADER_RESOURCE,0,0,DirectX::WIC_LOADER_IGNORE_SRGB,nullptr,texture_resource_views.back().GetAddressOf());
			if(FAILED(hr))Resource_Manager::instance().load_texture_memory(gltf_image.image.data(),gltf_image.image.size(),texture_resource_views.back().GetAddressOf());
		}
		else if (gltf_image.bufferView > -1)
		{
			const tinygltf::BufferView& buffer_view{ Gltf_Model.bufferViews.at(gltf_image.bufferView) };
			const tinygltf::Buffer& buffer{ Gltf_Model.buffers.at(buffer_view.buffer) };
			const ::byte* data = buffer.data.data() + buffer_view.byteOffset;

			ID3D11ShaderResourceView* texture_resource_view{};
			hr = DirectX::CreateWICTextureFromMemoryEx(device,reinterpret_cast<const uint8_t*>(data),buffer_view.byteLength,0,D3D11_USAGE_DEFAULT,D3D11_BIND_SHADER_RESOURCE,0,0,DirectX::WIC_LOADER_IGNORE_SRGB,nullptr,&texture_resource_view);
			if(FAILED(hr))hr=Resource_Manager::instance().load_texture_memory(data,buffer_view.byteLength,&texture_resource_view);
			if (hr == S_OK)
			{
				texture_resource_views.back().Attach(texture_resource_view);
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
			hr = DirectX::CreateWICTextureFromFileEx(device,filename.c_str(),0,D3D11_USAGE_DEFAULT,D3D11_BIND_SHADER_RESOURCE,0,0,DirectX::WIC_LOADER_IGNORE_SRGB,nullptr,&shader_resource_view);
			if(FAILED(hr))hr=Resource_Manager::instance().load_texture(filename,&shader_resource_view,&texture2d_desc);
			if (hr == S_OK)
			{
				texture_resource_views.back().Attach(shader_resource_view);
			}
		}
		if(texture_resource_views.back())
		{
			Microsoft::WRL::ComPtr<ID3D11Resource> original;texture_resource_views.back()->GetResource(original.GetAddressOf());
			Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;if(SUCCEEDED(original.As(&tex))){D3D11_TEXTURE2D_DESC d{};tex->GetDesc(&d);UINT support=0;device->CheckFormatSupport(d.Format,&support);
				if(d.MipLevels==1&&(support&D3D11_FORMAT_SUPPORT_MIP_AUTOGEN)){d.MipLevels=0;d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;d.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;Microsoft::WRL::ComPtr<ID3D11Texture2D> chain;
					if(SUCCEEDED(device->CreateTexture2D(&d,nullptr,chain.GetAddressOf()))){Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;if(SUCCEEDED(device->CreateShaderResourceView(chain.Get(),nullptr,view.GetAddressOf()))){pbr_mip_sources_.back()=original;texture_resource_views.back()=view;}}}
			}
		}
	}
}

void Gltf_Model::initialize_texture_mips(ID3D11DeviceContext* context)
{
	if(pbr_mips_ready_)return;
	for(size_t i=0;i<pbr_mip_sources_.size();++i)if(pbr_mip_sources_[i]){Microsoft::WRL::ComPtr<ID3D11Resource> target;texture_resource_views[i]->GetResource(target.GetAddressOf());context->CopySubresourceRegion(target.Get(),0,0,0,0,pbr_mip_sources_[i].Get(),0,nullptr);context->GenerateMips(texture_resource_views[i].Get());pbr_mip_sources_[i].Reset();}
	pbr_mips_ready_=true;
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

// =============================================================================
// 【新規】ノード単位の最適化描写ヘルパー関数
// =============================================================================
void Gltf_Model::render_node(
	ID3D11DeviceContext* immediate_context,
	int node_index,
	const std::vector<node>& current_nodes,
	const DirectX::XMFLOAT4X4& world,
	pass_mode pass,
	int& last_bound_material,
	const std::unordered_map<int, primitive_joint_constants>* precomputed_joint_matrices,
	const Frustum* frustum,
	meshlet_culling_stats* culling_stats,
	occlusion_state* occlusion,
	bool use_occlusion)
{
	using namespace DirectX;
	const node& node = current_nodes[node_index];
	const size_t lod_marker = node.name.find("_LOD");
	const bool duplicate_forward_lod =
		(pass == pass_mode::forward_transparency || pass == pass_mode::forward_opaque) &&
		lod_marker != std::string::npos && lod_marker + 4 < node.name.size() &&
		node.name[lod_marker + 4] >= '1' && node.name[lod_marker + 4] <= '9';

	if (node.skin > -1)
	{
		const skin& current_skin = skins.at(node.skin);
		auto upload_joint_constants = [&](const primitive_joint_constants& data)
		{
			D3D11_MAPPED_SUBRESOURCE mapped{};
			const HRESULT hr = immediate_context->Map(primitive_joint_cbuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
			if (FAILED(hr)) return false;

			const size_t bytes = current_skin.joints.size() * sizeof(data.matrices[0]);
			std::memcpy(mapped.pData, data.matrices, bytes);
			immediate_context->Unmap(primitive_joint_cbuffer.Get(), 0);
			immediate_context->VSSetConstantBuffers(3, 1, primitive_joint_cbuffer.GetAddressOf());
			return true;
		};

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
			if (!upload_joint_constants(*cached_data)) return;
		}
		else
		{
			primitive_joint_constants primitive_joint_data{};
			const XMMATRIX node_global_inverse{ XMMatrixInverse(NULL, XMLoadFloat4x4(&node.global_transform)) };
			for (size_t joint_index = 0; joint_index < current_skin.joints.size(); ++joint_index)
			{
				XMStoreFloat4x4(&primitive_joint_data.matrices[joint_index],
					XMLoadFloat4x4(&current_skin.inverse_bind_matrices.at(joint_index)) *
					XMLoadFloat4x4(&current_nodes.at(current_skin.joints.at(joint_index)).global_transform) *
					node_global_inverse
				);
			}
			if (!upload_joint_constants(primitive_joint_data)) return;
		}
	}

	if (node.mesh > -1 && !duplicate_forward_lod)
	{
		const mesh& mesh = meshes[node.mesh];
		for (const auto& primitive : mesh.primitives)
		{
			const material& material = materials[primitive.material];
			const bool transmission=material.pbr.transmission[0]>0;
			const bool full_pbr=force_full_pbr_||material.pbr.flags[3]!=0||primitive.has_color;
			bool in_pass=is_material_in_pass(material.data.alpha_mode,pass);
			if(pass==pass_mode::pbr_opaque)in_pass=full_pbr&&material.data.alpha_mode!=2&&!transmission;
			if(pass==pass_mode::forward_transparency)in_pass=material.data.alpha_mode==2||transmission;
			if(transmission&&(pass==pass_mode::deferred_geometry||pass==pass_mode::directional_shadow||pass==pass_mode::shadow))in_pass=false;
			if (!in_pass)
			{
				continue;
			}

			thread_local std::vector<std::pair<UINT, UINT>> visible_ranges;
			visible_ranges.clear();
			thread_local std::vector<uint8_t> occlusion_visible;
			occlusion_visible.clear();
			if (use_occlusion && occlusion && primitive.occlusion_groups.size() >= 2 &&
				occlusion->slots.size() == total_occlusion_slots_ && node.skin < 0)
			{
				occlusion_visible.resize(primitive.occlusion_groups.size(), 1);
				for (size_t i = 0; i < primitive.occlusion_groups.size(); ++i)
				{
					const UINT slot_index = node.occlusion_slot_base +
						primitive.occlusion_groups[i].slot - mesh.occlusion_slot_base;
					auto& slot = occlusion->slots[slot_index];
					if (slot.pending)
					{
						UINT64 samples = 0;
						const HRESULT result = immediate_context->GetData(slot.query.Get(), &samples,
							sizeof(samples), D3D11_ASYNC_GETDATA_DONOTFLUSH);
						if (result == S_OK)
						{
							slot.visible = samples != 0;
							slot.pending = false;
						}
					}
					occlusion_visible[i] = slot.generation != occlusion->generation ||
						slot.pending || slot.visible;
					if (culling_stats)
					{
						++culling_stats->occlusion_tested;
						if (!occlusion_visible[i]) ++culling_stats->occlusion_culled;
					}
				}
			}
			// Small primitives do not need meshlet scanning, but their bounds can
			// still reject the entire draw as the camera moves.
			if (frustum && node.skin < 0 && !primitive.meshlets.empty() && primitive.meshlets.size() < 8)
			{
				const XMMATRIX transform = XMLoadFloat4x4(&node.global_transform) * XMLoadFloat4x4(&world);
				XMVECTOR primitive_min, primitive_max;
				transform_bounds(primitive.meshlet_bounds_min, primitive.meshlet_bounds_max,
					transform, primitive_min, primitive_max);
				if (classify_frustum_bounds(*frustum, primitive_min, primitive_max) == 0)
				{
					if (culling_stats)
					{
						culling_stats->tested += primitive.meshlets.size();
						culling_stats->culled += primitive.meshlets.size();
						culling_stats->skipped_triangles += primitive.index_buffer_view.count() / 3;
					}
					continue;
				}
			}
			if (frustum && node.skin < 0 && primitive.meshlets.size() >= 8)
			{
				const XMMATRIX transform = XMLoadFloat4x4(&node.global_transform) * XMLoadFloat4x4(&world);
				XMVECTOR primitive_min, primitive_max;
				transform_bounds(primitive.meshlet_bounds_min, primitive.meshlet_bounds_max,
					transform, primitive_min, primitive_max);
				const uint8_t primitive_frustum = classify_frustum_bounds(*frustum, primitive_min, primitive_max);
				if (primitive_frustum == 0)
				{
					if (culling_stats)
					{
						culling_stats->tested += primitive.meshlets.size();
						culling_stats->culled += primitive.meshlets.size();
						culling_stats->skipped_triangles += primitive.index_buffer_view.count() / 3;
					}
					continue;
				}
				const bool occlusion_rejects = !occlusion_visible.empty() &&
					std::find(occlusion_visible.begin(), occlusion_visible.end(), 0) != occlusion_visible.end();
				// The complete primitive needs no meshlet scan and keeps its single draw.
				if (primitive_frustum == 2 && !occlusion_rejects && culling_stats)
					++culling_stats->whole_primitive_fast_paths;
				if (primitive_frustum != 2 || occlusion_rejects)
				{
				if (culling_stats) culling_stats->tested += primitive.meshlets.size();
				const uint64_t culled_before = culling_stats ? culling_stats->culled : 0;
				const uint64_t frustum_skipped_before = culling_stats ? culling_stats->skipped_triangles : 0;
				const uint64_t occlusion_skipped_before = culling_stats ? culling_stats->occlusion_skipped_triangles : 0;
				thread_local std::vector<uint8_t> group_frustum;
				group_frustum.resize(primitive.occlusion_groups.size());
				for (size_t i = 0; i < primitive.occlusion_groups.size(); ++i)
				{
					if (primitive_frustum == 2)
					{
						group_frustum[i] = 2;
						continue;
					}
					XMVECTOR group_min, group_max;
					const auto& group = primitive.occlusion_groups[i];
					transform_bounds(group.bounds_min, group.bounds_max, transform, group_min, group_max);
					group_frustum[i] = classify_frustum_bounds(*frustum, group_min, group_max);
				}
				auto append_range = [&](UINT first_index, UINT index_count)
				{
					if (!visible_ranges.empty() &&
						visible_ranges.back().first + visible_ranges.back().second == first_index)
						visible_ranges.back().second += index_count;
					else
						visible_ranges.emplace_back(first_index, index_count);
				};
				for (size_t group_index = 0; group_index < primitive.occlusion_groups.size(); ++group_index)
				{
					if (visible_ranges.size() > 4) break;
					const auto& group = primitive.occlusion_groups[group_index];
					const auto& first = primitive.meshlets[group.first_meshlet];
					const auto& last = primitive.meshlets[group.first_meshlet + group.meshlet_count - 1];
					const UINT group_index_count = last.first_index + last.index_count - first.first_index;
					const uint8_t status = group_frustum[group_index];
					if (status == 0)
					{
						if (culling_stats)
						{
							culling_stats->culled += group.meshlet_count;
							culling_stats->skipped_triangles += group_index_count / 3;
						}
						continue;
					}
					if (!occlusion_visible.empty() && !occlusion_visible[group_index])
					{
						if (culling_stats) culling_stats->occlusion_skipped_triangles += group_index_count / 3;
						continue;
					}
					if (status == 2)
					{
						append_range(first.first_index, group_index_count);
						continue;
					}
					for (UINT i = group.first_meshlet; i < group.first_meshlet + group.meshlet_count; ++i)
					{
						if (visible_ranges.size() > 4) break;
						const auto& part = primitive.meshlets[i];
						if (!meshlet_intersects_frustum(*frustum, part.bounds_min, part.bounds_max, transform))
						{
							if (culling_stats)
							{
								++culling_stats->culled;
								culling_stats->skipped_triangles += part.index_count / 3;
							}
							continue;
						}
						append_range(part.first_index, part.index_count);
					}
				}
				if (visible_ranges.empty()) continue;
				if (visible_ranges.size() > 4)
				{
					visible_ranges.clear();
					if (culling_stats)
					{
						++culling_stats->fragmented_fallbacks;
						culling_stats->culled = culled_before;
						culling_stats->skipped_triangles = frustum_skipped_before;
						culling_stats->occlusion_skipped_triangles = occlusion_skipped_before;
					}
				}
				}
			}

			// マテリアルが切り替わった時のみ PSSetShaderResources を呼び出す (ステート変更スキップ)
			if (pass != pass_mode::directional_shadow && pass != pass_mode::shadow &&
				primitive.material != last_bound_material)
			{
				immediate_context->PSSetShaderResources(1, 5, material.cached_srvs);
				last_bound_material = primitive.material;
			}

			// 頂点バッファのセット（事前に配列化済み）
			immediate_context->IASetVertexBuffers(
				0,
				static_cast<UINT>(primitive.cached_vertex_buffers.size()),
				primitive.cached_vertex_buffers.data(),
				primitive.cached_strides.data(),
				primitive.cached_offsets.data()
			);

			immediate_context->IASetIndexBuffer(primitive.index_buffer_view.buffer.Get(),
				primitive.index_buffer_view.format, 0);

			primitive_constants primitive_data{};
			primitive_data.material = primitive.material;
			primitive_data.has_tangent = primitive.has_tangent ? 1 : 0; // キャッシュ値を利用
			primitive_data.skin = node.skin;
			primitive_data.pad=(primitive.has_color?1:0)|(primitive.vertex_buffer_views.at("TEXCOORD_1").buffer?2:0);
			XMStoreFloat4x4(&primitive_data.world,
				XMLoadFloat4x4(&node.global_transform) * XMLoadFloat4x4(&world));
			if(pass==pass_mode::deferred_geometry||pass==pass_mode::pbr_opaque||pass==pass_mode::forward_opaque||pass==pass_mode::forward_transparency)
			{
				const float determinant=XMVectorGetX(XMMatrixDeterminant(XMLoadFloat4x4(&primitive_data.world)));
				Render_State::instance().set_rasterizer_state(immediate_context,determinant<0?Rasterizer_State::Cull_None_CCW:Rasterizer_State::Cull_None_CW);
			}

			immediate_context->UpdateSubresource(primitive_cbuffer.Get(), 0, 0, &primitive_data, 0, 0);
			immediate_context->VSSetConstantBuffers(0, 1, primitive_cbuffer.GetAddressOf());
			if (pass != pass_mode::directional_shadow && pass != pass_mode::shadow)
				immediate_context->PSSetConstantBuffers(0, 1, primitive_cbuffer.GetAddressOf());
			if(pass==pass_mode::deferred_geometry||pass==pass_mode::pbr_opaque||pass==pass_mode::forward_opaque||pass==pass_mode::forward_transparency)
			{
				immediate_context->PSSetShaderResources(47,1,pbr_material_resource_view_.GetAddressOf());
				immediate_context->PSSetShaderResources(48,GltfPbr::TextureCount,material.pbr_srvs);
				ID3D11SamplerState* samplers[15]{};for(auto& sampler:samplers)sampler=pbr_samplers_[0].Get();
				for(const auto& texture:material.pbr.textures)if(texture.info[0]>=0&&texture.info[1]>0)samplers[texture.info[1]]=pbr_samplers_[texture.info[2]].Get();
				immediate_context->PSSetSamplers(0,15,samplers);
				auto* environment_sampler=pbr_samplers_[0].Get();immediate_context->PSSetSamplers(15,1,&environment_sampler);
			}

			if (visible_ranges.empty())
			{
				if (culling_stats) ++culling_stats->draw_calls;
				immediate_context->DrawIndexed(static_cast<UINT>(primitive.index_buffer_view.count()), 0, 0);
			}
			else
			{
				if (culling_stats) culling_stats->draw_calls += visible_ranges.size();
				for (const auto& range : visible_ranges)
					immediate_context->DrawIndexed(range.second, range.first, 0);
			}
		}
	}

	for (int child_index : node.children)
	{
		render_node(immediate_context, child_index, current_nodes, world, pass, last_bound_material, precomputed_joint_matrices, frustum, culling_stats, occlusion, use_occlusion);
	}
}

void Gltf_Model::render(ID3D11DeviceContext* immediate_context, const DirectX::XMFLOAT4X4& world, pass_mode pass, bool is_animation, float delta_time, animation_mode anim_mode, int animation_index, const Frustum* frustum, meshlet_culling_stats* culling_stats, occlusion_state* occlusion, bool use_occlusion)
{
	initialize_texture_mips(immediate_context);
	if (pass == pass_mode::deferred_geometry)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().get_native_shader<Pixel_Shader>("GLTF_DEFERRED_GEOMETRY_PS"), NULL, 0);
	}
	else if (pass == pass_mode::forward_opaque || pass == pass_mode::pbr_opaque)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().get_native_shader<Pixel_Shader>("GLTF_FORWARD_OPAQUE_PS"), NULL, 0);
	}
	else if (pass == pass_mode::forward_transparency)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().get_native_shader<Pixel_Shader>("GLTF_FORWARD_TRANSPARENCY_PS"), NULL, 0);
	}
	else if (pass == pass_mode::voxel_gi)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().get_native_shader<Pixel_Shader>("VOXEL_GI_PS"), NULL, 0);
	}
	else if (pass == pass_mode::directional_shadow)
	{
		immediate_context->PSSetShader(nullptr, nullptr, 0);
	}
	else if (pass == pass_mode::shadow)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().get_native_shader<Pixel_Shader>("POINT_SHADOW_PS"), NULL, 0);
	}
	else
	{
		_ASSERT_EXPR(FALSE, L"Unknown pass_mode");
	}

	bool use_animation_cache = false;

	if (is_animation && animations.size() > 0)
	{
		// メンバー変数 animated_nodes_cache を再利用して動的確保を回避
		animated_nodes_cache = nodes;
		time += delta_time;

		if (anim_mode == animation_mode::single)
		{
			if (animation_index >= 0 && animation_index < static_cast<int>(animations.size()))
			{
				current_animation_index = animation_index;
				const float duration = animations[current_animation_index].duration;

				if (duration <= 0.0f || time >= duration)
				{
					time = 0.0f;
				}
				animate(current_animation_index, time, animated_nodes_cache);
				use_animation_cache = true;
			}
		}
		else if (anim_mode == animation_mode::all)
		{
			animate_all(time, animated_nodes_cache);
			use_animation_cache = true;
		}
	}

	const std::vector<node>& target_nodes = use_animation_cache ? animated_nodes_cache : nodes;

	const char* vertex_shader_name;
	switch (pass)
	{
	case pass_mode::directional_shadow:
		vertex_shader_name = "DIRECTIONAL_SHADOW_VS";
		break;
	case pass_mode::shadow:
		vertex_shader_name = "POINT_SHADOW_VS";
		break;
	case pass_mode::voxel_gi:
		vertex_shader_name = "VOXEL_GI_VS";
		break;
	default:
		vertex_shader_name = "GLTF_VS";
		break;
	}

	immediate_context->VSSetShader(Resource_Manager::instance().get_native_shader<Vertex_Shader>(vertex_shader_name), nullptr, 0);
	if (pass != pass_mode::directional_shadow && pass != pass_mode::shadow)
		immediate_context->PSSetShaderResources(0, 1, material_resource_view.GetAddressOf());
	immediate_context->IASetInputLayout(Resource_Manager::instance().get_shader<Vertex_Shader>(vertex_shader_name)->get_input_layout());
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	int last_bound_material = -1;
	for (int node_index : scenes.at(0).nodes)
	{
		render_node(immediate_context, node_index, target_nodes, world, pass, last_bound_material, nullptr, frustum, culling_stats, occlusion, use_occlusion);
	}
}

void Gltf_Model::render_with_nodes(
	ID3D11DeviceContext* immediate_context,
	const DirectX::XMFLOAT4X4& world,
	pass_mode pass,
	const std::vector<node>* precomputed_nodes,
	const std::unordered_map<int, primitive_joint_constants>* precomputed_joint_matrices,
	const Frustum* frustum,
	meshlet_culling_stats* culling_stats,
	occlusion_state* occlusion,
	bool use_occlusion)
{
	if (!precomputed_nodes || precomputed_nodes->empty())
	{
		render(immediate_context, world, pass, false, 0.0f, animation_mode::single, 0, frustum, culling_stats, occlusion, use_occlusion);
		return;
	}
	initialize_texture_mips(immediate_context);

	if (pass == pass_mode::deferred_geometry)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().get_native_shader<Pixel_Shader>("GLTF_DEFERRED_GEOMETRY_PS"), NULL, 0);
	}
	else if (pass == pass_mode::forward_opaque || pass == pass_mode::pbr_opaque)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().get_native_shader<Pixel_Shader>("GLTF_FORWARD_OPAQUE_PS"), NULL, 0);
	}
	else if (pass == pass_mode::forward_transparency)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().get_native_shader<Pixel_Shader>("GLTF_FORWARD_TRANSPARENCY_PS"), NULL, 0);
	}
	else if (pass == pass_mode::voxel_gi)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().get_native_shader<Pixel_Shader>("VOXEL_GI_PS"), NULL, 0);
	}
	else if (pass == pass_mode::directional_shadow)
	{
		immediate_context->PSSetShader(nullptr, nullptr, 0);
	}
	else if (pass == pass_mode::shadow)
	{
		immediate_context->PSSetShader(Resource_Manager::instance().get_native_shader<Pixel_Shader>("POINT_SHADOW_PS"), NULL, 0);
	}
	else
	{
		_ASSERT_EXPR(FALSE, L"Unknown pass_mode");
	}

	const char* vertex_shader_name;
	switch (pass)
	{
	case pass_mode::directional_shadow:
		vertex_shader_name = "DIRECTIONAL_SHADOW_VS";
		break;
	case pass_mode::shadow:
		vertex_shader_name = "POINT_SHADOW_VS";
		break;
	case pass_mode::voxel_gi:
		vertex_shader_name = "VOXEL_GI_VS";
		break;
	default:
		vertex_shader_name = "GLTF_VS";
		break;
	}

	immediate_context->VSSetShader(Resource_Manager::instance().get_native_shader<Vertex_Shader>(vertex_shader_name), nullptr, 0);
	if (pass != pass_mode::directional_shadow && pass != pass_mode::shadow)
		immediate_context->PSSetShaderResources(0, 1, material_resource_view.GetAddressOf());
	immediate_context->IASetInputLayout(Resource_Manager::instance().get_shader<Vertex_Shader>(vertex_shader_name)->get_input_layout());
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	int last_bound_material = -1;
	for (int node_index : scenes.at(0).nodes)
	{
		render_node(immediate_context, node_index, *precomputed_nodes, world, pass, last_bound_material, precomputed_joint_matrices, frustum, culling_stats, occlusion, use_occlusion);
	}
}

UINT Gltf_Model::issue_occlusion_queries(
	ID3D11DeviceContext* context,
	const DirectX::XMFLOAT4X4& world,
	const DirectX::XMMATRIX& view_projection,
	const DirectX::XMVECTOR& camera_position,
	const Frustum& frustum,
	occlusion_state& state,
	ID3D11Buffer* box_cbuffer,
	UINT query_slot_budget)
{
	using namespace DirectX;
	if (!context || !box_cbuffer || !total_occlusion_slots_ || !query_slot_budget) return 0;
	UINT issued = 0;
	if (state.model != this)
	{
		state.slots.clear();
		state.valid = false;
		state.model = this;
		state.query_cursor = 0;
	}
	if (state.slots.size() != total_occlusion_slots_) state.slots.resize(total_occlusion_slots_);
	const UINT slot_window = std::min(query_slot_budget, total_occlusion_slots_);
	const UINT window_start = state.query_cursor;
	state.query_cursor = (state.query_cursor + slot_window) % total_occlusion_slots_;

	XMFLOAT4X4 current_view_projection;
	XMStoreFloat4x4(&current_view_projection, view_projection);
	if (!state.valid || std::memcmp(&state.view_projection, &current_view_projection, sizeof(current_view_projection)) ||
		std::memcmp(&state.world, &world, sizeof(world)))
	{
		++state.generation;
		state.view_projection = current_view_projection;
		state.world = world;
		state.valid = true;
	}

	std::vector<int> pending_nodes(scenes.at(0).nodes.begin(), scenes.at(0).nodes.end());
	D3D11_QUERY_DESC query_desc{};
	query_desc.Query = D3D11_QUERY_OCCLUSION;
	for (size_t node_cursor = 0; node_cursor < pending_nodes.size(); ++node_cursor)
	{
		const node& current_node = nodes[pending_nodes[node_cursor]];
		pending_nodes.insert(pending_nodes.end(), current_node.children.begin(), current_node.children.end());
		if (current_node.mesh < 0 || current_node.skin >= 0) continue;
		const XMMATRIX node_world = XMLoadFloat4x4(&current_node.global_transform) * XMLoadFloat4x4(&world);
		const mesh& current_mesh = meshes[current_node.mesh];
		for (const auto& primitive : current_mesh.primitives)
		{
			if (primitive.occlusion_groups.size() < 2) continue;
			if (!is_material_in_pass(materials[primitive.material].data.alpha_mode, pass_mode::deferred_geometry))
				continue;
			for (const auto& group : primitive.occlusion_groups)
			{
				const UINT slot_index = current_node.occlusion_slot_base +
					group.slot - current_mesh.occlusion_slot_base;
				if ((slot_index + total_occlusion_slots_ - window_start) % total_occlusion_slots_ >= slot_window)
					continue;
				auto& slot = state.slots[slot_index];
				if (slot.pending)
				{
					UINT64 samples = 0;
					if (context->GetData(slot.query.Get(), &samples, sizeof(samples), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
						continue;
					slot.pending = false;
					slot.visible = samples != 0;
				}

				// Expand the proxy so a surface coincident with its box face does
				// not fail the strict depth test. Never reject when near/inside it.
				const XMFLOAT3 center{
					(group.bounds_min.x + group.bounds_max.x) * 0.5f,
					(group.bounds_min.y + group.bounds_max.y) * 0.5f,
					(group.bounds_min.z + group.bounds_max.z) * 0.5f };
				const XMFLOAT3 half{
					(group.bounds_max.x - group.bounds_min.x) * 0.51f + 0.001f,
					(group.bounds_max.y - group.bounds_min.y) * 0.51f + 0.001f,
					(group.bounds_max.z - group.bounds_min.z) * 0.51f + 0.001f };
				const XMFLOAT3 box_min{ center.x - half.x, center.y - half.y, center.z - half.z };
				const XMFLOAT3 box_max{ center.x + half.x, center.y + half.y, center.z + half.z };
				XMVECTOR world_min, world_max;
				transform_bounds(box_min, box_max, node_world, world_min, world_max);
				if (!AABBIntersectsFrustum(frustum, world_min, world_max)) continue;
				const XMVECTOR margin = XMVectorReplicate(0.25f);
				if (XMVector3GreaterOrEqual(camera_position, XMVectorSubtract(world_min, margin)) &&
					XMVector3LessOrEqual(camera_position, XMVectorAdd(world_max, margin)))
				{
					slot.visible = true;
					continue;
				}
				if (!slot.query && FAILED(Graphics_Core::instance().get_device()->CreateQuery(&query_desc,
					slot.query.GetAddressOf()))) continue;

				XMFLOAT4X4 box_matrix;
				XMStoreFloat4x4(&box_matrix,
					XMMatrixScaling(half.x * 2.0f, half.y * 2.0f, half.z * 2.0f) *
					XMMatrixTranslation(center.x, center.y, center.z) * node_world * view_projection);
				context->UpdateSubresource(box_cbuffer, 0, nullptr, &box_matrix, 0, 0);
				context->Begin(slot.query.Get());
				context->Draw(36, 0);
				context->End(slot.query.Get());
				++issued;
				slot.pending = true;
				slot.generation = state.generation;
			}
		}
	}
	return issued;
}

void Gltf_Model::animate(size_t animation_index, float time, std::vector<node>& animated_nodes)
{
	using namespace std;
	using namespace DirectX;

	auto indexof = [](const vector<float>& timelines, float time, float& interpolation_factor)->size_t {
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
		};

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

	auto indexof = [](const vector<float>& timelines, float time, float& interpolation_factor)->size_t
		{
			const size_t keyframe_count{ timelines.size() };

			if (keyframe_count <= 1)
			{
				interpolation_factor = 0.0f;
				return 0;
			}

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
			interpolation_factor =
				(time - timelines.at(keyframe_index)) /
				(timelines.at(keyframe_index + 1) - timelines.at(keyframe_index));
			return keyframe_index;
		};

	for (const animation& animation : animations)
	{
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
				if (scales.size() < 2 || keyframe_index + 1 >= scales.size()) continue;
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
				if (rotations.size() < 2 || keyframe_index + 1 >= rotations.size()) continue;
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
				if (translations.size() < 2 || keyframe_index + 1 >= translations.size()) continue;
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

	for (size_t node_idx = 0; node_idx < nodes.size(); ++node_idx)
	{
		const auto& nd = nodes[node_idx];
		if (nd.mesh < 0 || nd.mesh >= static_cast<int>(meshes.size()))
			continue;

		XMMATRIX node_world = XMLoadFloat4x4(&nd.global_transform) * world;

		const auto& msh = meshes[nd.mesh];
		for (const auto& prim : msh.primitives)
		{
			const auto& pos = prim.cpu_positions;
			const auto& idx = prim.cpu_indices;
			if (pos.empty() || idx.size() < 3) continue;

			for (size_t i = 0; i + 2 < idx.size(); i += 3)
			{
				uint32_t i0 = idx[i + 0];
				uint32_t i1 = idx[i + 1];
				uint32_t i2 = idx[i + 2];
				if (i0 >= pos.size() || i1 >= pos.size() || i2 >= pos.size())
					continue;

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
