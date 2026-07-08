#pragma once

#include <d3d11_1.h> 
//#include <d3d12.h>   // DX12の型判別のために追加
#include <wrl/client.h>
#include <string>

namespace RenderDebug
{
	// マーカーおよびリソース管理を行うコアUtilクラス
	class MarkerUtil
	{
	private:
		Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> m_pAnnotation;
		bool m_isAvailable = false;

	public:
		MarkerUtil() = default;
		~MarkerUtil() = default;

		// 初期化: 各種コンテキストやコマンドキューからインターフェースをクエリする
		void Initialize(IUnknown* pDeviceContextOrQueue)
		{
			if (!pDeviceContextOrQueue) return;

			HRESULT hr = pDeviceContextOrQueue->QueryInterface(IID_PPV_ARGS(&m_pAnnotation));
			if (SUCCEEDED(hr))
			{
				m_isAvailable = m_pAnnotation->GetStatus() != 0;
			}
		}

		void Shutdown()
		{
			m_pAnnotation.Reset();
			m_isAvailable = false;
		}

		// イベント開始
		void BeginEvent(LPCWSTR name)
		{
			if (m_isAvailable && m_pAnnotation)
			{
				m_pAnnotation->BeginEvent(name);
			}
		}

		// イベント終了
		void EndEvent()
		{
			if (m_isAvailable && m_pAnnotation)
			{
				m_pAnnotation->EndEvent();
			}
		}

		// 単発マーカー
		void SetMarker(LPCWSTR name)
		{
			if (m_isAvailable && m_pAnnotation)
			{
				m_pAnnotation->SetMarker(name);
			}
		}

		// --- 追加: リソース名（テクスチャやバッファなど）の設定 ---
		// DX11/DX12 のリソースポインタ（ID3D11Resource, ID3D12Resource等）をそのまま渡せます
		static void SetResourceName(IUnknown* pResource, LPCWSTR name)
		{
			if (!pResource || !name) return;

			Microsoft::WRL::ComPtr<ID3D11DeviceChild> pD3D11DeviceChild;
			if (SUCCEEDED(pResource->QueryInterface(IID_PPV_ARGS(&pD3D11DeviceChild))))
			{
				// ワイド文字列用のGUID（WKPDID_D3DDebugObjectNameW）を使用して命名
				size_t length = wcslen(name);
				pD3D11DeviceChild->SetPrivateData(WKPDID_D3DDebugObjectNameW, static_cast<UINT>(length * sizeof(wchar_t)), name);
			}
		}
	};

	// --- RAII 用のスコープクラス ---
	class ScopedEvent
	{
	private:
		MarkerUtil* m_pUtil;

	public:
		ScopedEvent(MarkerUtil* pUtil, LPCWSTR name) : m_pUtil(pUtil)
		{
			if (m_pUtil) m_pUtil->BeginEvent(name);
		}

		~ScopedEvent()
		{
			if (m_pUtil) m_pUtil->EndEvent();
		}

		ScopedEvent(const ScopedEvent&) = delete;
		ScopedEvent& operator=(const ScopedEvent&) = delete;
	};
}

#if defined(_DEBUG) || defined(DEVELOPMENT)
#define DX_START_EVENT(utilPtr, name) (utilPtr)->BeginEvent(name)
#define DX_END_EVENT(utilPtr)         (utilPtr)->EndEvent()
#define DX_SET_MARKER(utilPtr, name)  (utilPtr)->SetMarker(name)
#define DX_SET_NAME(pResource, name)  RenderDebug::MarkerUtil::SetResourceName(pResource, name)

#define DX_WCHAR_STR(x) DX_WCHAR_STR_IMPL(x)
#define DX_WCHAR_STR_IMPL(x) L#x

#define DX_CONCAT_IMPL(a, b) a##b
#define DX_CONCAT(a, b) DX_CONCAT_IMPL(a, b)
#define DX_SCOPED_EVENT(utilPtr, name) RenderDebug::ScopedEvent DX_CONCAT(__scopedEvent_, __LINE__)(utilPtr, name)
#else
#define DX_START_EVENT(utilPtr, name) 
#define DX_END_EVENT(utilPtr)         
#define DX_SET_MARKER(utilPtr, name)  
#define DX_SET_NAME(pResource, name)  
#define DX_SET_NAME_FROM_VAR(pResource) // リリース時は消滅
#define DX_SCOPED_EVENT(utilPtr, name) 
#endif