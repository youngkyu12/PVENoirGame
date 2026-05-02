//------------------------------------------------------- ----------------------
// File: Object.h
//-----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <vector>
#include <type_traits>

#include "Mesh.h"
#include "Camera.h"
#include "Animator.h"
#include "Component.h"
#include "RendererComponent.h"
#include "ModelComponent.h"
#include "ColliderComponent.h"

#define DIR_FORWARD					0x01
#define DIR_BACKWARD				0x02
#define DIR_LEFT					0x04
#define DIR_RIGHT					0x08
#define DIR_UP						0x10
#define DIR_DOWN					0x20

class CShader;
class CAnimator;
class CComponent;
class CAnimatorComponent;
class CAnimController;
class CRenderObjectComponent;
class CSkinningComponent;
class CColliderComponent;

struct CB_GAMEOBJECT_INFO
{
	XMFLOAT4X4  m_xmf4x4World;
	XMFLOAT4X4 TexTransform = Matrix4x4::Identity();
	XMFLOAT4X4 ShadowTransform = Matrix4x4::Identity();
	UINT        m_nObjectID;
	UINT        _pad[3];   // 16-byte align
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CGameObject
{
public:
	CGameObject(int nMeshes = 1);
	virtual ~CGameObject();

	// Lifecycle / Release
public:
	virtual void ReleaseShaderVariables();
	virtual void ReleaseUploadBuffers();

	// Build
public:
	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void BuildMaterials(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) {}

	void CreateComponents(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	void DestroyComponents();
	void UpdateComponents(float fTimeElapsed);

	// Render
public:
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);

	virtual void Animate(float fTimeElapsed);

	virtual void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera = nullptr);

	// Camera Visible
public:
	bool IsVisible(CCamera* pCamera = nullptr);

	// Movement helpers (legacy convenience)
public:
	void MoveStrafe(float fDistance = 1.0f);
	void MoveUp(float fDistance = 1.0f);
	void MoveForward(float fDistance = 1.0f);

	void Rotate(float fPitch = 10.0f, float fYaw = 10.0f, float fRoll = 10.0f);
	void Rotate(XMFLOAT3* pxmf3Axis, float fAngle);

	// Components API
public:
	template<typename T, typename... Args>
	T* AddComponent(Args&&... args);

	template<typename T>
	T* GetComponent() const;

	CRendererComponent* GetRenderer();
	const CRendererComponent* GetRenderer() const;

	CAnimatorComponent* GetAnimatorComponent();
	const CAnimatorComponent* GetAnimatorComponent() const;
	CAnimatorComponent* EnsureAnimatorComponent();

	// Model (Meshes)
public:
	CModelComponent* GetModel() const { return m_pModel; }

	int GetMeshCount() const
	{
		return (m_pModel) ? m_pModel->GetMeshCount() : 0;
	}

	std::shared_ptr<CMesh> GetMeshShared(int idx) const
	{
		return (m_pModel) ? m_pModel->GetMeshShared(idx) : nullptr;
	}

	std::vector<std::shared_ptr<CMesh>>& GetMeshes()
	{
		static std::vector<std::shared_ptr<CMesh>> dummy;
		return (m_pModel) ? m_pModel->GetMeshes() : dummy;
	}

	const std::vector<std::shared_ptr<CMesh>>& GetMeshes() const
	{
		static std::vector<std::shared_ptr<CMesh>> dummy;
		return (m_pModel) ? m_pModel->GetMeshes() : dummy;
	}

	void SetMesh(int nIndex, shared_ptr<CMesh> pMesh);

	// RenderObject / Root Binding
public:
	void SetCbvGPUDescriptorHandle(D3D12_GPU_DESCRIPTOR_HANDLE d3dCbvGPUDescriptorHandle);
	void SetCbvGPUDescriptorHandlePtr(UINT64 nCbvGPUDescriptorHandlePtr);
	D3D12_GPU_DESCRIPTOR_HANDLE GetCbvGPUDescriptorHandle();

	virtual void SetRootParameter(ID3D12GraphicsCommandList* cmd);

	void SetMappedGameObjectCB(CB_GAMEOBJECT_INFO* p);
	CB_GAMEOBJECT_INFO* GetMappedGameObjectCB() const;

	// Transform (authoritative)
public:
	XMFLOAT3 GetPosition() const { return m_pTransform->position; }
	XMFLOAT3 GetLook()     const { return m_pTransform->GetLook(); }
	XMFLOAT3 GetUp()       const { return m_pTransform->GetUp(); }
	XMFLOAT3 GetRight()    const { return m_pTransform->GetRight(); }

	const XMFLOAT4X4& GetWorldMatrix() const { return m_pTransform->GetWorldMatrix(); }

	void SetPosition(float x, float y, float z)
	{
		m_pTransform->SetPosition(XMFLOAT3(x, y, z));
		if ( m_pCollider )
			m_pCollider->UpdateWorldBounds();
	}
	void SetPosition(XMFLOAT3 p) { SetPosition(p.x, p.y, p.z); }

	void SetWorldMatrix(const XMFLOAT4X4& W)
	{
		m_pTransform->SetWorldMatrixFromMatrix(W);
		if ( m_pCollider )
			m_pCollider->UpdateWorldBounds();
	}

	// Animation / Skinning (per-object)
public:
	void EnableSkinning(ID3D12Device* pd3dDevice, int nBones);
	void DisableSkinning();

	bool IsSkinnedObject() const;
	int  GetBoneCount()    const;

	const std::vector<Bone>& GetBones() const;
	const std::unordered_map<std::string, int>& GetBoneNameToIndex() const;

	D3D12_GPU_VIRTUAL_ADDRESS GetBoneCBAddress() const;

	CAnimator* EnsureAnimator();
	CAnimator* GetAnimator() const;
	void PlayAnimation(const std::string& name, bool loop = true, float start = 0.0f);

	void UpdateBoneTransformsOnGPU(const XMFLOAT4X4* pxmf4x4BoneTransforms, int nBones);

	CAnimController* EnsureAnimController();
	CAnimController* GetAnimController() const;

	void SetId(int id) { m_ID = id; }
	void SetActive(bool b) { m_bEnabled = b; }

	// Shader
public:
	void SetShader(std::shared_ptr<CShader> pShader) { m_pShader = std::move(pShader); }
	std::shared_ptr<CShader> GetShader() const { return m_pShader; }

	// Public state (legacy)
public:
	D3D12_GPU_DESCRIPTOR_HANDLE		m_d3dCbvGPUDescriptorHandle = { 0 };

protected:
	// Components (owned)
	CTransformComponent* m_pTransform = nullptr;
	CTransformComponent* GetTransform() const { return m_pTransform; }
	CRendererComponent* m_pRenderer = nullptr;
	CModelComponent* m_pModel = nullptr;
	CAnimatorComponent* m_pAnimatorComponent = nullptr;
	CRenderObjectComponent* m_pRenderObject = nullptr;
	CSkinningComponent* m_pSkinning = nullptr;
	CColliderComponent* m_pCollider = nullptr;
	std::vector<std::unique_ptr<CComponent>> m_components;

	bool m_bComponentsCreated = false;
	ID3D12Device* m_pd3dDeviceForComponents = nullptr;
	ID3D12GraphicsCommandList* m_pd3dCmdForComponents = nullptr;

	int m_ID = 0;
	bool m_bEnabled = true;

	// Legacy / fallback resources (kept as-is)
	ComPtr<ID3D12Resource>			m_pd3dcbGameObject;
	CB_GAMEOBJECT_INFO* m_pcbMappedGameObject = nullptr;

	void PreRenderComponents(ID3D12GraphicsCommandList* cmd);
	void PostRenderComponents(ID3D12GraphicsCommandList* cmd);

	// Skinning (legacy fields kept)
	bool                                m_bSkinnedObject = false;
	int                                 m_nBones = 0;

	ComPtr<ID3D12Resource>              m_pd3dcbBoneTransforms = NULL;   // Upload heap CB
	XMFLOAT4X4* m_pcbMappedBoneTransforms = nullptr;

	// Shader (per-object)
	std::shared_ptr<CShader> m_pShader;
};



// ============================================================================
// CGameObject - Component templates (header-only)
// ============================================================================
template<typename T, typename... Args>
T* CGameObject::AddComponent(Args&&... args)
{
	static_assert(std::is_base_of<CComponent, T>::value, "T must derive from CComponent");

	auto comp = std::make_unique<T>(this, std::forward<Args>(args)...);
	T* raw = comp.get();

	if (raw)
	{
		CComponent* base = raw;
		if (base->IsRenderer())
			m_pRenderer = static_cast<CRendererComponent*>(base); 

		if constexpr ( std::is_same_v<T, CTransformComponent> )
			m_pTransform = raw;
		else if constexpr ( std::is_same_v<T, CModelComponent> )
			m_pModel = raw;
		else if constexpr ( std::is_same_v<T, CRenderObjectComponent> )
			m_pRenderObject = raw;
		else if constexpr ( std::is_same_v<T, CSkinningComponent> )
			m_pSkinning = raw;
		else if constexpr ( std::is_same_v<T, CAnimatorComponent> )
			m_pAnimatorComponent = raw;
		else if constexpr ( std::is_same_v<T, CColliderComponent> )
			m_pCollider = raw;
	}

	m_components.emplace_back(std::move(comp));

	if (m_bComponentsCreated && raw)
		raw->OnCreate(m_pd3dDeviceForComponents, m_pd3dCmdForComponents);

	return raw;
}

template<typename T>
T* CGameObject::GetComponent() const
{
	const auto want = CComponent::StaticTypeId<T>();

	for (const auto& c : m_components)
	{
		if (c && c->GetTypeId() == want)
			return static_cast<T*>(c.get());
	}
	return nullptr;
}
