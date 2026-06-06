#include "sprite_batch.h"
#include "sprite.h"
#include "Engine/Graphics/shader/shader.h"
#include <algorithm>
#include <cmath>

namespace
{
	const D3D11_INPUT_ELEMENT_DESC SpriteInputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
}

void sprite_batch::initialize(ID3D11Device* device)
{
	if (_initialized || device == nullptr)
	{
		return;
	}

	create_vs_from_cso(device, "sprite_vs.cso", &_vertex_shader, &_input_layout, const_cast<D3D11_INPUT_ELEMENT_DESC*>(SpriteInputLayout), 3);
	create_ps_from_cso(device, "sprite_ps.cso", &_pixel_shader);

	D3D11_BUFFER_DESC vb_desc{};
	vb_desc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * MaxSpritesPerBatch * VerticesPerSprite);
	vb_desc.Usage = D3D11_USAGE_DYNAMIC;
	vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	HRESULT hr = device->CreateBuffer(&vb_desc, nullptr, &_vertex_buffer);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	std::vector<uint16_t> indices(MaxSpritesPerBatch * IndicesPerSprite);
	for (uint16_t sprite_index = 0; sprite_index < static_cast<uint16_t>(MaxSpritesPerBatch); ++sprite_index)
	{
		const uint16_t base = static_cast<uint16_t>(sprite_index * VerticesPerSprite);
		const size_t offset = static_cast<size_t>(sprite_index) * IndicesPerSprite;
		indices[offset + 0] = base + 0;
		indices[offset + 1] = base + 1;
		indices[offset + 2] = base + 2;
		indices[offset + 3] = base + 2;
		indices[offset + 4] = base + 1;
		indices[offset + 5] = base + 3;
	}

	D3D11_BUFFER_DESC ib_desc{};
	ib_desc.ByteWidth = static_cast<UINT>(sizeof(uint16_t) * indices.size());
	ib_desc.Usage = D3D11_USAGE_IMMUTABLE;
	ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA ib_data{};
	ib_data.pSysMem = indices.data();
	hr = device->CreateBuffer(&ib_desc, &ib_data, &_index_buffer);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	_initialized = true;
	log_printf("スプライトバッチの初期化 >> 成功\n", LogLevel::Success);
}

void sprite_batch::draw(const sprite& target,
	float dx, float dy,
	float dw, float dh,
	const FLOAT color_rgba[4],
	float angle)
{
	draw(target, dx, dy, dw, dh, color_rgba, angle,
		0.f, 0.f,
		static_cast<float>(target.get_texture_width()),
		static_cast<float>(target.get_texture_height()));
}

void sprite_batch::draw(const sprite& target,
	float dx, float dy,
	float dw, float dh,
	const FLOAT color_rgba[4],
	float angle,
	float sx, float sy,
	float sw, float sh)
{
	if (target.get_texture() == nullptr)
	{
		return;
	}

	QueuedSprite entry{};
	entry.texture = target.get_texture();
	entry.tex_width = target.get_texture_width();
	entry.tex_height = target.get_texture_height();
	entry.dx = dx;
	entry.dy = dy;
	entry.dw = dw;
	entry.dh = dh;
	entry.color[0] = color_rgba[0];
	entry.color[1] = color_rgba[1];
	entry.color[2] = color_rgba[2];
	entry.color[3] = color_rgba[3];
	entry.angle = angle;
	entry.sx = sx;
	entry.sy = sy;
	entry.sw = sw;
	entry.sh = sh;

	_queue.push_back(entry);
}

void sprite_batch::clear()
{
	_queue.clear();
}

void sprite_batch::build_vertices(const QueuedSprite& entry, Vertex* out_vertices, float viewport_width, float viewport_height) const
{
	float x0 = entry.dx;
	float y0 = entry.dy;
	float x1 = entry.dx + entry.dw;
	float y1 = entry.dy;
	float x2 = entry.dx;
	float y2 = entry.dy + entry.dh;
	float x3 = entry.dx + entry.dw;
	float y3 = entry.dy + entry.dh;

	const float cx = entry.dx + entry.dw * 0.5f;
	const float cy = entry.dy + entry.dh * 0.5f;
	const float radians = DirectX::XMConvertToRadians(entry.angle);
	const float cos_a = cosf(radians);
	const float sin_a = sinf(radians);

	auto rotate = [&](float& x, float& y)
		{
			x -= cx;
			y -= cy;
			const float tx = x;
			const float ty = y;
			x = cos_a * tx - sin_a * ty;
			y = sin_a * tx + cos_a * ty;
			x += cx;
			y += cy;
		};

	rotate(x0, y0);
	rotate(x1, y1);
	rotate(x2, y2);
	rotate(x3, y3);

	x0 = 2.0f * x0 / viewport_width - 1.0f;
	y0 = 1.0f - 2.0f * y0 / viewport_height;
	x1 = 2.0f * x1 / viewport_width - 1.0f;
	y1 = 1.0f - 2.0f * y1 / viewport_height;
	x2 = 2.0f * x2 / viewport_width - 1.0f;
	y2 = 1.0f - 2.0f * y2 / viewport_height;
	x3 = 2.0f * x3 / viewport_width - 1.0f;
	y3 = 1.0f - 2.0f * y3 / viewport_height;

	const dx::XMFLOAT4 color(entry.color[0], entry.color[1], entry.color[2], entry.color[3]);
	const float tex_w = static_cast<float>(entry.tex_width);
	const float tex_h = static_cast<float>(entry.tex_height);
	const float u0 = entry.sx / tex_w;
	const float v0 = entry.sy / tex_h;
	const float u1 = (entry.sx + entry.sw) / tex_w;
	const float v1 = (entry.sy + entry.sh) / tex_h;

	out_vertices[0] = { { x0, y0, 0.f }, color, { u0, v0 } };
	out_vertices[1] = { { x1, y1, 0.f }, color, { u1, v0 } };
	out_vertices[2] = { { x2, y2, 0.f }, color, { u0, v1 } };
	out_vertices[3] = { { x3, y3, 0.f }, color, { u1, v1 } };
}

void sprite_batch::render_texture_batch(ID3D11DeviceContext* immediate_context,
	ID3D11ShaderResourceView* texture,
	const QueuedSprite* sprites,
	size_t count)
{
	if (count == 0 || texture == nullptr)
	{
		return;
	}

	D3D11_VIEWPORT viewport{};
	UINT num_viewports = 1;
	immediate_context->RSGetViewports(&num_viewports, &viewport);

	D3D11_MAPPED_SUBRESOURCE mapped{};
	HRESULT hr = immediate_context->Map(_vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));

	auto* vertices = reinterpret_cast<Vertex*>(mapped.pData);
	for (size_t i = 0; i < count; ++i)
	{
		build_vertices(sprites[i], vertices + i * VerticesPerSprite, viewport.Width, viewport.Height);
	}
	immediate_context->Unmap(_vertex_buffer, 0);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	immediate_context->IASetVertexBuffers(0, 1, &_vertex_buffer, &stride, &offset);
	immediate_context->IASetIndexBuffer(_index_buffer, DXGI_FORMAT_R16_UINT, 0);
	immediate_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	immediate_context->IASetInputLayout(_input_layout);
	immediate_context->VSSetShader(_vertex_shader, nullptr, 0);
	immediate_context->PSSetShader(_pixel_shader, nullptr, 0);
	immediate_context->PSSetShaderResources(0, 1, &texture);

	const UINT index_count = static_cast<UINT>(count * IndicesPerSprite);
	immediate_context->DrawIndexed(index_count, 0, 0);
}

void sprite_batch::flush(ID3D11DeviceContext* immediate_context)
{
	if (!_initialized || _queue.empty() || immediate_context == nullptr)
	{
		_queue.clear();
		return;
	}

	std::sort(_queue.begin(), _queue.end(),
		[](const QueuedSprite& a, const QueuedSprite& b)
		{
			return reinterpret_cast<std::uintptr_t>(a.texture) < reinterpret_cast<std::uintptr_t>(b.texture);
		});

	size_t batch_start = 0;
	while (batch_start < _queue.size())
	{
		const ID3D11ShaderResourceView* batch_texture = _queue[batch_start].texture;
		size_t batch_end = batch_start + 1;
		while (batch_end < _queue.size() && _queue[batch_end].texture == batch_texture)
		{
			++batch_end;
		}

		for (size_t offset = batch_start; offset < batch_end;)
		{
			const size_t chunk_count = (std::min)(MaxSpritesPerBatch, batch_end - offset);
			render_texture_batch(immediate_context, _queue[offset].texture, &_queue[offset], chunk_count);
			offset += chunk_count;
		}

		batch_start = batch_end;
	}

	_queue.clear();
}
