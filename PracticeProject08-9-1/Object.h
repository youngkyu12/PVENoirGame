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


#define DIR_FORWARD					0x01
#define DIR_BACKWARD				0x02
#define DIR_LEFT					0x04
#define DIR_RIGHT					0x08
#define DIR_UP						0x10
#define DIR_DOWN					0x20

class CMesh;
class CShader;
class CAnimator;
class CAnimController;
class CComponent;

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
	CGameObject(int nMeshes=1);
	virtual ~CGameObject();

	virtual void ReleaseShaderVariables();
	virtual void ReleaseUploadBuffers();

// Build
public:
	virtual void CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList);
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
	void Rotate(XMFLOAT3 *pxmf3Axis, float fAngle);

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
	void SetMesh(int nIndex, shared_ptr<CMesh> pMesh);

	void SetCbvGPUDescriptorHandle(D3D12_GPU_DESCRIPTOR_HANDLE d3dCbvGPUDescriptorHandle) { m_d3dCbvGPUDescriptorHandle = d3dCbvGPUDescriptorHandle; }
	void SetCbvGPUDescriptorHandlePtr(UINT64 nCbvGPUDescriptorHandlePtr) { m_d3dCbvGPUDescriptorHandle.ptr = nCbvGPUDescriptorHandlePtr; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetCbvGPUDescriptorHandle() { return(m_d3dCbvGPUDescriptorHandle); }

	virtual void SetRootParameter(ID3D12GraphicsCommandList* cmd)
	{
		// 기존: per-object CBV descriptor table
		cmd->SetGraphicsRootDescriptorTable(ROOT_PARAMETER_OBJECT, m_d3dCbvGPUDescriptorHandle);

		// 추가: skinned면 b7(root param 7)에 bone palette CBV 바인딩
		if (m_bSkinnedObject)
		{
			cmd->SetGraphicsRootConstantBufferView(
				ROOT_PARAMETER_BONE_PALETTE,   // == b7
				m_pd3dcbBoneTransforms->GetGPUVirtualAddress()
			);
		}
	}

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
	vector<shared_ptr<CMesh>>		m_ppMeshes;
	int								m_nMeshes = 0;

	D3D12_GPU_DESCRIPTOR_HANDLE		m_d3dCbvGPUDescriptorHandle = { 0 };

	// ================================
	// Animation / Skinning (per-object)
	// ================================
	void EnableSkinning(ID3D12Device* pd3dDevice, int nBones);
	void DisableSkinning();

	bool IsSkinnedObject() const { return m_bSkinnedObject; }
	int  GetBoneCount()    const { return m_nBones; }

	// Mesh가 들고 있는 "스켈레톤 메타데이터" 접근 (forward)
	const std::vector<Bone>& GetBones() const;
	const std::unordered_map<std::string, int>& GetBoneNameToIndex() const;

	// Bone palette CB (b7 등에 바인딩할 GPU VA)
	D3D12_GPU_VIRTUAL_ADDRESS GetBoneCBAddress() const;

	// Animator는 오브젝트가 소유 (메시 공유 대비)
	CAnimator* EnsureAnimator();
	CAnimator* GetAnimator() const { return m_pAnimator.get(); }
	void PlayAnimation(const std::string& name, bool loop = true, float start = 0.0f);

	// CPU -> GPU 팔레트 업데이트 (Animator에서 만든 최종 본 행렬 업로드)
	void UpdateBoneTransformsOnGPU(const XMFLOAT4X4* pxmf4x4BoneTransforms, int nBones);

protected:
	// --------------------
	// Components (owned)
	// --------------------
	CTransformComponent* m_pTransform = nullptr;
	CTransformComponent* GetTransform() const { return m_pTransform; }
	std::vector<std::unique_ptr<CComponent>> m_components;

	bool m_bComponentsCreated = false;
	ID3D12Device* m_pd3dDeviceForComponents = nullptr;
	ID3D12GraphicsCommandList* m_pd3dCmdForComponents = nullptr;

	ComPtr<ID3D12Resource>			m_pd3dcbGameObject;
	CB_GAMEOBJECT_INFO* m_pcbMappedGameObject = nullptr;

	// --------------------
	// Skinning (per-object)
	// --------------------
	bool                                m_bSkinnedObject = false;
	int                                 m_nBones = 0;

	ComPtr<ID3D12Resource>              m_pd3dcbBoneTransforms = NULL;   // Upload heap CB
	
	// Animator (per-object)
	std::unique_ptr<CAnimator>          m_pAnimator;

public:
	void SetMappedGameObjectCB(CB_GAMEOBJECT_INFO* p) { m_pcbMappedGameObject = p; }
	CB_GAMEOBJECT_INFO* GetMappedGameObjectCB() const { return m_pcbMappedGameObject; }

protected:
	XMFLOAT4X4* m_pcbMappedBoneTransforms = nullptr;

public:
		CAnimController* EnsureAnimController();
		CAnimController* GetAnimController() const { return m_pAnimController.get(); }

protected:
	std::unique_ptr<CAnimController> m_pAnimController;

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

	m_components.emplace_back(std::move(comp));

	// 이미 CreateComponents가 끝난 상태면 즉시 OnCreate 호출
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
