#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include "Engine/Utilities/dx_common.h"
#include "Engine/Utilities/misc.h"

/**
 * @brief スプライト描画クラス?�?2D?�?
 *
 * �?クスチャをスクリーン空間に矩形として描画するクラス�?
 * UI�?ビルボ�?�ド、簡�?2D描画に使用する�?
 *
 * @remark
 * - スクリーン座標系?��ピクセル単位）で描画
 * - 単一�?クスチャ?��カラー乗算�?�み対�?
 *
 * @warning
 * - SRVとして使用中の�?クスチャをRTVとして同時にバインドしてはならな�??�?D3D11制�??�?
 * - アルファブレンド設定�?�外部で適�?に行う�?要がある
 *
 * @todo
 * - バッチ描画対応�?DrawCall削減�?
 * - UV回転・ミラーリング対�?
 * - アトラス対�?
 * - インスタンシング対�?
 */
class sprite
{
private:
	/// 頂点シェーダー
	ID3D11VertexShader* _vertex_shader;

	/// ピクセルシェーダー
	ID3D11PixelShader* _pixel_shader;

	/// 入力レイアウ�?
	ID3D11InputLayout* _input_layout;

	/// 頂点バッファ?��矩形?�?
	ID3D11Buffer* _vertex_buffer;

	/// �?クスチャ
	ID3D11ShaderResourceView* _shader_resource_view;

	/// �?クスチャ�?報
	D3D11_TEXTURE2D_DESC _texture2d_desc;

	/**
	 * @brief 頂点構�?�?
	 */
	struct Vertex
	{
		dx::XMFLOAT3 position; ///< 頂点位置?��スクリーン空間�?
		dx::XMFLOAT4 color;    ///< 頂点カラー
		dx::XMFLOAT2 texcoord; ///< UV座�?
	};

	/// 頂点バッファ生�??
	bool create_vs_buffer_object(ID3D11Device* device);

	/// 頂点シェーダーと入力レイアウト生�?
	bool create_vs_shader_object_and_input_layout(ID3D11Device* device);

	/// ピクセルシェーダー生�??
	bool create_ps_shader_object(ID3D11Device* device);

public:
	/**
	 * @brief コンストラクタ
	 *
	 * �?クスチャをロードし、描画に�?要なGPUリソースを生成する�?
	 *
	 * @param device D3D11�?バイス
	 * @param filename �?クスチャファイルパス
	 *
	 * @note
	 * �?クスチャサイズは�?部で取得され、UV計算に使用され�?
	 */
	sprite(ID3D11Device* device, const wchar_t* filename);

	/**
	 * @brief �?ストラクタ
	 */
	~sprite()
	{
		log_printf("スプライト消去\n", LogLevel::Info);
	}

	/// コピ�?�禁止
	sprite& operator=(const sprite&) = delete;
	sprite(const sprite&) = delete;

	/**
	 * @brief スプライト描画?���?�体表示?�?
	 *
	 * @param immediate_context �?バイスコン�?キス�?
	 * @param dx 左上X座標（スクリーン?�?
	 * @param dy 左上Y座標（スクリーン?�?
	 * @param dw �?
	 * @param dh 高さ
	 * @param ColorRGBA 色?��乗算�?
	 * @param angle 回転角（度?�?
	 *
	 * @pre
	 * - 適�?なビューポ�?�トが設定されて�?ること
	 * - アルファブレンドス�?ートが�?要に応じて設定されて�?ること
	 */
	void render(ID3D11DeviceContext* immediate_context,
		float dx, float dy,
		float dw, float dh,
		const FLOAT ColorRGBA[4],
		float angle);

	/**
	 * @brief スプライト描画?��部�?表示?�?
	 *
	 * �?クスチャの一部領域を描画する?��スプライトシート対応）�?
	 *
	 * @param immediate_context �?バイスコン�?キス�?
	 * @param dx 左上X座�?
	 * @param dy 左上Y座�?
	 * @param dw �?
	 * @param dh 高さ
	 * @param ColorRGBA 色
	 * @param angle 回転角（度?�?
	 * @param sx UV開始X?��ピクセル?�?
	 * @param sy UV開始Y?��ピクセル?�?
	 * @param sw UV�??��ピクセル?�?
	 * @param sh UV高さ?��ピクセル?�?
	 *
	 * @note
	 * UVはピクセル単位で�?定され、�??部で正規化され�?
	 */
	void render(ID3D11DeviceContext* immediate_context,
		float dx, float dy,
		float dw, float dh,
		const FLOAT ColorRGBA[4],
		float angle,
		float sx, float sy,
		float sw, float sh);

	/// sprite_batch �p: �o�C���h�ς݃e�N�X�`�� SRV
	ID3D11ShaderResourceView* get_texture() const { return _shader_resource_view; }

	/// sprite_batch �p: �e�N�X�`�����i�s�N�Z���j
	UINT get_texture_width() const { return _texture2d_desc.Width; }

	/// sprite_batch �p: �e�N�X�`�������i�s�N�Z���j
	UINT get_texture_height() const { return _texture2d_desc.Height; }
};
