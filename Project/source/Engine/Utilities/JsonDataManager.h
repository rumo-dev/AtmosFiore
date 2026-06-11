#pragma once

//=============================================================================
//  JsonDataManager.h
//  JSON を使ったデータ管理クラス (DirectX11 / C++17)
//
//  依存: nlohmann/json (json.hpp) ─ ヘッダーオンリー
//  コンパイル要件: /std:c++17 以上
//=============================================================================

#include "Engine/Graphics/Model/Tiny/Json.hpp"

#include <string>
#include <vector>
#include <optional>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <functional>
#ifdef _WIN32
#  include <Windows.h>   // OutputDebugStringA
#else
   // Linux/Mac ビルドテスト用スタブ
inline void OutputDebugStringA(const char*) {}
#endif

using json = nlohmann::json;

//-----------------------------------------------------------------------------
//  ログ出力ヘルパー (VisualStudio 出力ウィンドウに表示)
//-----------------------------------------------------------------------------
namespace jdm_detail
{
	inline void DebugLog(const std::string& msg)
	{
		OutputDebugStringA(("[JsonDataManager] " + msg + "\n").c_str());
	}
}

//=============================================================================
//  class JsonDataManager
//=============================================================================
class JsonDataManager
{
public:
	//-------------------------------------------------------------------------
	//  コンストラクタ
	//  @param filepath   JSONファイルのパス。空文字ならメモリ上のみ管理。
	//  @param autoSave   true のとき set/delete 等の変更後に自動保存。
	//-------------------------------------------------------------------------
	explicit JsonDataManager(const std::string& filepath = "",
		bool autoSave = true)
		: m_filepath(filepath)
		, m_autoSave(autoSave)
	{
		if (!filepath.empty())
		{
			std::ifstream ifs(filepath);
			if (ifs.is_open())
				Load(filepath);
		}
	}

	//=========================================================================
	//  ファイル I/O
	//=========================================================================

	/// JSONファイルを読み込む
	bool Load(const std::string& filepath = "")
	{
		const std::string& path = filepath.empty() ? m_filepath : filepath;
		if (path.empty()) { jdm_detail::DebugLog("Load: filepath が空です"); return false; }

		std::ifstream ifs(path);
		if (!ifs.is_open())
		{
			jdm_detail::DebugLog("Load: ファイルを開けません: " + path);
			return false;
		}
		try
		{
			ifs >> m_data;
			if (!filepath.empty()) m_filepath = filepath;
			return true;
		}
		catch (const json::parse_error& e)
		{
			jdm_detail::DebugLog(std::string("Load: パースエラー: ") + e.what());
			return false;
		}
	}

	/// データを JSON ファイルへ書き込む
	bool Save(const std::string& filepath = "", int indent = 2) const
	{
		const std::string& path = filepath.empty() ? m_filepath : filepath;
		if (path.empty()) { jdm_detail::DebugLog("Save: filepath が空です"); return false; }

		std::ofstream ofs(path);
		if (!ofs.is_open())
		{
			jdm_detail::DebugLog("Save: ファイルを開けません: " + path);
			return false;
		}
		ofs << m_data.dump(indent);
		return true;
	}

	/// JSON 文字列からデータを読み込む
	bool LoadFromString(const std::string& jsonStr)
	{
		try
		{
			m_data = json::parse(jsonStr);
			return true;
		}
		catch (const json::parse_error& e)
		{
			jdm_detail::DebugLog(std::string("LoadFromString: パースエラー: ") + e.what());
			return false;
		}
	}

	/// 現在のデータを JSON 文字列へ変換する
	std::string Dumps(int indent = 2) const
	{
		return m_data.dump(indent);
	}

	//=========================================================================
	//  CRUD 操作  ─ ドット記法でネストキーにアクセス可能
	//  例: Get<std::string>("user.address.city")
	//=========================================================================

	/// キーで値を取得する。存在しない場合は std::nullopt を返す
	template<typename T>
	std::optional<T> Get(const std::string& key) const
	{
		const json* node = Traverse(key);
		if (!node) return std::nullopt;
		try { return node->get<T>(); }
		catch (...) { return std::nullopt; }
	}

	/// キーで値を取得する。存在しない場合は defaultValue を返す
	template<typename T>
	T GetOr(const std::string& key, T defaultValue) const
	{
		return Get<T>(key).value_or(std::move(defaultValue));
	}

	/// キーに値をセットする (ネストを自動生成)
	template<typename T>
	void Set(const std::string& key, const T& value)
	{
		SetImpl(key, value);
		MaybeSave();
	}

	/// キーを削除する。削除できた場合 true
	bool Delete(const std::string& key)
	{
		auto keys = SplitKey(key);
		json* node = &m_data;
		for (size_t i = 0; i < keys.size() - 1; ++i)
		{
			if (!node->is_object() || !node->contains(keys[i])) return false;
			node = &(*node)[keys[i]];
		}
		if (!node->is_object() || !node->contains(keys.back())) return false;
		node->erase(keys.back());
		MaybeSave();
		return true;
	}

	/// キーが存在するか確認する
	bool Exists(const std::string& key) const
	{
		return Traverse(key) != nullptr;
	}

	/// トップレベルに JSON オブジェクトをマージする
	void Merge(const json& other)
	{
		m_data.merge_patch(other);
		MaybeSave();
	}

	/// 全データを削除する
	void Clear()
	{
		m_data = json::object();
		MaybeSave();
	}

	//=========================================================================
	//  配列操作
	//=========================================================================

	/// キーが指す配列に要素を追加する (存在しない場合は新規作成)
	template<typename T>
	bool Append(const std::string& key, const T& value)
	{
		json* node = TraverseMutable(key);
		if (node == nullptr)
		{
			SetImpl(key, json::array());
			node = TraverseMutable(key);
		}
		if (!node->is_array())
		{
			jdm_detail::DebugLog("Append: '" + key + "' は配列ではありません");
			return false;
		}
		node->push_back(value);
		MaybeSave();
		return true;
	}

	/// キーが指す配列から値を削除する (最初にマッチした要素)
	template<typename T>
	bool RemoveFromArray(const std::string& key, const T& value)
	{
		json* node = TraverseMutable(key);
		if (!node || !node->is_array()) return false;

		const json jval = value;
		for (auto it = node->begin(); it != node->end(); ++it)
		{
			if (*it == jval)
			{
				node->erase(it);
				MaybeSave();
				return true;
			}
		}
		return false;
	}

	/// キーが指す配列の要素数を返す
	size_t ArraySize(const std::string& key) const
	{
		const json* node = Traverse(key);
		if (!node || !node->is_array()) return 0;
		return node->size();
	}

	//=========================================================================
	//  ユーティリティ
	//=========================================================================

	/// トップレベルのキー一覧を返す
	std::vector<std::string> Keys() const
	{
		std::vector<std::string> result;
		if (!m_data.is_object()) return result;
		for (auto& [k, _] : m_data.items())
			result.push_back(k);
		return result;
	}

	/// 全データの参照を返す (読み取り専用)
	const json& All() const { return m_data; }

	/// 全データの参照を返す (書き込み可能 ─ 直接編集後は手動で Save() を呼ぶこと)
	json& All() { return m_data; }

	//-------------------------------------------------------------------------
	//  スキーマ検証
	//  schema = { {"user.name", json::value_t::string},
	//             {"user.age",  json::value_t::number_integer} }
	//-------------------------------------------------------------------------
	struct ValidationError
	{
		std::string key;
		std::string message;
	};

	std::vector<ValidationError> ValidateSchema(
		const std::vector<std::pair<std::string, json::value_t>>& schema) const
	{
		std::vector<ValidationError> errors;
		for (auto& [key, expectedType] : schema)
		{
			const json* node = Traverse(key);
			if (!node)
			{
				errors.push_back({ key, "キーが存在しません" });
			}
			else if (node->type() != expectedType)
			{
				errors.push_back({ key,
					"型が不正: 期待=" + std::string(json(expectedType).type_name()) +
					" 実際=" + std::string(node->type_name()) });
			}
		}
		return errors;
	}

	//-------------------------------------------------------------------------
	//  DirectX11 向けヘルパー
	//  (XMFLOAT2 / XMFLOAT3 / XMFLOAT4 の読み書き)
	//-------------------------------------------------------------------------
#ifdef _DIRECTXMATH_H_

	void SetFloat2(const std::string& key, const DirectX::XMFLOAT2& v)
	{
		SetImpl(key, json::array({ v.x, v.y }));
		MaybeSave();
	}
	void SetFloat3(const std::string& key, const DirectX::XMFLOAT3& v)
	{
		SetImpl(key, json::array({ v.x, v.y, v.z }));
		MaybeSave();
	}
	void SetFloat4(const std::string& key, const DirectX::XMFLOAT4& v)
	{
		SetImpl(key, json::array({ v.x, v.y, v.z, v.w }));
		MaybeSave();
	}

	std::optional<DirectX::XMFLOAT2> GetFloat2(const std::string& key) const
	{
		const json* n = Traverse(key);
		if (!n || !n->is_array() || n->size() < 2) return std::nullopt;
		return DirectX::XMFLOAT2{ (*n)[0].get<float>(), (*n)[1].get<float>() };
	}
	std::optional<DirectX::XMFLOAT3> GetFloat3(const std::string& key) const
	{
		const json* n = Traverse(key);
		if (!n || !n->is_array() || n->size() < 3) return std::nullopt;
		return DirectX::XMFLOAT3{
			(*n)[0].get<float>(), (*n)[1].get<float>(), (*n)[2].get<float>() };
	}
	std::optional<DirectX::XMFLOAT4> GetFloat4(const std::string& key) const
	{
		const json* n = Traverse(key);
		if (!n || !n->is_array() || n->size() < 4) return std::nullopt;
		return DirectX::XMFLOAT4{
			(*n)[0].get<float>(), (*n)[1].get<float>(),
			(*n)[2].get<float>(), (*n)[3].get<float>() };
	}

#endif // _DIRECTXMATH_H_

	//=========================================================================
	//  プロパティ
	//=========================================================================
	void        SetFilepath(const std::string& p) { m_filepath = p; }
	std::string GetFilepath()  const { return m_filepath; }
	void        SetAutoSave(bool v) { m_autoSave = v; }
	bool        IsAutoSave()   const { return m_autoSave; }

private:
	//=========================================================================
	//  内部ヘルパー
	//=========================================================================

	static std::vector<std::string> SplitKey(const std::string& key)
	{
		std::vector<std::string> result;
		std::stringstream ss(key);
		std::string token;
		while (std::getline(ss, token, '.'))
			if (!token.empty()) result.push_back(token);
		return result;
	}

	const json* Traverse(const std::string& key) const
	{
		const json* node = &m_data;
		for (auto& k : SplitKey(key))
		{
			if (!node->is_object() || !node->contains(k)) return nullptr;
			node = &(*node)[k];
		}
		return node;
	}

	json* TraverseMutable(const std::string& key)
	{
		json* node = &m_data;
		for (auto& k : SplitKey(key))
		{
			if (!node->is_object() || !node->contains(k)) return nullptr;
			node = &(*node)[k];
		}
		return node;
	}

	template<typename T>
	void SetImpl(const std::string& key, const T& value)
	{
		auto keys = SplitKey(key);
		json* node = &m_data;
		for (size_t i = 0; i < keys.size() - 1; ++i)
		{
			if (!node->contains(keys[i]) || !(*node)[keys[i]].is_object())
				(*node)[keys[i]] = json::object();
			node = &(*node)[keys[i]];
		}
		(*node)[keys.back()] = value;
	}

	void MaybeSave() const
	{
		if (m_autoSave && !m_filepath.empty())
			Save();
	}

	//=========================================================================
	//  メンバ変数
	//=========================================================================
	std::string m_filepath;
	bool        m_autoSave;
	json        m_data = json::object();
};
