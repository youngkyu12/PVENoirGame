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

struct CB_GAMEOBJECT_INFO
{
	XMFLOAT4X4  m_xmf4x4World;
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

	virtual void ReleaseShaderVariables();
	virtual void ReleaseUploadBuffers();

	// Build
public:
	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void BuildMaterials(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) {}

	// Render
public:
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);

	virtual void Animate(float fTimeElapsed);

	virtual void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera = nullptr);

	void MoveStrafe(float fDistance = 1.0f);
	void MoveUp(float fDistance = 1.0f);
	void MoveForward(float fDistance = 1.0f);

	void Rotate(float fPitch = 10.0f, float fYaw = 10.0f, float fRoll = 10.0f);
	void Rotate(XMFLOAT3* pxmf3Axis, float fAngle);

public:
	void CreateComponents(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	void DestroyComponents();
	void UpdateComponents(float fTimeElapsed);

	template<typename T, typename... Args>
	T* AddComponent(Args&&... args);

	template<typename T>
	T* GetComponent() const;

	// Get & Set Method
public:
	// ===== Model (Meshes) accessors =====
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

	void SetCbvGPUDescriptorHandle(D3D12_GPU_DESCRIPTOR_HANDLE d3dCbvGPUDescriptorHandle);
	void SetCbvGPUDescriptorHandlePtr(UINT64 nCbvGPUDescriptorHandlePtr);
	D3D12_GPU_DESCRIPTOR_HANDLE GetCbvGPUDescriptorHandle();

	virtual void SetRootParameter(ID3D12GraphicsCommandList* cmd);

	// ===== Transform authoritative getters =====
	XMFLOAT3 GetPosition() const { return m_pTransform->position; }
	XMFLOAT3 GetLook()     const { return m_pTransform->GetLook(); }
	XMFLOAT3 GetUp()       const { return m_pTransform->GetUp(); }
	XMFLOAT3 GetRight()    const { return m_pTransform->GetRight(); }

	const XMFLOAT4X4& GetWorldMatrix() const { return m_pTransform->GetWorldMatrix(); }

	void SetPosition(float x, float y, float z)
	{
		m_pTransform->SetPosition(XMFLOAT3(x, y, z));
	}
	void SetPosition(XMFLOAT3 p) { SetPosition(p.x, p.y, p.z); }

	// 호환용: 외부 월드행렬 입력이 필요할 때도 Transform만 변경(권위 유지)
	void SetWorldMatrix(const XMFLOAT4X4& W)
	{
		m_pTransform->SetWorldMatrixFromMatrix(W);
	}

public:
	D3D12_GPU_DESCRIPTOR_HANDLE		m_d3dCbvGPUDescriptorHandle = { 0 };

	// ================================
	// Animation / Skinning (per-object)
	// ================================
	void EnableSkinning(ID3D12Device* pd3dDevice, int nBones);
	void DisableSkinning();

	bool IsSkinnedObject() const;
	int  GetBoneCount()    const;

	// Mesh가 들고 있는 "스켈레톤 메타데이터" 접근 (forward)
	const std::vector<Bone>& GetBones() const;
	const std::unordered_map<std::string, int>& GetBoneNameToIndex() const;

	// Bone palette CB (b7 등에 바인딩할 GPU VA)
	D3D12_GPU_VIRTUAL_ADDRESS GetBoneCBAddress() const;

	// Animator는 오브젝트가 소유 (메시 공유 대비)
	CAnimator* EnsureAnimator();
	CAnimator* GetAnimator() const;
	void PlayAnimation(const std::string& name, bool loop = true, float start = 0.0f);

	// CPU -> GPU 팔레트 업데이트 (Animator에서 만든 최종 본 행렬 업로드)
	void UpdateBoneTransformsOnGPU(const XMFLOAT4X4* pxmf4x4BoneTransforms, int nBones);

	void SetColliderType(EColliderType Type);

protected:
	// --------------------
	// Components (owned)
	// --------------------
	CTransformComponent* m_pTransform = nullptr;
	CTransformComponent* GetTransform() const { return m_pTransform; }
	CRendererComponent* m_pRenderer = nullptr;
	CModelComponent* m_pModel = nullptr;
	CAnimatorComponent* m_pAnimatorComponent = nullptr;
	CRenderObjectComponent* m_pRenderObject = nullptr;
	CSkinningComponent* m_pSkinning = nullptr;
	std::vector<std::unique_ptr<CComponent>> m_components;

	bool m_bComponentsCreated = false;
	ID3D12Device* m_pd3dDeviceForComponents = nullptr;
	ID3D12GraphicsCommandList* m_pd3dCmdForComponents = nullptr;
	EColliderType mColliderType = EColliderType::None;

public:
	CRendererComponent* GetRenderer()
	{
		if (m_pRenderer) return m_pRenderer;

		for (auto& c : m_components)
		{
			if (c && c->IsRenderer())
			{
				m_pRenderer = static_cast<CRendererComponent*>(c.get());
				return m_pRenderer;
			}
		}
		return nullptr;
	}
	CAnimatorComponent* GetAnimatorComponent();
	const CAnimatorComponent* GetAnimatorComponent() const;

	CAnimatorComponent* EnsureAnimatorComponent();

protected:
	const CRendererComponent* GetRenderer() const
	{
		return const_cast<CGameObject*>(this)->GetRenderer();
	}
	ComPtr<ID3D12Resource>			m_pd3dcbGameObject;
	CB_GAMEOBJECT_INFO* m_pcbMappedGameObject = nullptr;

	void PreRenderComponents(ID3D12GraphicsCommandList* cmd);
	void PostRenderComponents(ID3D12GraphicsCommandList* cmd);

	// --------------------
	// Skinning (per-object)
	// --------------------
	bool                                m_bSkinnedObject = false;
	int                                 m_nBones = 0;

	ComPtr<ID3D12Resource>              m_pd3dcbBoneTransforms = NULL;   // Upload heap CB


public:
	void SetMappedGameObjectCB(CB_GAMEOBJECT_INFO* p);
	CB_GAMEOBJECT_INFO* GetMappedGameObjectCB() const;

protected:
	XMFLOAT4X4* m_pcbMappedBoneTransforms = nullptr;

public:
	CAnimController* EnsureAnimController();
	CAnimController* GetAnimController() const;

protected:
	std::shared_ptr<CShader> m_pShader;

public:
	void SetShader(std::shared_ptr<CShader> pShader) { m_pShader = std::move(pShader); }
	std::shared_ptr<CShader> GetShader() const { return m_pShader; }

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

	// ★ Renderer면 캐시 갱신 (빈오브젝트면 안 붙이면 됨)
	if (raw)
	{
		CComponent* base = raw; // 업캐스트는 항상 안전/가능
		if (base->IsRenderer())
			m_pRenderer = static_cast<CRendererComponent*>(base); // base->derived 다운캐스트 (IsRenderer로 논리 보증)
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
