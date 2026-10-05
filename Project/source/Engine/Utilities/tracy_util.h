#pragma once

#include <d3d11.h>

// =================================================================
// Tracy が有効な場合のみ本実装を使用し、無効な場合は no-op スタブにする
// =================================================================
#if defined(TRACY_ENABLE)

#include "Engine/Extentions/Tracy/tracy/Tracy.hpp"
#include "Engine/Extentions/Tracy/tracy/TracyD3D11.hpp"

// =================================================================
// プロファイリング分類 (カテゴリ) の定義
// =================================================================
enum class TracyCategory : uint32_t {
	System,         // システム・メインループ (オレンジ)
	Update,         // 更新ロジック全般 (金色)
	RenderPass,     // 主要なレンダリングパス全般 (ロイヤルブルー)
	Geometry,       // 行列・メッシュジオメトリ描画 (エメラルドグリーン)
	Shadow,         // シャドウマップ関連処理 (ゴールド/ダーク系)
	PostProcess,    // ポストエフェクト関連 (マゼンタ)
	UI,             // ImGuiやゲーム内UI (ホットピンク)
	Audio,          // オーディオシステム処理 (シアン/水色)
	Wait,           // VSyncや同期待ち処理 (ダークゴールド)
	ConstantBuffer  // 定数バッファ更新・転送処理 (ライム)
};

// カテゴリに対応する色を返すヘルパー関数
constexpr uint32_t GetCategoryColor(TracyCategory category) {
	switch (category) {
	case TracyCategory::System:      return 0xFF8C00; // DarkOrange
	case TracyCategory::Update:      return 0xFFD700; // Gold
	case TracyCategory::RenderPass:  return 0x4169E1; // RoyalBlue
	case TracyCategory::Geometry:    return 0x50C878; // Emerald Green
	case TracyCategory::Shadow:      return 0xDAA520; // Goldenrod
	case TracyCategory::PostProcess: return 0xFF00FF; // Magenta
	case TracyCategory::UI:          return 0xFF69B4; // HotPink
	case TracyCategory::Audio:       return 0x00FFFF; // Cyan
	case TracyCategory::Wait:           return 0xB8860B; // DarkGoldenrod
	case TracyCategory::ConstantBuffer: return 0x32CD32; // LimeGreen
	default:                            return 0xFFFFFF; // White
	}
}

class TracyManager {
public:
	static TracyManager& instance() {
		static TracyManager instance;
		return instance;
	}

	TracyManager(const TracyManager&) = delete;
	TracyManager& operator=(const TracyManager&) = delete;

	void initialize(ID3D11Device* device, ID3D11DeviceContext* context) {
		if (!m_initialized) {
			m_tracyCtx = TracyD3D11Context(device, context);
			m_initialized = true;
		}
	}

	void finalize() {
		if (m_initialized && m_tracyCtx) {
			TracyD3D11Destroy(m_tracyCtx);
			m_tracyCtx = nullptr;
			m_initialized = false;
		}
	}

	void collect() {
		if (m_initialized && m_tracyCtx) {
			TracyD3D11Collect(m_tracyCtx);
		}
	}

	TracyD3D11Ctx get_context() const { return m_tracyCtx; }
	bool is_initialized() const { return m_initialized; }

private:
	TracyManager() : m_tracyCtx(nullptr), m_initialized(false) {}
	~TracyManager() { finalize(); }

	TracyD3D11Ctx m_tracyCtx;
	bool          m_initialized;
};

// =================================================================
// 1. CPU プロファイリング用ショートカットマクロ
// =================================================================
#define TRACY_CPU_ZONE ZoneScoped

#define TRACY_CPU_ZONE_N(name) ZoneScopedN(name)

#define TRACY_CPU_ZONE_C(name, category) \
    ZoneScopedNC(name, GetCategoryColor(category))

#define TRACY_CPU_ZONE_DYN(name_str) \
    ZoneScoped; \
    ZoneTransient(___tracy_scoped_zone, name_str, true)

#define TRACY_CPU_TEXT(text_str, size) ZoneText(text_str, size)

#define TRACY_CB_ZONE(name) \
    TRACY_CPU_ZONE_C(name, TracyCategory::ConstantBuffer)

#define TRACY_CB_ZONE_DYN(name_str) \
    TRACY_CPU_ZONE_DYN(name_str)

// =================================================================
// 2. GPU プロファイリング用ショートカットマクロ
// =================================================================
#define TRACY_GPU_ZONE(name) \
    TracyD3D11Zone(TracyManager::instance().get_context(), name)

#define TRACY_GPU_ZONE_C(name, category) \
    TracyD3D11ZoneC(TracyManager::instance().get_context(), name, GetCategoryColor(category))

#define TRACY_GPU_ZONE_DYN(name, str, len) \
    TracyD3D11ZoneDyn(TracyManager::instance().get_context(), name, str, len)

#else // !TRACY_ENABLE

// =================================================================
// Tracy 無効時のスタブ実装 (何もしない / エラーを出さない)
// =================================================================
enum class TracyCategory : uint32_t {
	System,
	Update,
	RenderPass,
	Geometry,
	Shadow,
	PostProcess,
	UI,
	Audio,
	Wait,
	ConstantBuffer
};

constexpr uint32_t GetCategoryColor(TracyCategory) {
	return 0xFFFFFF;
}

class TracyManager {
public:
	static TracyManager& instance() {
		static TracyManager instance;
		return instance;
	}

	TracyManager(const TracyManager&) = delete;
	TracyManager& operator=(const TracyManager&) = delete;

	void initialize(ID3D11Device*, ID3D11DeviceContext*) {}
	void finalize() {}
	void collect() {}

	void* get_context() const { return nullptr; }
	bool is_initialized() const { return false; }

private:
	TracyManager() = default;
	~TracyManager() = default;
};

#define TRACY_CPU_ZONE
#define TRACY_CPU_ZONE_N(name)
#define TRACY_CPU_ZONE_C(name, category)
#define TRACY_CPU_ZONE_DYN(name_str)
#define TRACY_CPU_TEXT(text_str, size)
#define TRACY_CB_ZONE(name)
#define TRACY_CB_ZONE_DYN(name_str)
#define TRACY_GPU_ZONE(name)
#define TRACY_GPU_ZONE_C(name, category)
#define TRACY_GPU_ZONE_DYN(name, str, len)

#endif // TRACY_ENABLE