#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <type_traits>
#include <wrl/client.h>
#include "Engine/Graphics/UI/ImGui/imgui.h"

#include "Engine/utilities/misc.h"
#include "shader_base.h"
#include "vertex.h"
#include "pixel.h"
#include "geometry.h"
#include "hull.h"
#include "domain.h"
#include "compute.h"

using Microsoft::WRL::ComPtr;

/**
 * @brief シェーダー型ごとのデフォルト target を提供する traits
 * @tparam T シェーダークラス型
 */
template<typename T>
struct ShaderTargetTraits {
	/**
	 * @brief デフォルトのシェーダーターゲットを取得
	 * @return ターゲット文字列（未定義の場合は空文字）
	 */
	static std::string GetDefault() { return ""; }  ///< fallback
};

/// @name 各シェーダー型のターゲット定義
/// @{
template<> struct ShaderTargetTraits<Vertex_Shader> { static std::string GetDefault() { return "vs_5_0"; } };
template<> struct ShaderTargetTraits<Pixel_Shader> { static std::string GetDefault() { return "ps_5_0"; } };
template<> struct ShaderTargetTraits<Geometry_Shader> { static std::string GetDefault() { return "gs_5_0"; } };
template<> struct ShaderTargetTraits<Hull_Shader> { static std::string GetDefault() { return "hs_5_0"; } };
template<> struct ShaderTargetTraits<Domain_Shader> { static std::string GetDefault() { return "ds_5_0"; } };
template<> struct ShaderTargetTraits<Compute_Shader> { static std::string GetDefault() { return "cs_5_0"; } };
/// @}

/**
 * @brief シェーダーのロード・管理を行うクラス
 *
 * @note
 * - 本クラスは「名前ベースでのシェーダー共有」を目的としたマネージャです。
 * - 同じ name を指定した場合、同一インスタンスが返されます（キャッシュ機構）。
 * - テンプレートによりシェーダー型（VS/PSなど）を静的に扱える設計になっています。
 * - 実体は Shader_Base を基底としたポリモーフィズムで保持されています。
 *
 * @warning
 * - load<T>() と Get<T>() の T が一致しない場合、
 *   dynamic_pointer_cast により nullptr が返ります。
 *   （例: Vertex_Shader としてロード → Pixel_Shader として取得）
 *
 * - 名前(name)は型安全ではありません。
 *   同じ name に異なる型を割り当てるとバグの原因になります。
 *
 * - GetNative<T>() は内部ポインタを直接返すため、
 *   ライフタイムは ShaderManager に依存します。
 *
 * @code
 * ShaderManager shaderManager;
 *
 * // フォルダ設定
 * shaderManager.SetShaderFolders(L"./hlsl/", L"./cso/");
 *
 * // ロード
 * auto vs = shaderManager.load<Vertex_Shader>(
 *     "BasicVS",
 *     "basic_vertex",
 *     "main",
 *     "vs_5_0"
 * );
 *
 * auto ps = shaderManager.load<Pixel_Shader>(
 *     "BasicPS",
 *     "basic_pixel"
 * );
 *
 * // 取得（キャッシュから）
 * auto vs2 = shaderManager.Get<Vertex_Shader>("BasicVS");
 *
 * // ネイティブポインタ取得（DirectX用）
 * ID3D11VertexShader* nativeVS =
 *     shaderManager.GetNative<Vertex_Shader>("BasicVS");
 *
 * // 全リロード（ホットリロード）
 * shaderManager.ReloadAll();
 * @endcode
 */
class ShaderManager {
public:

	/**
	 * @brief シェーダーフォルダを設定
	 * @param hlslDir HLSLファイルディレクトリ
	 * @param csoDir  コンパイル済みCSOディレクトリ
	 */
	void SetShaderFolders(const std::wstring& hlslDir, const std::wstring& csoDir) {
		m_hlslDir = hlslDir;
		m_csoDir = csoDir;
	}

	/**
	 * @brief シェーダーをロードまたは取得
	 * @tparam T シェーダー型
	 * @param name 管理用の名前
	 * @param fileName ファイル名（拡張子なし）
	 * @param entry エントリポイント（省略時 "main"）
	 * @param target シェーダーモデル（省略時 traits に依存）
	 * @return シェーダーの共有ポインタ（失敗時 nullptr）
	 * 
	 * @warning
	 * - 同じ name に対して異なる型 T を指定すると未定義的な使い方になります。
	 *   必ず同一 name には同一型を使用してください。
	 */
	template<typename T>
	std::shared_ptr<T> load(
		const std::string& name,
		const std::string& fileName,
		const std::string& entry = "",
		const std::string& target = "")
	{
		// 既にロード済みならそれを返す
		if (m_shaders.find(name) != m_shaders.end()) {
			return std::static_pointer_cast<T>(m_shaders[name]);
		}

		auto shader = std::make_shared<T>();

		// デフォルト設定
		std::string entryPoint = entry.empty() ? "main" : entry;
		std::string shaderTarget = target.empty() ? ShaderTargetTraits<T>::GetDefault() : target;

		// フルパス構築
		std::wstring hlslPath = m_hlslDir + s2ws(fileName) + L".hlsl";
		std::wstring csoPath = m_csoDir + s2ws(fileName) + L".cso";

		log_printf("Loading Shader: %s\n", LogLevel::Info, name.c_str());
		log_printf(" HLSL Path: %ls\n", LogLevel::Info, hlslPath.c_str());
		log_printf(" CSO Path: %ls\n", LogLevel::Info, csoPath.c_str());
		log_printf(" Entry Point: %s\n", LogLevel::Info, entryPoint.c_str());
		log_printf(" Shader Target: %s\n", LogLevel::Info, shaderTarget.c_str());

		if (!shader->load(hlslPath, csoPath, entryPoint, shaderTarget)) {
			return nullptr;
		}

		m_shaders[name] = shader;
		return shader;
	}

	/**
	 * @brief シェーダーを取得
	 * @tparam T シェーダー型
	 * @param name 管理名
	 * @return 型安全にキャストされたシェーダー（存在しない場合 nullptr）
	 */
	template<typename T>
	std::shared_ptr<T> Get(const std::string& name)
	{
		auto it = m_shaders.find(name);
		if (it == m_shaders.end()) {
			return nullptr;
		}

		return std::dynamic_pointer_cast<T>(it->second);
	}

	/**
	 * @brief ネイティブシェーダーポインタを取得
	 * @tparam T シェーダー型
	 * @param name 管理名
	 * @return ID3D11XXXShader*（存在しない場合 nullptr）
	 */
	template<typename T>
	auto GetNative(const std::string& name)
	{
		auto shader = Get<T>(name);
		if (!shader) return decltype(shader->get())(nullptr);
		return shader->get();
	}

	/**
	 * @brief 全シェーダーをリロード
	 */
	void ReloadAll() {
		for (auto& [name, shader] : m_shaders) {
			if (!shader->reload()) {
				std::cerr << "Failed to reload shader: " << name << std::endl;
			}
		}
	}

	/**
	 * @brief ImGui を使用したシェーダー管理UIを描画
	 *
	 * - 種類ごとに分類表示
	 * - 個別リロードボタン
	 * - グループ単位リロード対応
	 */
	void draw_imgui()
	{
#ifdef USE_IMGUI

		std::unordered_map<std::string, std::vector<std::string>> shaderGroups;

		// グループ分類
		for (auto& pair : m_shaders)
		{
			const std::string& name = pair.first;
			Shader_Base* shader = pair.second.get();

			std::string typeName = "Unknown";

			if (dynamic_cast<Vertex_Shader*>(shader))        typeName = "Vertex Shader";
			else if (dynamic_cast<Pixel_Shader*>(shader))    typeName = "Pixel Shader";
			else if (dynamic_cast<Geometry_Shader*>(shader)) typeName = "Geometry Shader";
			else if (dynamic_cast<Hull_Shader*>(shader))     typeName = "Hull Shader";
			else if (dynamic_cast<Domain_Shader*>(shader))   typeName = "Domain Shader";
			else if (dynamic_cast<Compute_Shader*>(shader))  typeName = "Compute Shader";

			shaderGroups[typeName].push_back(name);
		}

		// 描画
		for (auto& group : shaderGroups)
		{
			const std::string& typeName = group.first;
			auto& list = group.second;

			std::string header = typeName + " (" + std::to_string(list.size()) + ")";
			if (ImGui::TreeNode(header.c_str()))
			{
				if (ImGui::BeginTable("ShaderTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
				{
					ImGui::TableSetupColumn("Shader Name");
					ImGui::TableSetupColumn("Actions");
					ImGui::TableHeadersRow();

					for (auto& shaderName : list)
					{
						Shader_Base* shader = m_shaders[shaderName].get();

						ImGui::TableNextRow();

						ImGui::TableSetColumnIndex(0);
						ImGui::Text("%s", shaderName.c_str());

						ImGui::TableSetColumnIndex(1);
						std::string buttonLabel = "Reload##" + shaderName;

						if (ImGui::Button(buttonLabel.c_str()))
						{
							if (!shader->reload())
							{
								std::cerr << "Failed to reload shader: " << shaderName << std::endl;
							}
						}
					}

					ImGui::EndTable();
				}

				ImGui::TreePop();
			}

			// グループ単位リロード
			std::string reloadAllLabel = "Reload All " + typeName;
			if (ImGui::Button(reloadAllLabel.c_str()))
			{
				for (auto& shaderName : list)
				{
					Shader_Base* shader = m_shaders[shaderName].get();
					shader->reload();
				}
			}
		}

#endif
	}

private:
	std::wstring m_hlslDir;  ///< HLSLディレクトリ
	std::wstring m_csoDir;   ///< CSOディレクトリ

	/**
	 * @brief シェーダー格納マップ
	 * key: 名前
	 * value: Shader_Base 派生インスタンス
	 */
	std::unordered_map<std::string, std::shared_ptr<Shader_Base>> m_shaders;
};