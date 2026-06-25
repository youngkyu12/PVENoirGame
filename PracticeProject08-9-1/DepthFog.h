//-----------------------------------------------------------------------------
// File: DepthFog.h
//-----------------------------------------------------------------------------

#pragma once

#include "Shader.h"

struct CB_FOG
{
	XMFLOAT4 fogColor = XMFLOAT4(0.62f, 0.67f, 0.72f, 1.0f);

	// x = fogStart
	// y = fogEnd
	// z = fogDensity
	// w = fogEnable (0.0f or 1.0f)
	XMFLOAT4 fogParams0 = XMFLOAT4(20.0f, 40.0f, 0.0f, 1.0f);

	// x = cameraNear
	// y = cameraFar
	// z = fogMode (0=Linear, 1=Exp, 2=Exp2)
	// w = fadeAlpha
	XMFLOAT4 fogParams1 = XMFLOAT4(1.01f, 5000.0f, 0.0f, 0.0f);
};

enum class EDepthFogPresetMode : uint8_t
{
	OuterWide = 0,
	ZoneDense = 1
};

class CDepthFogSystem final
{
public:
	CDepthFogSystem();
	~CDepthFogSystem();

public:
	void ResetState();

	void BuildResources(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		ID3D12RootSignature* rootSignature
	);

	void ReleaseResources();
	void ReleaseShaderVariables();
	void ReleaseConstantBuffer();

	void CreateConstantBuffer(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd
	);

	void SetSourceSrvIndices(UINT sceneColorSrvIndex, UINT sceneDepthSrvIndex);
	void SetAmbientOcclusionSrvIndex(UINT ambientOcclusionSrvIndex);
	void SetFrameResourceIndex(UINT frameResourceIndex);
	void SetPassEnabled(bool enabled) { m_passEnabled = enabled; }
	bool IsPassEnabled() const { return m_passEnabled; }

	void UpdateState(float dt, EDepthFogPresetMode mode);
	void UploadConstantBuffer();
	void BindConstantBuffer(ID3D12GraphicsCommandList* cmd) const;

	void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	const CB_FOG& GetFogData() const { return m_fogData; }

	void OnResize(int width, int height);

private:
	std::shared_ptr<CDepthFogShader> m_shader;

	static constexpr UINT kFrameResourceCount = 2;

	std::array<ComPtr<ID3D12Resource>, kFrameResourceCount> m_cbFog;
	std::array<CB_FOG*, kFrameResourceCount> m_mappedFog = {};
	UINT m_nFrameResourceIndex = 0;

	CB_FOG m_fogData{};
	CB_FOG m_zoneDensePreset{};
	CB_FOG m_outerWidePreset{};
	CB_FOG m_blendStartPreset{};
	CB_FOG m_blendTargetPreset{};

	EDepthFogPresetMode m_currentMode = EDepthFogPresetMode::OuterWide;
	EDepthFogPresetMode m_targetMode = EDepthFogPresetMode::OuterWide;

	bool m_passEnabled = true;

	float m_fadeAlpha = 1.0f;
	float m_fadeDuration = 1.0f;

	UINT m_sceneColorSrvIndex = UINT_MAX;
	UINT m_sceneDepthSrvIndex = UINT_MAX;
	UINT m_ambientOcclusionSrvIndex = UINT_MAX;

	float m_screenWidth = FRAME_BUFFER_WIDTH;
	float m_screenHeight = FRAME_BUFFER_HEIGHT;
};
