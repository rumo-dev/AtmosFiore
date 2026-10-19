#include "dlss_manager.h"

#if defined(_WIN64)
#include <filesystem>
#include <sstream>
#include <Windows.h>

#include "nvsdk_ngx_helpers.h"
#include "nvsdk_ngx_helpers_d3d.h"
#include "Engine/System/graphics_core.h"
#include "Engine/System/Manager/PostProcess/post_process_manager.h"
#include "Game/World/camera/camera_manager.h"

namespace {
NVSDK_NGX_PerfQuality_Value ToNgxMode(DLSSManager::Mode mode) {
	switch (mode) {
	case DLSSManager::Mode::UltraPerformance: return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
	case DLSSManager::Mode::Performance: return NVSDK_NGX_PerfQuality_Value_MaxPerf;
	case DLSSManager::Mode::Balanced: return NVSDK_NGX_PerfQuality_Value_Balanced;
	case DLSSManager::Mode::Quality: return NVSDK_NGX_PerfQuality_Value_MaxQuality;
	case DLSSManager::Mode::DLAA: return NVSDK_NGX_PerfQuality_Value_DLAA;
	}
	return NVSDK_NGX_PerfQuality_Value_MaxQuality;
}

std::wstring ExecutableDirectory() {
	wchar_t path[MAX_PATH]{};
	const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) return L".";
	return std::filesystem::path(path).parent_path().wstring();
}
}

DLSSManager::~DLSSManager() {
	ReleaseFeature();
	if (m_capabilityParameters) {
		m_capabilityParameters = nullptr; // SDK-owned; released by NGX shutdown.
	}
	if (m_initialized) NVSDK_NGX_D3D11_Shutdown1(m_device);
}

bool DLSSManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, UINT outputWidth, UINT outputHeight) {
	m_device = device;
	m_context = context;
	m_outputWidth = outputWidth;
	m_outputHeight = outputHeight;
	if (!device || !context || outputWidth == 0 || outputHeight == 0) {
		m_reason = "D3D11 device, context, or output size is invalid";
		return false;
	}

	// NGX requires a unique, GUID-like Project ID for a custom renderer. Reuse
	// this project's Visual Studio GUID so the value stays stable across builds.
	const std::wstring appDirectory = ExecutableDirectory();
	NVSDK_NGX_Result result = NVSDK_NGX_D3D11_Init_with_ProjectID(
		"9970c797-4c8c-42a1-9c72-6b1bee18925b", NVSDK_NGX_ENGINE_TYPE_CUSTOM, "1.0.0",
		appDirectory.c_str(), device, nullptr, NVSDK_NGX_Version_API);
	if (NVSDK_NGX_FAILED(result)) {
		SetFailure("NGX initialization", result);
		return false;
	}
	m_initialized = true;

	NVSDK_NGX_Parameter* capabilityParameters = nullptr;
	result = NVSDK_NGX_D3D11_GetCapabilityParameters(&capabilityParameters);
	m_capabilityParameters = capabilityParameters;
	if (NVSDK_NGX_FAILED(result) || !m_capabilityParameters) {
		SetFailure("NGX capability query", result);
		return false;
	}

	m_fullscreenQuad = std::make_unique<FullscreenQuad>(device);
	m_output = std::make_unique<Framebuffer>(device, outputWidth, outputHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, false, false, true);
	m_featureDirty = true;
	if (!CreateFeature()) return false;
	m_reason.clear();
	return true;
}

bool DLSSManager::CreateFeature() {
	if (!m_initialized || !m_capabilityParameters) return false;
	ReleaseFeature();

	unsigned int renderWidth = 0, renderHeight = 0;
	unsigned int maxWidth = 0, maxHeight = 0, minWidth = 0, minHeight = 0;
	float optimalSharpness = 0.0f;
	NVSDK_NGX_Result result = NGX_DLSS_GET_OPTIMAL_SETTINGS(
		static_cast<NVSDK_NGX_Parameter*>(m_capabilityParameters), m_outputWidth, m_outputHeight, ToNgxMode(m_mode),
		&renderWidth, &renderHeight, &maxWidth, &maxHeight, &minWidth, &minHeight, &optimalSharpness);
	if (NVSDK_NGX_FAILED(result) || renderWidth == 0 || renderHeight == 0) {
		SetFailure("DLSS optimal render-size query", result);
		return false;
	}

	NVSDK_NGX_Parameter* parameters = nullptr;
	result = NVSDK_NGX_D3D11_AllocateParameters(&parameters);
	if (NVSDK_NGX_FAILED(result) || !parameters) {
		SetFailure("NGX parameter allocation", result);
		return false;
	}

	NVSDK_NGX_DLSS_Create_Params createParams{};
	createParams.Feature.InWidth = renderWidth;
	createParams.Feature.InHeight = renderHeight;
	createParams.Feature.InTargetWidth = m_outputWidth;
	createParams.Feature.InTargetHeight = m_outputHeight;
	createParams.Feature.InPerfQualityValue = ToNgxMode(m_mode);
	// The G-buffer produces render-resolution motion vectors for MVLowRes.
	createParams.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_MVLowRes | NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
	createParams.InEnableOutputSubrects = false;
	NVSDK_NGX_Handle* feature = nullptr;
	result = NGX_D3D11_CREATE_DLSS_EXT(m_context, &feature, parameters, &createParams);
	NVSDK_NGX_D3D11_DestroyParameters(parameters);
	if (NVSDK_NGX_FAILED(result) || !feature) {
		m_feature = nullptr;
		SetFailure("DLSS feature creation", result);
		return false;
	}
	m_feature = feature;

	m_inputWidth = renderWidth;
	m_inputHeight = renderHeight;
	auto* device = m_device;
	m_scaledColor = std::make_unique<Framebuffer>(device, renderWidth, renderHeight, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, false);
	m_featureDirty = false;
	m_resetNextFrame = true;
	m_reason.clear();
	return true;
}

void DLSSManager::ReleaseFeature() {
	if (m_feature) {
		NVSDK_NGX_D3D11_ReleaseFeature(static_cast<NVSDK_NGX_Handle*>(m_feature));
		m_feature = nullptr;
	}
}

void DLSSManager::SetFailure(const char* operation, long long result) {
	std::ostringstream message;
	message << operation << " failed (NGX result 0x" << std::hex << static_cast<unsigned int>(result) << ")";
	m_reason = message.str();
	m_enabled = false;
}

void DLSSManager::SetMode(Mode mode) {
	if (m_mode == mode) return;
	m_mode = mode;
	m_featureDirty = true;
	m_resetNextFrame = true;
	if (m_initialized && CreateFeature() && m_enabled) {
		Post_Process_Manager::SetDLSSRenderResolution(m_inputWidth, m_inputHeight);
	}
}

void DLSSManager::SetEnabled(bool enabled) {
	m_enabled = enabled && IsAvailable();
	if (m_enabled) {
		if (m_featureDirty && !CreateFeature()) m_enabled = false;
	}
	if (m_enabled) {
		Post_Process_Manager::SetDLSSRenderResolution(m_inputWidth, m_inputHeight);
	} else if (m_outputWidth && m_outputHeight) {
		Post_Process_Manager::SetDLSSRenderResolution(m_outputWidth, m_outputHeight);
	}
}

void DLSSManager::SetSharpness(float sharpness) {
	m_sharpness = (sharpness < 0.0f) ? 0.0f : ((sharpness > 1.0f) ? 1.0f : sharpness);
}

bool DLSSManager::Process(ID3D11DeviceContext* context, ID3D11ShaderResourceView* color,
	ID3D11ShaderResourceView* depth, ID3D11ShaderResourceView* motionVectors) {
	if (!m_enabled || !IsAvailable() || !context || !color || !depth || !motionVectors) return false;
	if (m_featureDirty && !CreateFeature()) return false;

	// Post-process color targets remain at output resolution, so resample the
	// linear HDR color to NGX's recommended render size.
	Render_State::instance().set_blend_state(context, Blend_State::Opaque);
	m_scaledColor->Clear(context);
	m_scaledColor->Activate(context);
	m_fullscreenQuad->Blit(context, &color, 0, 1);
	m_scaledColor->Deactivate(context);

	// GeometryBuffer already matches DLSS's input resolution, so use its depth
	// and motion-vector resources directly instead of copying them every frame.
	Microsoft::WRL::ComPtr<ID3D11Resource> colorResource, depthResource, motionResource, outputResource;
	m_scaledColor->GetColorMap()->GetResource(&colorResource);
	depth->GetResource(&depthResource);
	motionVectors->GetResource(&motionResource);
	m_output->GetColorMap()->GetResource(&outputResource);

	NVSDK_NGX_Parameter* parameters = nullptr;
	NVSDK_NGX_Result result = NVSDK_NGX_D3D11_AllocateParameters(&parameters);
	if (NVSDK_NGX_FAILED(result) || !parameters) {
		SetFailure("NGX parameter allocation", result);
		return false;
	}

	NVSDK_NGX_D3D11_DLSS_Eval_Params eval{};
	eval.Feature.pInColor = colorResource.Get();
	eval.Feature.pInOutput = outputResource.Get();
	eval.Feature.InSharpness = m_sharpness;
	eval.pInDepth = depthResource.Get();
	eval.pInMotionVectors = motionResource.Get();
	eval.InRenderSubrectDimensions = { m_inputWidth, m_inputHeight };
	// Motion vectors are normalized UV offsets and this feature uses MVLowRes,
	// so convert them to input/render-pixel offsets using the vector texture size.
	eval.InMVScaleX = static_cast<float>(m_inputWidth);
	eval.InMVScaleY = static_cast<float>(m_inputHeight);
	eval.InReset = m_resetNextFrame ? 1 : 0;
	result = NGX_D3D11_EVALUATE_DLSS_EXT(context, static_cast<NVSDK_NGX_Handle*>(m_feature), parameters, &eval);
	NVSDK_NGX_D3D11_DestroyParameters(parameters);
	if (NVSDK_NGX_FAILED(result)) {
		SetFailure("DLSS evaluation", result);
		return false;
	}
	m_resetNextFrame = false;

	// NGX may leave its compute UAV bindings in the immediate context. Clear
	// them before the output texture is consumed by the tone-mapping pass.
	ID3D11UnorderedAccessView* nullUavs[D3D11_PS_CS_UAV_REGISTER_COUNT]{};
	context->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, nullUavs, nullptr);
	return true;
}
#else
DLSSManager::~DLSSManager() = default;
bool DLSSManager::Initialize(ID3D11Device*, ID3D11DeviceContext*, UINT, UINT) {
	m_reason = "NVIDIA DLSS is only available in x64 builds";
	return false;
}
bool DLSSManager::Process(ID3D11DeviceContext*, ID3D11ShaderResourceView*, ID3D11ShaderResourceView*, ID3D11ShaderResourceView*) { return false; }
bool DLSSManager::CreateFeature() { return false; }
void DLSSManager::ReleaseFeature() {}
void DLSSManager::SetFailure(const char*, long long) {}
void DLSSManager::SetEnabled(bool enabled) { m_enabled = enabled && IsAvailable(); }
void DLSSManager::SetMode(Mode mode) { m_mode = mode; }
void DLSSManager::SetSharpness(float sharpness) { m_sharpness = sharpness; }
#endif
