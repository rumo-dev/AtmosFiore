#pragma once
#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
//#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"
#include "Engine/Graphics/Shader/shader.h"
#include "Engine/Graphics/UI/ImGui/imgui.h"

#include "Game/Effect/bloom/Bloom.h"
#include "Game/Effect/fog/fog.h"
#include "Game/Effect/shadow/shadow.h"
#include "Game/Effect/adaptation/adaptation.h"
#include "Game/Effect/tone_mapping/toon_mapping.h"

/**
 * @brief ポストプロセス管理クラス
 *
 * レンダリング結果に対して各種ポストエフェクトを適用する。
 *
 * @remark
 * - フレームバッファに一度描画 → 各エフェクトを順に適用
 * - 最終結果をバックバッファへ出力
 *
 * 処理フロー：
 * @code
 * begin()
 *   ↓
 * シーン描画
 *   ↓
 * end()
 *   ↓
 * render()  // ポストエフェクト適用
 *   ↓
 * draw()    // 最終出力
 * @endcode
 *
 * @warning
 * - SRVとRTVの同時バインドは禁止（D3D11制約）
 * - エフェクト順序により結果が大きく変わる
 *
 * @todo
 * - エフェクトのチェーン化（柔軟な順序制御）
 * - HDR対応（トーンマッピング）
 * - Motion Blur / DOF追加
 * - マルチレンダーターゲット対応
 */
class Post_Process_Manager {
public:
	Post_Process_Manager() {};
	~Post_Process_Manager() {};

	/**
	 * @brief 初期化
	 *
	 * フレームバッファおよび各エフェクトを生成する
	 */
	static void initialize();

	/**
	 * @brief 更新処理
	 *
	 * @param elapsedtime 経過時間
	 */
	static void update(float elapsedtime);

	/**
	 * @brief ポストプロセス開始
	 *
	 * フレームバッファへ描画を切り替える
	 *
	 * @pre
	 * 通常のバックバッファ描画から呼び出すこと
	 */
	static void begin();

	/**
	 * @brief ポストプロセス終了
	 *
	 * フレームバッファへの描画を終了する
	 */
	static void end();

	/**
	 * @brief 最終描画
	 *
	 * ポストプロセス後の結果を画面へ描画する
	 */
	static void draw();

	/**
	 * @brief ポストエフェクト適用
	 *
	 * Bloom / Fog / Shadow 等を順に適用する
	 *
	 * @note
	 * エフェクトの順序は結果に大きく影響する
	 */
	static void render();

	/**
	 * @brief ImGui描画
	 *
	 * 各エフェクトのパラメータ調整UIを表示
	 */
	static void renderGUI();

public:
	/**
	 * @brief Bloom取得
	 */
	bloom& GetBloom() {
		return *bloomer;
	};

	/**
	 * @brief Fog取得
	 */
	Fog& GetFog() {
		return *fogger;
	};

	/**
	 * @brief Shadow取得
	 */
	shadow& GetShadow() {
		return *shadower;
	};
	/**
	 * @brief 自動露出取得
	 */
	Adaptation& GetAdaptation() {
		return *adaptation;
	};

	/**
	 * @brief トーンマッピング取得
	 */
	ToneMapping& GetToneMapping() {
		return *tone_mapper;
	};

private:
	/// フルスクリーン描画用フレームバッファ
	static Framebuffer fsquad;

	//static std::unique_ptr<Fullscreen_Quad> bit_block_transfer;

	/// Bloomエフェクト
	static std::unique_ptr<bloom> bloomer;

	/// Fogエフェクト
	static std::unique_ptr<Fog> fogger;

	/// Shadowエフェクト
	static std::unique_ptr<shadow> shadower;

	/// 自動露出エフェクト
	static std::unique_ptr<Adaptation> adaptation;

	/// トーンマッピングエフェクト
	static std::unique_ptr<ToneMapping> tone_mapper;

	/// 経過時間
	float time = 0.0f;
};