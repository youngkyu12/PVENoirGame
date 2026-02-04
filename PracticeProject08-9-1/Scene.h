//-----------------------------------------------------------------------------
// File: Scene.h
//-----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <vector>

#include "Shader.h"
#include "Player.h"
#include "DescriptorHeap.h"

#define ROOT_PARAMETER_GLOBAL_SRV 6
#define ROOT_PARAMETER_DRAW_OPTIONS 5
constexpr UINT LEGACY_SRV_COUNT = 6; // t0(1) + t1~t5(5)

class CMaterial;
class CGameObject;
struct CB_GAMEOBJECT_INFO;
struct CB_BONE_PALETTE;

struct LIGHT
{
	XMFLOAT4				m_xmf4Ambient;
	XMFLOAT4				m_xmf4Diffuse;
	XMFLOAT4				m_xmf4Specular;
	XMFLOAT3				m_xmf3Position;
	float 					m_fFalloff;
	XMFLOAT3				m_xmf3Direction;
	float 					m_fTheta; //cos(m_fTheta)
	XMFLOAT3				m_xmf3Attenuation;
	float					m_fPhi; //cos(m_fPhi)
	bool					m_bEnable;
	int						m_nType;
	float					m_fRange;
	float					padding;
};

struct LIGHTS
{
	LIGHT					m_pLights[MAX_LIGHTS];
	XMFLOAT4				m_xmf4GlobalAmbient;
};

struct MATERIAL
{
	XMFLOAT4				m_xmf4Ambient;
	XMFLOAT4				m_xmf4Diffuse;
	XMFLOAT4				m_xmf4Specular; //(r,g,b,a=power)
	XMFLOAT4				m_xmf4Emissive;
	XMUINT4					m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);
};

struct MATERIALS
{
	MATERIAL				m_pReflections[MAX_MATERIALS];
};

// ============================================================================
// [ADD] Scene-owned batches (members only; not wired yet)
// ============================================================================
struct SCENE_STATIC_BATCH
{
	std::shared_ptr<CStaticObjectsShader>			shader;
	std::vector<std::unique_ptr<CGameObject>>		objects;
	UINT											nObjects = 0;

	ComPtr<ID3D12Resource>							cbGameObjects;
	CB_GAMEOBJECT_INFO* mappedGameObjects = nullptr;

	UINT											cbElementBytes = 0;

	D3D12_GPU_DESCRIPTOR_HANDLE						baseCbvGpu = { 0 };
	UINT											cbvInc = 0;

	std::shared_ptr<CMaterial>						material;		// legacy 있으면 사용
};

struct SCENE_SKINNED_BATCH
{
	std::shared_ptr<CSkinnedObjectsShader>			shader;
	std::vector<std::unique_ptr<CGameObject>>		objects;
	UINT											nObjects = 0;

	ComPtr<ID3D12Resource>							cbGameObjects;
	CB_GAMEOBJECT_INFO* mappedGameObjects = nullptr;

	UINT											cbElementBytes = 0;

	D3D12_GPU_DESCRIPTOR_HANDLE						baseCbvGpu = { 0 };
	UINT											cbvInc = 0;

	ComPtr<ID3D12Resource>							cbBonePalette;
	CB_BONE_PALETTE* mappedBonePalette = nullptr;
	UINT											boneCbBytes = 0;

	std::shared_ptr<CMaterial>						material;		// legacy 있으면 사용
};

class CScene
{
public:
	CScene();
	~CScene();

	void ReleaseObjects();
	virtual void ReleaseShaderVariables();
	void ReleaseUploadBuffers();

	// Build
public:
	void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	void BuildLightsAndMaterials();

	void CreateGraphicsRootSignature(ID3D12Device* pd3dDevice);
	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
protected:
	void BuildStaticBatch(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		const std::shared_ptr<CStaticObjectsShader>& pStaticShader,
		UINT nRenderTargets,
		DXGI_FORMAT* rtvFormats,
		DXGI_FORMAT dsvFormat
	);

	void BuildSkinnedBatch(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		const std::shared_ptr<CSkinnedObjectsShader>& pSkinnedShader,
		UINT nRenderTargets,
		DXGI_FORMAT* rtvFormats,
		DXGI_FORMAT dsvFormat
	);
	// Render
public:
	bool ProcessInput(UCHAR* pKeysBuffer);
	void AnimateObjects(float fTimeElapsed);

	void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera = NULL);

	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);

	// Input
public:
	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	// Get & Set Method
public:
	ID3D12RootSignature* GetGraphicsRootSignature() { return(m_pd3dGraphicsRootSignature.Get()); }
	void SetGraphicsRootSignature(ID3D12GraphicsCommandList* pd3dCommandList) { pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get()); }

public:
	std::shared_ptr<CPlayer>					m_pPlayer;
	static std::unique_ptr<CDescriptorHeap>		m_pDescriptorHeap;

protected:
	ComPtr<ID3D12RootSignature>				m_pd3dGraphicsRootSignature;

	std::vector<std::shared_ptr<CShader>>	m_ppShaders;
	int										m_nShaders = 0;

	std::unique_ptr<LIGHTS>					m_pLights;

	ComPtr<ID3D12Resource>					m_pd3dcbLights;
	LIGHTS* m_pcbMappedLights = nullptr;

	std::unique_ptr<MATERIALS>				m_pMaterials;

	ComPtr<ID3D12Resource>					m_pd3dcbMaterials;
	MATERIAL* m_pcbMappedMaterials = nullptr;

public:
	void SetMaterialDiffuseSrvIndex(int materialId, UINT srvIndex)
	{
		m_pMaterials->m_pReflections[materialId].m_xmn4TextureIndices.x = srvIndex;
	}

public:
	// [ADD] batches (not used yet)
	SCENE_STATIC_BATCH						m_staticBatch;
	SCENE_SKINNED_BATCH						m_skinnedBatch;
};
