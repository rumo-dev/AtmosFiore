#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include <string>

#include "Engine/Graphics/FrameBuffer/frame_buffer.h"
#include "Engine/Graphics/FullscreenQuad/fullscreen_quad.h"

class DLSSManager {
public:
	enum class Mode { UltraPerformance, Performance, Balanced, Quality, DLAA };

	DLSSManager() = default;
	~DLSSManager();
	DLSSManager(const DLSSManager&) = delete;
	DLSSManager& operator=(const DLSSManager&) = delete;

	bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, UINT outputWidth, UINT outputHeight);
	bool Process(ID3D11DeviceContext* context, ID3D11ShaderResourceView* color,
		ID3D11ShaderResourceView* depth, ID3D11ShaderResourceView* motionVectors);
	ID3D11ShaderResourceView* GetOutput() const { return m_output ? m_output->GetColorMap() : nullptr; }
	bool IsAvailable() const { return m_initialized && m_feature != nullptr && m_reason.empty(); }
	const std::string& GetUnavailableReason() const { return m_reason; }
	bool IsEnabled() const { return m_enabled; }
	void SetEnabled(bool enabled);
	Mode GetMode() const { return m_mode; }
	void SetMode(Mode mode);
	float GetSharpness() const { return m_sharpness; }
	void SetSharpness(float sharpness);

private:
	bool CreateFeature();
	void ReleaseFeature();
	void SetFailure(const char* operation, long long result);

	ID3D11Device* m_device = nullptr;
	ID3D11DeviceContext* m_context = nullptr;
	UINT m_outputWidth = 0;
	UINT m_outputHeight = 0;
	UINT m_inputWidth = 0;
	UINT m_inputHeight = 0;
	void* m_capabilityParameters = nullptr; // Owned by NGX.
	void* m_feature = nullptr;
	bool m_initialized = false;
	bool m_enabled = false;
	bool m_featureDirty = false;
	bool m_resetNextFrame = true;
	Mode m_mode = Mode::Quality;
	float m_sharpness = 0.0f;
	std::string m_reason = "NGX is not initialized";
	std::unique_ptr<FullscreenQuad> m_fullscreenQuad;
	std::unique_ptr<Framebuffer> m_scaledColor;
	std::unique_ptr<Framebuffer> m_scaledDepth;
	std::unique_ptr<Framebuffer> m_scaledMotionVectors;
	std::unique_ptr<Framebuffer> m_output;
};
