//-----------------------------------------------------------------------------
// File: Scene.h
//-----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <vector>
#include <array>

#include "Shader.h"
#include "DescriptorHeap.h"
#include "LightTypes.h"

#define ROOT_PARAMETER_DRAW_OPTIONS 5
#define ROOT_PARAMETER_GLOBAL_SRV 6
constexpr UINT LEGACY_SRV_COUNT = 6; // t0(1) + t1~t5(5)

class CMaterial;
class CGameObject;
class CFollowTransformComponent;
class CArrowComponent;
class CColliderComponent;

struct CB_GAMEOBJECT_INFO;
struct CB_BONE_PALETTE;

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
// Scene-owned batches
// ============================================================================
struct SCENE_STATIC_BATCH
{
	std::shared_ptr<CStaticObjectsShader>            shader;

	UINT                                             capacity = 0;   // �ִ� ����
	UINT                                             count = 0;      // ���� ��(=objects.size())

	std::vector<CGameObject*>                        objectRefs;

	ComPtr<ID3D12Resource>                           cbGameObjects;
	CB_GAMEOBJECT_INFO* mappedGameObjects = nullptr;

	UINT                                             cbElementBytes = 0;

	D3D12_GPU_DESCRIPTOR_HANDLE                      baseCbvGpu = { 0 };
	UINT                                             cbvInc = 0;

	std::shared_ptr<CMaterial>                       material;       // legacy ������ ���
};

struct SCENE_SKINNED_BATCH
{
	std::shared_ptr<CSkinnedObjectsShader>           shader;

	UINT                                             capacity = 0;   // �ִ� ����
	UINT                                             count = 0;      // ���� ��(=objects.size())

	UINT                                             cbElementBytes = 0;

	ComPtr<ID3D12Resource>                           cbGameObjects;
	CB_GAMEOBJECT_INFO* mappedGameObjects = nullptr;

	D3D12_GPU_DESCRIPTOR_HANDLE                      baseCbvGpu = { 0 };
	UINT                                             cbvInc = 0;

	std::vector<CGameObject*>                        objectRefs;
};

class CScene
{
public:
	CScene();
	~CScene();

	// Lifecycle / Release
public:
	void ReleaseObjects();
	virtual void ReleaseShaderVariables();
	void ReleaseUploadBuffers();

	// Build
public:
	void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	void BuildLightsAndMaterials();

	void CreateGraphicsRootSignature(ID3D12Device* pd3dDevice);
	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

	void BuildPlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	void CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* target);

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

	// Frame / Render
public:
	bool ProcessInput(UCHAR* pKeysBuffer);
	void AnimateObjects(float fTimeElapsed);

	void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera = NULL);

	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);

	void CollisionUpdate();

	// Input (messages)
public:
	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

	// Get / Set
public:
	ID3D12RootSignature* GetGraphicsRootSignature() { return(m_pd3dGraphicsRootSignature.Get()); }
	void SetGraphicsRootSignature(ID3D12GraphicsCommandList* pd3dCommandList) { pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get()); }

	CCamera* GetMainCamera() const { return m_pMainCamera; }
	CGameObject* GetMainCameraObject() const { return m_pMainCameraObject.get(); }

	CGameObject* GetPlayer() const { return m_pPlayer.get(); }
	std::shared_ptr<CGameObject> GetPlayerShared() const { return m_pPlayer; }

	void SetMaterialDiffuseSrvIndex(int materialId, UINT srvIndex)
	{
		m_pMaterials->m_pReflections[materialId].m_xmn4TextureIndices.x = srvIndex;
	}

	CGameObject* GetDemoFighter(int index) const;
	void RequestDemoFighterAttack(int index);

	CGameObject* GetPlayerBySlot(int slot) const; // slot: 0..3
	bool IsLocalPlayer(const CGameObject* obj) const;
	void RequestPlayerAttackBySlot(int slot); // slot: 0..3
	void RequestFireArrow(CGameObject* shooter, float speed, float lifeSec = 3.0f, float yOffset = 0.0f);

public:
	static std::unique_ptr<CDescriptorHeap>		m_pDescriptorHeap;

	// Scene State / Owned Objects
public:
	std::shared_ptr<CGameObject>					m_pPlayer;

	std::unique_ptr<CGameObject>					m_pMainCameraObject;  // Scene�� �����ϴ� �� ������Ʈ
	CCamera* m_pMainCamera = nullptr; // ������Ʈ �� ī�޶� ������Ʈ ĳ��

	SCENE_STATIC_BATCH								m_staticBatch;
	SCENE_SKINNED_BATCH								m_skinnedBatch;

	std::vector<std::unique_ptr<CGameObject>>		m_staticObjects;
	std::vector<std::unique_ptr<CGameObject>>		m_skinnedObjects;
	
	static constexpr UINT							kArrowPoolSize = 32;
	std::vector<CGameObject*>						m_arrowRefs; // size = kArrowPoolSize, raw pointers (owned by m_staticObjects)

	std::array<CGameObject*, 3>						m_demoFighters = { nullptr, nullptr, nullptr };

	std::vector<std::unique_ptr<CGameObject>>		m_lightObjects;
	CFollowTransformComponent* m_pPlayerSpotFollower = nullptr;

	std::vector<CColliderComponent> m_ColliderBoxes;

	// GPU / Shader Variables (Scene-owned)
protected:
	ComPtr<ID3D12RootSignature>						m_pd3dGraphicsRootSignature;

	ComPtr<ID3D12Resource>							m_pd3dcbLights;
	LIGHTS* m_pcbMappedLights = nullptr;

	std::unique_ptr<MATERIALS>						m_pMaterials;

	ComPtr<ID3D12Resource>							m_pd3dcbMaterials;
	MATERIAL* m_pcbMappedMaterials = nullptr;

	ComPtr<ID3D12Resource>							m_pd3dcbPlayerGameObject;
	CB_GAMEOBJECT_INFO* m_pcbMappedPlayerGameObject = nullptr;
	D3D12_GPU_DESCRIPTOR_HANDLE						m_playerCbvGpu = { 0 };
	UINT											m_playerCbElementBytes = 0;
};
