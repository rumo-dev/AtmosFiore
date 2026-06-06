
#include "obj_model.h"

using namespace DirectX;
Obj_Model::Obj_Model(ID3D11Device* device, const wchar_t* obj_filename, bool flipping_v_coordinates/*UNIT.14*/)
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	uint32_t current_index{ 0 };

	std::vector<XMFLOAT3> positions;
	std::vector<XMFLOAT3> normals;
	// UNIT.14
	std::vector<XMFLOAT2> texcoords;
	std::vector<std::wstring> mtl_filenames;

	std::wifstream fin(obj_filename);
	_ASSERT_EXPR(fin, L"'OBJ file not found.");
	wchar_t command[256];
	while (fin)
	{
		fin >> command;
		if (0 == wcscmp(command, L"v"))
		{
			// v x y z w
			//
			// Specifies a geometric vertex and its x y z coordinates.Rational
			// curves and surfaces require a fourth homogeneous coordinate, also
			// called the weight.
			//
			// x y z are the x, y, and z coordinates for the vertex.These are
			// floating point numbers that define the position of the vertex in
			// three dimensions.
			//
			// w is the weight required for rational curves and surfaces.It is
			// not required for non - rational curves and surfaces.If you do not
			// specify a value for w, the default is 1.0.
			float x, y, z;
			fin >> x >> y >> z;
			positions.push_back({ x, y, z });
			fin.ignore(1024, L'\n');
		}
		// UNIT.14
		else if (0 == wcscmp(command, L"vt"))
		{
			// vt u v w
			//
			// Specifies a texture vertex and its coordinates.A 1D texture
			// requires only u texture coordinates, a 2D texture requires both u
			// and v texture coordinates, and a 3D texture requires all three
			// coordinates.
			//
			// u is the value for the horizontal direction of the texture.
			//
			// v is an optional argument.
			//
			// v is the value for the vertical direction of the texture.The
			// default is 0.
			//
			// w is an optional argument.
			//
			// w is a value for the depth of the texture.The default is 0.
			float u, v;
			fin >> u >> v;
			texcoords.push_back({ u, flipping_v_coordinates ? 1.0f - v : v });
			fin.ignore(1024, L'\n');
		}
		else if (0 == wcscmp(command, L"vn"))
		{
			// vn i j k
			//
			// Specifies a normal vector with components i, j, and k.
			//
			// Vertex normals affect the smooth - shading and rendering of geometry.0
			// For polygons, vertex normals are used in place of the actual facet
			// normals.For surfaces, vertex normals are interpolated over the
			// entire surface and replace the actual analytic surface normal.
			//
			// When vertex normals are present, they supersede smoothing groups.
			//
			// i j k are the i, j, and k coordinates for the vertex normal.They
			// are floating point numbers.
			FLOAT i, j, k;
			fin >> i >> j >> k;
			normals.push_back({ i, j, k });
			fin.ignore(1024, L'\n');
		}
		else if (0 == wcscmp(command, L"f"))
		{
			//f  v1 / vt1 / vn1   v2 / vt2 / vn2   v3 / vt3 / vn3 . . .
			//
			// optionally include the texture vertex and vertex normal reference
			// numbers.
			//
			// The reference numbers for the vertices, texture vertices, and
			// vertex normals must be separated by slashes(/ ).There is no space
			// between the number and the slash.
			//
			// v is the reference number for a vertex in the face element.A
			// minimum of three vertices are required.
			//
			// vt is an optional argument.
			//
			// vt is the reference number for a texture vertex in the face
			// element.It always follows the first slash.
			//
			// vn is an optional argument.
			//
			// vn is the reference number for a vertex normal in the face element.
			// It must always follow the second slash.
			//
			// Face elements use surface normals to indicate their orientation.If
			// vertices are ordered counterclockwise around the face, both the
			// face and the normal will point toward the viewer.If the vertex
			// ordering is clockwise, both will point away from the viewer.If
			// vertex normals are assigned, they should point in the general
			// direction of the surface normal, otherwise unpredictable results
			// may occur.
			//
			// If a face has a texture map assigned to it and no texture vertices
			// are assigned in the f statement, the texture map is ignored when
			// the element is rendered.
			for (size_t i = 0; i < 3; i++)
			{
				Vertex vertex;
				size_t v, vt, vn;

				fin >> v;
				vertex.position = positions.at(v - 1);
				if (L'/' == fin.peek())
				{
					fin.ignore(1);
					if (L'/' != fin.peek())
					{
						fin >> vt;
						// UNIT.14
						vertex.texcoord = texcoords.at(vt - 1);
					}
					if (L'/' == fin.peek())
					{
						fin.ignore(1);
						fin >> vn;
						vertex.normal = normals.at(vn - 1);
					}
				}
				vertices.push_back(vertex);
				indices.push_back(current_index++);
			}
			fin.ignore(1024, L'\n');
		}
		// UNIT.14
		else if (0 == wcscmp(command, L"mtllib"))
		{
			// mtllib filename1 filename2 . . .
			// Specifies the material library file for the material definitions
			// set with the usemtl statement.You can specify multiple filenames
			// with mtllib.If multiple filenames are specified, the first file
			// listed is searched first for the material definition, the second
			// file is searched next, and so on.
			wchar_t mtllib[256];
			fin >> mtllib;
			mtl_filenames.push_back(mtllib);
		}
		// UNIT.15
		else if (0 == wcscmp(command, L"usemtl"))
		{
			wchar_t usemtl[MAX_PATH]{ 0 };
			fin >> usemtl;
			subsets.push_back({ usemtl, static_cast<uint32_t>(indices.size()), 0 });
		}
		else
		{
			fin.ignore(1024, L'\n');
		}
	}
	fin.close();

	// UNIT.15
	std::vector<Subset>::reverse_iterator iterator = subsets.rbegin();
	iterator->index_count = static_cast<uint32_t>(indices.size()) - iterator->index_start;
	for (iterator = subsets.rbegin() + 1; iterator != subsets.rend(); ++iterator)
	{
		iterator->index_count = (iterator - 1)->index_start - iterator->index_start;
	}

	// UNIT.14
	std::filesystem::path mtl_filename(obj_filename);
	mtl_filename.replace_filename(std::filesystem::path(mtl_filenames[0]).filename());

	fin.open(mtl_filename);
	// UNIT.16
	//_ASSERT_EXPR(fin, L"'MTL file not found.");

	while (fin)
	{
		fin >> command;
		if (0 == wcscmp(command, L"map_Kd"))
		{
			// map_Kd - options args filename
			//
			// Specifies that a color texture file or color procedural texture file is
			// linked to the diffuse reflectivity of the material.During rendering,
			// the map_Kd value is multiplied by the Kd value.
			//
			// "filename" is the name of a color texture file(.mpc), a color
			// procedural texture file(.cxc), or an image file.
			fin.ignore();
			wchar_t map_Kd[256];
			fin >> map_Kd;

			std::filesystem::path path(obj_filename);
			path.replace_filename(std::filesystem::path(map_Kd).filename());
			// UNIT.15
			//texture_filename = path;
			//materials.rbegin()->texture_filename = path;
			// UNIT.16
			materials.rbegin()->texture_filenames[0] = path;
			fin.ignore(1024, L'\n');
		}
		// UNIT.16
		else if (0 == wcscmp(command, L"map_bump") || 0 == wcscmp(command, L"bump"))
		{
			// map_bump - options args filename
			//
			// Specifies that a bump texture file or a bump procedural texture file is
			// linked to the material.
			//
			// "filename" is the name of a bump texture file(.mpb), a bump procedural
			// texture file(.cxb), or an image file.
			fin.ignore();
			wchar_t map_bump[256];
			fin >> map_bump;

			std::filesystem::path path(obj_filename);
			path.replace_filename(std::filesystem::path(map_bump).filename());
			materials.rbegin()->texture_filenames[1] = path;
			fin.ignore(1024, L'\n');
		}
		// UNIT.15
		else if (0 == wcscmp(command, L"newmtl"))
		{
			// The folowing syntax describes the material name statement.
			//
			//	newmtl name
			//
			// Specifies the start of a material description and assigns a name to the
			// material.An.mtl file must have one newmtl statement at the start of
			// each material description.
			// "name" is the name of the material.Names may be any length but
			// cannot include blanks.Underscores may be used in material names.material material;
			fin.ignore();
			wchar_t newmtl[256];
			Material material;
			fin >> newmtl;
			material.name = newmtl;
			materials.push_back(material);
		}
		// UNIT.15
		else if (0 == wcscmp(command, L"Ka"))
		{
			// Ka r g b
			//
			// The Ka statement specifies the ambient reflectivity using RGB values.
			// "r g b" are the values for the red, green, and blue components of the
			// color.The g and b arguments are optional.If only r is specified,
			// then g, and b are assumed to be equal to r.The r g b values are
			// normally in the range of 0.0 to 1.0.Values outside this range increase
			// or decrease the relectivity accordingly.
			float r, g, b;
			fin >> r >> g >> b;
			materials.rbegin()->Ka = { r, g, b, 1 };
			fin.ignore(1024, L'\n');
		}
		// UNIT.15
		else if (0 == wcscmp(command, L"Kd"))
		{
			// Kd r g b
			//
			// The Kd statement specifies the diffuse reflectivity using RGB values.
			// "r g b" are the values for the red, green, and blue components of the
			// atmosphere.The g and b arguments are optional.If only r is
			// specified, then g, and b are assumed to be equal to r.The r g b values
			// are normally in the range of 0.0 to 1.0.Values outside this range
			// increase or decrease the relectivity accordingly.
			float r, g, b;
			fin >> r >> g >> b;
			materials.rbegin()->Kd = { r, g, b, 1 };
			fin.ignore(1024, L'\n');
		}
		// UNIT.15
		else if (0 == wcscmp(command, L"Ks"))
		{
			// Ks r g b
			//
			// The Ks statement specifies the specular reflectivity using RGB values.
			// "r g b" are the values for the red, green, and blue components of the
			// atmosphere.The g and b arguments are optional.If only r is
			// specified, then g, and b are assumed to be equal to r.The r g b values
			// are normally in the range of 0.0 to 1.0.Values outside this range
			// increase or decrease the relectivity accordingly.
			float r, g, b;
			fin >> r >> g >> b;
			materials.rbegin()->Ks = { r, g, b, 1 };
			fin.ignore(1024, L'\n');
		}
		else
		{
			fin.ignore(1024, L'\n');
		}
	}
	fin.close();

	create_com_buffers(device, vertices.data(), vertices.size(), indices.data(), indices.size());

	HRESULT hr{ S_OK };

	D3D11_INPUT_ELEMENT_DESC input_element_desc[]
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		// UNIT.14
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	create_vs_from_cso(device, "obj_model_vs.cso", _vertex_shader.GetAddressOf(), _input_layout.GetAddressOf(), input_element_desc, ARRAYSIZE(input_element_desc));
	create_ps_from_cso(device, "obj_model_ps.cso", _pixel_shader.GetAddressOf());

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(Constants);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	hr = device->CreateBuffer(&buffer_desc, nullptr, _constant_buffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	// UNIT.16
	if (materials.size() == 0)
	{
		for (const Subset& subset : subsets)
		{
			materials.push_back({ subset.usemtl });
		}
	}

	// UNIT.14
	D3D11_TEXTURE2D_DESC texture2d_desc{};
	// UNIT.15
	//load_texture_from_file(device, texture_filename.c_str(), shader_resource_view.GetAddressOf(), &texture2d_desc);
	for (Material& material : materials)
	{
		//load_texture_from_file(device, material.texture_filename.c_str(), material.shader_resource_view.GetAddressOf(), &texture2d_desc);
		// UNIT.16
		if (material.texture_filenames[0].size() > 0)
		{
			load_texture_from_file(device, material.texture_filenames[0].c_str(), material.shader_resource_views[0].GetAddressOf(), &texture2d_desc);
		}
		else
		{
			make_dummy_texture(device, material.shader_resource_views[0].GetAddressOf(), 0xFFFFFFFF, 16);
		}
		if (material.texture_filenames[1].size() > 0)
		{
			load_texture_from_file(device, material.texture_filenames[1].c_str(), material.shader_resource_views[1].GetAddressOf(), &texture2d_desc);
		}
		else
		{
			make_dummy_texture(device, material.shader_resource_views[1].GetAddressOf(), 0xFFFF7F7F, 16);
		}
	}

	// UNIT.16
	for (const Vertex& v : vertices)
	{
		bounding_box[0].x = std::min<float>(bounding_box[0].x, v.position.x);
		bounding_box[0].y = std::min<float>(bounding_box[0].y, v.position.y);
		bounding_box[0].z = std::min<float>(bounding_box[0].z, v.position.z);
		bounding_box[1].x = std::max<float>(bounding_box[1].x, v.position.x);
		bounding_box[1].y = std::max<float>(bounding_box[1].y, v.position.y);
		bounding_box[1].z = std::max<float>(bounding_box[1].z, v.position.z);
	}
}

// UNIT.13
void Obj_Model::render(ID3D11DeviceContext* immediate_context, const XMFLOAT4X4& world, const XMFLOAT4& material_color, bool is_baunding_box, ID3D11PixelShader* alternative_pixel_shader/*UNIT.16*/)
{
	uint32_t stride{ sizeof(Vertex) };
	uint32_t offset{ 0 };
	immediate_context->IASetVertexBuffers(0, 1, _vertex_buffer.GetAddressOf(), &stride, &offset);
	immediate_context->IASetIndexBuffer(_index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	immediate_context->IASetInputLayout(_input_layout.Get());

	immediate_context->VSSetShader(_vertex_shader.Get(), nullptr, 0);
	//immediate_context->PSSetShader(pixel_shader.Get(), nullptr, 0);
	// UNIT.16
	alternative_pixel_shader ? immediate_context->PSSetShader(alternative_pixel_shader, nullptr, 0) : immediate_context->PSSetShader(_pixel_shader.Get(), nullptr, 0);

#if 0
	// UNIT.14
	immediate_context->PSSetShaderResources(0, 1, shader_resource_view.GetAddressOf());

	D3D11_BUFFER_DESC buffer_desc{};
	index_buffer->GetDesc(&buffer_desc);
	immediate_context->DrawIndexed(buffer_desc.ByteWidth / sizeof(uint32_t), 0, 0);
#else
	// UNIT.15
	for (const Material& material : materials)
	{
		immediate_context->PSSetShaderResources(0, 1, material.shader_resource_views[0].GetAddressOf());
		// UNIT.16
		immediate_context->PSSetShaderResources(1, 1, material.shader_resource_views[1].GetAddressOf());

		Constants data{ world, material.Ka, material.Kd, material.Ks };
		XMStoreFloat4(&data.Kd, XMLoadFloat4(&material_color) * XMLoadFloat4(&material.Kd));
		immediate_context->UpdateSubresource(_constant_buffer.Get(), 0, 0, &data, 0, 0);
		immediate_context->VSSetConstantBuffers(0, 1, _constant_buffer.GetAddressOf());
		immediate_context->PSSetConstantBuffers(0, 1, _constant_buffer.GetAddressOf());


		for (const Subset& subset : subsets)
		{
			if (material.name == subset.usemtl)
			{
				immediate_context->DrawIndexed(subset.index_count, subset.index_start, 0);
			}
		}
	}
	if (is_baunding_box) {
		{
			// バウンディングボックス描画
			uint32_t stride{ sizeof(XMFLOAT3) };
			uint32_t offset{ 0 };
			immediate_context->IASetVertexBuffers(0, 1, _bbox_vertex_buffer.GetAddressOf(), &stride, &offset);
			immediate_context->IASetIndexBuffer(_bbox_index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST); // ワイヤーフレーム
			immediate_context->IASetInputLayout(_input_layout.Get()); // 頂点レイアウトはXMFLOAT3だけに対応している必要あり

			immediate_context->VSSetShader(_vertex_shader.Get(), nullptr, 0);
			immediate_context->PSSetShader(_pixel_shader.Get(), nullptr, 0); // 単色で良い

			Constants data{};
			data.world = world;
			data.Kd = material_color;
			immediate_context->UpdateSubresource(_constant_buffer.Get(), 0, 0, &data, 0, 0);
			immediate_context->VSSetConstantBuffers(0, 1, _constant_buffer.GetAddressOf());
			immediate_context->PSSetConstantBuffers(0, 1, _constant_buffer.GetAddressOf());

			immediate_context->DrawIndexed(24, 0, 0);
		}
	}
#endif
}

// UNIT.13
void Obj_Model::create_com_buffers(ID3D11Device* device, Vertex* vertices, size_t vertex_count, uint32_t* indices, size_t index_count)
{
	HRESULT hr = S_OK;

	D3D11_BUFFER_DESC buffer_desc{};
	D3D11_SUBRESOURCE_DATA subresource_data{};
	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertex_count);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	buffer_desc.CPUAccessFlags = 0;
	buffer_desc.MiscFlags = 0;
	buffer_desc.StructureByteStride = 0;
	subresource_data.pSysMem = vertices;
	subresource_data.SysMemPitch = 0;
	subresource_data.SysMemSlicePitch = 0;
	hr = device->CreateBuffer(&buffer_desc, &subresource_data, _vertex_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	buffer_desc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * index_count);
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	subresource_data.pSysMem = indices;
	hr = device->CreateBuffer(&buffer_desc, &subresource_data, _index_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}

void Obj_Model::create_bounding_box(ID3D11Device* device)
{
	XMFLOAT3 min = bounding_box[0];
	XMFLOAT3 max = bounding_box[1];

	XMFLOAT3 corners[8] =
	{
		{min.x, min.y, min.z},
		{max.x, min.y, min.z},
		{max.x, max.y, min.z},
		{min.x, max.y, min.z},
		{min.x, min.y, max.z},
		{max.x, min.y, max.z},
		{max.x, max.y, max.z},
		{min.x, max.y, max.z},
	};

	uint32_t indices[24] =
	{
		0,1, 1,2, 2,3, 3,0, // 前面
		4,5, 5,6, 6,7, 7,4, // 背面
		0,4, 1,5, 2,6, 3,7  // 側面
	};

	HRESULT hr = S_OK;

	D3D11_BUFFER_DESC desc{};
	D3D11_SUBRESOURCE_DATA data{};

	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.ByteWidth = sizeof(corners);
	data.pSysMem = corners;
	hr = device->CreateBuffer(&desc, &data, _bbox_vertex_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	desc.ByteWidth = sizeof(indices);
	data.pSysMem = indices;
	hr = device->CreateBuffer(&desc, &data, _bbox_index_buffer.ReleaseAndGetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
}