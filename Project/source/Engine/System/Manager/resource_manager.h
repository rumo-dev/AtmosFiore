#pragma once
#include "Engine/System/Manager/Shader/shader_manager.h"
#include "Engine/System/Manager/Model/model_manager.h"

/**
 * @brief 各種リソース管理クラス（シングルトン）
 *
 * @details
 * アプリケーション全体で使用するリソース（Shader / Model など）を
 * 一元管理するファサードクラス。
 *
 * 内部では個別のマネージャ（ShaderManager, ModelManager）に処理を委譲する。
 *
 * @note
 * - シングルトンとして設計されており、グローバルに1インスタンスのみ存在
 * - 各マネージャの「まとめ役」であり、実際の処理は委譲している
 * - 初期ロードや一括リロードの入口として使用する想定
 *
 * - 拡張時は以下を追加するだけで統一管理できる：
 *   - TextureManager
 *   - MaterialManager
 *   - AnimationManager など
 *
 * @warning
 * - 初期化順序に依存します：
 *   Graphics_Core などが初期化される前に load_all() を呼ぶと失敗します
 *
 * - 各マネージャのライフタイムは本クラスに依存します
 *   （外部でポインタを保持し続ける場合は注意）
 *
 * - スレッドセーフではありません
 *
 * @code
 * // 初期化
 * auto& rm = Resource_Manager::instance();
 *
 * rm.load_all();
 *
 * // 個別ロードも可能
 * rm.load_shaders();
 * rm.load_models();
 *
 * // 使用例（Shader）
 * auto vs = rm.shader_manager.Get<Vertex_Shader>("BasicVS");
 *
 * // ImGui デバッグUI
 * rm.draw_imgui();
 * @endcode
 */
class Resource_Manager {
public:

	/**
	 * @brief インスタンス取得
	 * @return シングルトンインスタンス
	 */
	static Resource_Manager& instance() {
		static Resource_Manager inst;
		return inst;
	}

	/// シェーダーマネージャ
	ShaderManager shader_manager;

	/// モデルマネージャ
	ModelManager model_manager;

	/**
	 * @brief 全リソースをロード
	 *
	 * @note
	 * 内部で load_shaders() / load_models() を呼び出す想定
	 */
	void load_all();

	/**
	 * @brief シェーダーのみロード
	 */
	void load_shaders();

	/**
	 * @brief モデルのみロード
	 */
	void load_models();

	/**
	 * @brief ImGui デバッグUI描画
	 *
	 * @details
	 * - Shader 一覧 + リロード
	 * - Model 一覧
	 * - Texture は未実装（プレースホルダ）
	 */
	void draw_imgui()
	{
#ifdef USE_IMGUI

		if (ImGui::CollapsingHeader("Shaders"))
			shader_manager.draw_imgui();

		if (ImGui::CollapsingHeader("Textures"))
			ImGui::Text("Texture viewer is not implemented yet.");

		if (ImGui::CollapsingHeader("Models"))
			model_manager.draw_imgui();

#endif  // USE_IMGUI
	}

private:
	/// コンストラクタ（外部生成禁止）
	Resource_Manager() = default;

	/// コピー禁止
	Resource_Manager(const Resource_Manager&) = delete;

	/// 代入禁止
	Resource_Manager& operator=(const Resource_Manager&) = delete;
};