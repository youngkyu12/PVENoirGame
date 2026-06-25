//-----------------------------------------------------------------------------
// File: DepthFog.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "DepthFog.h"

#include "GlobalValues.h"

namespace
{
	static float LerpFloat(float a, float b, float t)
	{
		return a + ( b - a ) * t;
	}

	static XMFLOAT4 LerpFloat4(const XMFLOAT4& a, const XMFLOAT4& b, float t)
	{
		return XMFLOAT4(
			LerpFloat(a.x, b.x, t),
			LerpFloat(a.y, b.y, t),
			LerpFloat(a.z, b.z, t),
			LerpFloat(a.w, b.w, t)
		);
	}

	static CB_FOG LerpFogPreset(const CB_FOG& a, const CB_FOG& b, float t)
	{
		CB_FOG out{};
		out.fogColor = LerpFloat4(a.fogColor, b.fogColor, t);
		out.fogParams0 = LerpFloat4(a.fogParams0, b.fogParams0, t);
		out.fogParams1 = LerpFloat4(a.fogParams1, b.fogParams1, t);
		return out;
	}
}

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
	CB_FOG base{};

	// 기존 안개 구역용 짙은 안개.
	// 필요하면 기존에 쓰던 수치를 여기서 유지/조정하면 된다.
	m_zoneDensePreset = base;
	m_zoneDensePreset.fogColor = XMFLOAT4(0.62f, 0.67f, 0.72f, 1.0f);
	m_zoneDensePreset.fogParams0 = XMFLOAT4(
		20.0f,
		40.0f,
		0.0f,
		1.0f
	);
	m_zoneDensePreset.fogParams1 = XMFLOAT4(
		1.01f,
		5000.0f,
		0.0f,
		1.0f
	);

	// 안개 구역 외부용 넓은 안개.
	// 건물/타워 대부분이 LOD2로 들어가는 300m부터,
	// 현재 가장 먼 주요 static cull 거리인 VillageWall 900m까지 덮는다.
	m_outerWidePreset = base;
	m_outerWidePreset.fogColor = XMFLOAT4(0.62f, 0.67f, 0.72f, 1.0f);
	m_outerWidePreset.fogParams0 = XMFLOAT4(
		300.0f,
		500.0f,
		0.0f,
		1.0f
	);
	m_outerWidePreset.fogParams1 = XMFLOAT4(
		1.01f,
		5000.0f,
		0.0f,
		1.0f
	);

	m_fogData = m_outerWidePreset;
	m_blendStartPreset = m_outerWidePreset;
	m_blendTargetPreset = m_outerWidePreset;

	m_currentMode = EDepthFogPresetMode::OuterWide;
	m_targetMode = EDepthFogPresetMode::OuterWide;

	m_passEnabled = true;

	m_fadeAlpha = 1.0f;
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
	m_ambientOcclusionSrvIndex = UINT_MAX;

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
	for ( UINT i = 0; i < kFrameResourceCount; ++i )
	{
		if ( m_cbFog[i] )
		{
			if ( m_mappedFog[i] )
			{
				m_cbFog[i]->Unmap(0, NULL);
				m_mappedFog[i] = nullptr;
			}

			m_cbFog[i].Reset();
		}

		m_mappedFog[i] = nullptr;
	}

	m_nFrameResourceIndex = 0;
}

void CDepthFogSystem::CreateConstantBuffer(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd)
{
	ReleaseConstantBuffer();

	if ( !dev )
		return;

	const UINT cbBytes = ( ( sizeof(CB_FOG) + 255 ) & ~255 );

	for ( UINT i = 0; i < kFrameResourceCount; ++i )
	{
		m_cbFog[i] = ::CreateBufferResource(
			dev,
			cmd,
			nullptr,
			cbBytes,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			nullptr
		);

		if ( !m_cbFog[i] )
			continue;

		m_cbFog[i]->Map(
			0,
			nullptr,
			reinterpret_cast< void** >( &m_mappedFog[i] )
		);

		if ( m_mappedFog[i] )
			::memcpy(m_mappedFog[i], &m_fogData, sizeof(CB_FOG));
	}

	m_nFrameResourceIndex = 0;
}

void CDepthFogSystem::SetSourceSrvIndices(UINT sceneColorSrvIndex, UINT sceneDepthSrvIndex)
{
	m_sceneColorSrvIndex = sceneColorSrvIndex;
	m_sceneDepthSrvIndex = sceneDepthSrvIndex;
}

void CDepthFogSystem::SetAmbientOcclusionSrvIndex(UINT ambientOcclusionSrvIndex)
{
	m_ambientOcclusionSrvIndex = ambientOcclusionSrvIndex;
}

void CDepthFogSystem::SetFrameResourceIndex(UINT frameResourceIndex)
{
	m_nFrameResourceIndex = frameResourceIndex % kFrameResourceCount;
}

void CDepthFogSystem::UpdateState(float dt, EDepthFogPresetMode mode)
{
	const CB_FOG& desiredPreset =
		( mode == EDepthFogPresetMode::ZoneDense )
		? m_zoneDensePreset
		: m_outerWidePreset;

	if ( mode != m_targetMode )
	{
		m_currentMode = m_targetMode;
		m_targetMode = mode;

		m_blendStartPreset = m_fogData;
		m_blendTargetPreset = desiredPreset;
		m_fadeAlpha = 0.0f;
	}

	float safeDt = dt;
	if ( safeDt < 0.0f )
		safeDt = 0.0f;

	const float safeDuration =
		( m_fadeDuration > 0.0001f )
		? m_fadeDuration
		: 0.0001f;

	if ( m_fadeAlpha < 1.0f )
	{
		m_fadeAlpha += safeDt / safeDuration;

		if ( m_fadeAlpha > 1.0f )
			m_fadeAlpha = 1.0f;
	}

	m_fogData = LerpFogPreset(
		m_blendStartPreset,
		m_blendTargetPreset,
		m_fadeAlpha
	);

	// 이제 외곽 안개도 항상 켜진 상태다.
	m_fogData.fogParams0.w = 1.0f;

	// 셰이더의 기존 fadeAlpha 입력은 유지한다.
	// ZoneDense <-> OuterWide 전환 보간값으로 사용된다.
	m_fogData.fogParams1.w = m_fadeAlpha;
}

void CDepthFogSystem::UploadConstantBuffer()
{
	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	if ( !m_mappedFog[frameIndex] )
		return;

	::memcpy(m_mappedFog[frameIndex], &m_fogData, sizeof(CB_FOG));
}

void CDepthFogSystem::BindConstantBuffer(ID3D12GraphicsCommandList* cmd) const
{
	if ( !cmd )
		return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	if ( !m_cbFog[frameIndex] )
		return;

	const D3D12_GPU_VIRTUAL_ADDRESS fogGpu =
		m_cbFog[frameIndex]->GetGPUVirtualAddress();

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
		m_ambientOcclusionSrvIndex,
		0
	);

	opt.m_xmu4PostSrvIdx1 = XMUINT4(0, 0, 0, 0);

	opt.m_xmf4UiRect = XMFLOAT4(
		m_screenWidth * 0.5f,
		m_screenHeight * 0.5f,
		static_cast< float >( m_screenWidth ),
		static_cast< float >( m_screenHeight )
	);

	opt.m_xmf4Viewport = XMFLOAT4(
		static_cast< float >( m_screenWidth ),
		static_cast< float >( m_screenHeight ),
		1.0f / static_cast< float >( m_screenWidth ),
		1.0f / static_cast< float >( m_screenHeight )
	);

	m_shader->ResetDrawOptionWriteIndex();
	m_shader->Render(cmd, camera, &opt);
}

void CDepthFogSystem::OnResize(int width, int height)
{
	m_screenWidth = static_cast<float>(width);
	m_screenHeight = static_cast<float>(height);
}
