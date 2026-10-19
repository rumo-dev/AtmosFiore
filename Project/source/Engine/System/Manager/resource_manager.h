#pragma once
#include "Engine/System/Manager/Shader/shader_manager.h"
#include "Engine/System/Manager/Model/model_manager.h"
#include <memory>
#include <string>
#include <cstddef>
#include <utility>
#include <d3d11.h>

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
 * auto vs = rm.get_shader<Vertex_Shader>("BasicVS");
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

	template<typename T>
	std::shared_ptr<T> load_shader(const std::string& name, const std::string& file,
		const std::string& entry = "", const std::string& target = "") {
		return shader_manager_.load<T>(name, file, entry, target);
	}

	template<typename T>
	std::shared_ptr<T> get_shader(const std::string& name) {
		return shader_manager_.Get<T>(name);
	}

	template<typename T>
	auto get_native_shader(const std::string& name) {
		return shader_manager_.GetNative<T>(name);
	}

	template<typename T>
	std::shared_ptr<T> load_shader_file(const std::string& fileName) {
		return shader_manager_.LoadByFile<T>(fileName);
	}

	template<typename T>
	std::shared_ptr<T> get_shader_file(const std::string& fileName) {
		return shader_manager_.GetByFile<T>(fileName);
	}

	Gltf_Model* load_model(const std::string& key, const std::string& path) {
		return model_manager_.load(key, path);
	}
	Gltf_Model* get_model(const std::string& key) { return model_manager_.get_model(key); }
	void add_model_instance(const std::string& key, ModelInstance instance) { model_manager_.add_instance(key, std::move(instance)); }
	ModelInstance* get_model_instance(const std::string& key) { return model_manager_.get_instance(key); }
	void remove_model_instance(const std::string& key) { model_manager_.remove_instance(key); }
	void update_models(float delta_time) { model_manager_.update(delta_time); }
	void render_models(pass_mode pass) { model_manager_.render_all(pass); }
	void draw_shader_debug() { shader_manager_.draw_imgui(); }
	void draw_model_debug() { model_manager_.draw_imgui(); }
	HRESULT load_texture(const std::wstring& path, ID3D11ShaderResourceView** view,
		D3D11_TEXTURE2D_DESC* desc = nullptr);
	HRESULT load_texture_memory(const void* data, size_t size, ID3D11ShaderResourceView** view);

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

	// Release cache entries that have no live users and report how many were freed.
	size_t unload_unused();
	void unload_all();

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

		if (ImGui::Button("Release unused resources"))
			unload_unused();
		ImGui::SameLine();
		if (ImGui::Button("Release all resources"))
			unload_all();
		ImGui::Text("Cached shaders: %zu | models: %zu | instances: %zu",
			shader_manager_.GetLoadedCount(), model_manager_.get_loaded_model_count(), model_manager_.get_instance_count());

		if (ImGui::CollapsingHeader("Shaders"))
			shader_manager_.draw_imgui();

		if (ImGui::CollapsingHeader("Textures"))
			ImGui::Text("Texture viewer is not implemented yet.");

		if (ImGui::CollapsingHeader("Models"))
			model_manager_.draw_imgui();

#endif  // USE_IMGUI
	}

private:
	/// 具体的なキャッシュへのアクセスは Resource_Manager に集約する
	ShaderManager shader_manager_;
	ModelManager model_manager_;
	bool shaders_loaded_ = false;

	/// コンストラクタ（外部生成禁止）
	Resource_Manager();

	/// コピー禁止
	Resource_Manager(const Resource_Manager&) = delete;

	/// 代入禁止
	Resource_Manager& operator=(const Resource_Manager&) = delete;
};
