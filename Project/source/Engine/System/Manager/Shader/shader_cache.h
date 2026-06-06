#pragma once

#include <string>
#include <unordered_map>
#include <wrl/client.h>
#include <d3dcommon.h>
#include <mutex>

using Microsoft::WRL::ComPtr;

/**
 * @brief シェーダーバイナリキャッシュクラス
 *
 * コンパイル済みシェーダーバイナリ（ID3DBlob）をキャッシュし、
 * 再コンパイルや再読み込みを防ぐ。
 *
 * @remark
 * キャッシュキーは通常「ファイルパス + エントリーポイント + ターゲット」で管理することを推奨。
 *
 * @note
 * - ComPtrにより参照カウント管理されるため安全に共有可能
 * - 同一シェーダーの複数回コンパイルを防ぐことでロード時間を短縮
 *
 * @warning
 * - 現在の実装ではキーがファイルパスのみのため、異なるエントリーポイントやターゲットを区別できない
 * - スレッドセーフではない（必要ならロックを追加）
 *
 * @todo
 * - キーに entry / target を含める
 * - ファイル更新検知（ホットリロード）
 * - スレッドセーフ対応
 */
class Shader_Cache {
public:

	/**
	 * @brief キャッシュへ保存
	 *
	 * @param key キャッシュキー（推奨：path + entry + target）
	 * @param blob コンパイル済みシェーダーバイナリ
	 */
	void Store(const std::wstring& key, ComPtr<ID3DBlob> blob) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_cache[key] = blob;
	}

	/**
	 * @brief キャッシュから取得
	 *
	 * @param key キャッシュキー
	 * @return ID3DBlob（存在しない場合は nullptr）
	 */
	ComPtr<ID3DBlob> Get(const std::wstring& key) {
		std::lock_guard<std::mutex> lock(m_mutex);

		auto it = m_cache.find(key);
		if (it == m_cache.end()) return nullptr;
		return it->second;
	}

	/**
	 * @brief キャッシュ削除
	 *
	 * @param key キャッシュキー
	 */
	void Remove(const std::wstring& key) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_cache.erase(key);
	}

	/**
	 * @brief 全キャッシュクリア
	 */
	void Clear() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_cache.clear();
	}

private:
	/// キャッシュ本体
	std::unordered_map<std::wstring, ComPtr<ID3DBlob>> m_cache;

	/// スレッドセーフ用ロック
	std::mutex m_mutex;
};