#pragma once
#include <d3d11.h>
#include <vector>
#include <cstdint>
#include "Engine/Utilities/dx_common.h"

class sprite;

/**
 * @brief スプライトバッチ描画（DrawCall削減）
 *
 * 同一テクスチャのスプライトをまとめて1回の DrawIndexed に集約する。
 * draw() でキューに積み、flush() で描画する。
 */
class sprite_batch
{
public:
	static sprite_batch& instance()
	{
		static sprite_batch inst;
		return inst;
	}

	/// GPUリソース初期化（Graphics_Core 初期化時に1回呼ぶ）
	void initialize(ID3D11Device* device);

	/// スプライトを描画キューに追加
	void draw(const sprite& target,
		float dx, float dy,
		float dw, float dh,
		const FLOAT color_rgba[4],
		float angle);

	void draw(const sprite& target,
		float dx, float dy,
		float dw, float dh,
		const FLOAT color_rgba[4],
		float angle,
		float sx, float sy,
		float sw, float sh);

	/// キューに溜まったスプライトを描画（テクスチャ単位でバッチ）
	void flush(ID3D11DeviceContext* immediate_context);

	/// キューを破棄（描画しない）
	void clear();

	bool is_initialized() const { return _initialized; }

	sprite_batch(const sprite_batch&) = delete;
	sprite_batch& operator=(const sprite_batch&) = delete;

private:
	sprite_batch() = default;

	static constexpr size_t MaxSpritesPerBatch = 2048;
	static constexpr size_t VerticesPerSprite = 4;
	static constexpr size_t IndicesPerSprite = 6;

	struct Vertex
	{
		dx::XMFLOAT3 position;
		dx::XMFLOAT4 color;
		dx::XMFLOAT2 texcoord;
	};

	struct QueuedSprite
	{
		ID3D11ShaderResourceView* texture = nullptr;
		UINT tex_width = 0;
		UINT tex_height = 0;
		float dx = 0.f;
		float dy = 0.f;
		float dw = 0.f;
		float dh = 0.f;
		float color[4]{ 1.f, 1.f, 1.f, 1.f };
		float angle = 0.f;
		float sx = 0.f;
		float sy = 0.f;
		float sw = 0.f;
		float sh = 0.f;
	};

	void build_vertices(const QueuedSprite& entry, Vertex* out_vertices, float viewport_width, float viewport_height) const;
	void render_texture_batch(ID3D11DeviceContext* immediate_context,
		ID3D11ShaderResourceView* texture,
		const QueuedSprite* sprites,
		size_t count);

	bool _initialized = false;
	ID3D11VertexShader* _vertex_shader = nullptr;
	ID3D11PixelShader* _pixel_shader = nullptr;
	ID3D11InputLayout* _input_layout = nullptr;
	ID3D11Buffer* _vertex_buffer = nullptr;
	ID3D11Buffer* _index_buffer = nullptr;

	std::vector<QueuedSprite> _queue;
};
