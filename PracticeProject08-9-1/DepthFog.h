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
	void SetPassEnabled(bool enabled) { m_passEnabled = enabled; }
	bool IsPassEnabled() const { return m_passEnabled; }

	void UpdateState(float dt, bool enableFog);
	void UploadConstantBuffer();
	void BindConstantBuffer(ID3D12GraphicsCommandList* cmd) const;

	void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	const CB_FOG& GetFogData() const { return m_fogData; }

private:
	std::shared_ptr<CDepthFogShader> m_shader;

	ComPtr<ID3D12Resource> m_cbFog;
	CB_FOG* m_mappedFog = nullptr;

	CB_FOG m_fogData{};
	CB_FOG m_enabledPreset{};
	CB_FOG m_disabledPreset{};

	bool m_targetEnabled = false;
	bool m_passEnabled = true;

	float m_fadeAlpha = 0.0f;
	float m_fadeDuration = 1.0f;

	UINT m_sceneColorSrvIndex = UINT_MAX;
	UINT m_sceneDepthSrvIndex = UINT_MAX;
};