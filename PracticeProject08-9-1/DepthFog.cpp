//-----------------------------------------------------------------------------
// File: DepthFog.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "DepthFog.h"

#include "GlobalValues.h"

CDepthFogSystem::CDepthFogSystem()
{
	ResetState();
}

CDepthFogSystem::~CDepthFogSystem()
{
	ReleaseResources();
}

void CDepthFogSystem::ResetState()
{
	m_fogData = CB_FOG{};

	m_enabledPreset = m_fogData;
	m_disabledPreset = m_fogData;

	m_disabledPreset.fogParams0.x = 0.0f;
	m_disabledPreset.fogParams0.y = 1.0f;
	m_disabledPreset.fogParams0.z = 0.0f;
	m_disabledPreset.fogParams0.w = 0.0f;

	m_fogData = m_disabledPreset;

	m_targetEnabled = false;
	m_passEnabled = true;

	m_fadeAlpha = 0.0f;
	m_fadeDuration = 1.0f;
}

void CDepthFogSystem::BuildResources(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	ID3D12RootSignature* rootSignature)
{
	if ( m_shader )
		m_shader->ReleaseShaderVariables();

	m_shader.reset();
	m_shader = std::make_shared<CDepthFogShader>();

	DXGI_FORMAT fogRtv = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT fogDsv = DXGI_FORMAT_UNKNOWN;

	m_shader->CreateShader(
		dev,
		rootSignature,
		1,
		&fogRtv,
		fogDsv
	);

	m_shader->CreateShaderVariables(dev, cmd);
}

void CDepthFogSystem::ReleaseResources()
{
	ReleaseShaderVariables();

	m_shader.reset();

	m_sceneColorSrvIndex = UINT_MAX;
	m_sceneDepthSrvIndex = UINT_MAX;

	ResetState();
}

void CDepthFogSystem::ReleaseShaderVariables()
{
	if ( m_shader )
		m_shader->ReleaseShaderVariables();

	ReleaseConstantBuffer();
}

void CDepthFogSystem::ReleaseConstantBuffer()
{
	if ( m_cbFog )
	{
		if ( m_mappedFog )
		{
			m_cbFog->Unmap(0, NULL);
			m_mappedFog = nullptr;
		}

		m_cbFog.Reset();
	}
}

void CDepthFogSystem::CreateConstantBuffer(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd)
{
	ReleaseConstantBuffer();

	if ( !dev )
		return;

	const UINT cbBytes = ( ( sizeof(CB_FOG) + 255 ) & ~255 );

	m_cbFog = ::CreateBufferResource(
		dev,
		cmd,
		nullptr,
		cbBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	if ( !m_cbFog )
		return;

	m_cbFog->Map(0, nullptr, reinterpret_cast< void** >( &m_mappedFog ));

	UploadConstantBuffer();
}

void CDepthFogSystem::SetSourceSrvIndices(UINT sceneColorSrvIndex, UINT sceneDepthSrvIndex)
{
	m_sceneColorSrvIndex = sceneColorSrvIndex;
	m_sceneDepthSrvIndex = sceneDepthSrvIndex;
}

void CDepthFogSystem::UpdateState(float dt, bool enableFog)
{
	m_targetEnabled = enableFog;

	const float targetAlpha = enableFog ? 1.0f : 0.0f;

	float safeDt = dt;
	if ( safeDt < 0.0f )
		safeDt = 0.0f;

	const float safeDuration = ( m_fadeDuration > 0.0001f )
		? m_fadeDuration
		: 0.0001f;

	const float step = safeDt / safeDuration;

	if ( m_fadeAlpha < targetAlpha )
	{
		m_fadeAlpha += step;

		if ( m_fadeAlpha > targetAlpha )
			m_fadeAlpha = targetAlpha;
	}
	else if ( m_fadeAlpha > targetAlpha )
	{
		m_fadeAlpha -= step;

		if ( m_fadeAlpha < targetAlpha )
			m_fadeAlpha = targetAlpha;
	}

	const float t = m_fadeAlpha;
	const float invT = 1.0f - t;

	m_fogData.fogColor.x = m_disabledPreset.fogColor.x * invT + m_enabledPreset.fogColor.x * t;
	m_fogData.fogColor.y = m_disabledPreset.fogColor.y * invT + m_enabledPreset.fogColor.y * t;
	m_fogData.fogColor.z = m_disabledPreset.fogColor.z * invT + m_enabledPreset.fogColor.z * t;
	m_fogData.fogColor.w = m_disabledPreset.fogColor.w * invT + m_enabledPreset.fogColor.w * t;

	// 기존 동작 유지:
	// 실제 fog 수치는 enabled preset을 유지하고,
	// enable flag와 fade alpha만 갱신한다.
	m_fogData = m_enabledPreset;

	m_fogData.fogParams0.w = ( m_fadeAlpha > 0.0f || enableFog ) ? 1.0f : 0.0f;
	m_fogData.fogParams1.w = m_fadeAlpha;
}

void CDepthFogSystem::UploadConstantBuffer()
{
	if ( !m_mappedFog )
		return;

	::memcpy(m_mappedFog, &m_fogData, sizeof(CB_FOG));
}

void CDepthFogSystem::BindConstantBuffer(ID3D12GraphicsCommandList* cmd) const
{
	if ( !cmd )
		return;

	if ( !m_cbFog )
		return;

	const D3D12_GPU_VIRTUAL_ADDRESS fogGpu = m_cbFog->GetGPUVirtualAddress();
	cmd->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_FOG, fogGpu);
}

void CDepthFogSystem::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd )
		return;

	if ( !m_shader )
		return;

	if ( !m_passEnabled )
		return;

	if ( m_sceneColorSrvIndex == UINT_MAX )
		return;

	if ( m_sceneDepthSrvIndex == UINT_MAX )
		return;

	PS_CB_DRAW_OPTIONS opt{};
	opt.m_xmn4DrawOptions = XMINT4(0, 0, 0, 0);

	opt.m_xmu4PostSrvIdx0 = XMUINT4(
		m_sceneColorSrvIndex,
		m_sceneDepthSrvIndex,
		0,
		0
	);

	opt.m_xmu4PostSrvIdx1 = XMUINT4(0, 0, 0, 0);

	opt.m_xmf4UiRect = XMFLOAT4(
		FRAME_BUFFER_WIDTH * 0.5f,
		FRAME_BUFFER_HEIGHT * 0.5f,
		static_cast< float >( FRAME_BUFFER_WIDTH ),
		static_cast< float >( FRAME_BUFFER_HEIGHT )
	);

	opt.m_xmf4Viewport = XMFLOAT4(
		static_cast< float >( FRAME_BUFFER_WIDTH ),
		static_cast< float >( FRAME_BUFFER_HEIGHT ),
		1.0f / static_cast< float >( FRAME_BUFFER_WIDTH ),
		1.0f / static_cast< float >( FRAME_BUFFER_HEIGHT )
	);

	m_shader->ResetDrawOptionWriteIndex();
	m_shader->Render(cmd, camera, &opt);
}