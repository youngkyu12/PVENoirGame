//------------------------------------------------------- ----------------------
// File: Mesh.h
//-----------------------------------------------------------------------------
#pragma once
#include "stdafx.h"
#include "AnimatorData.h"
#include "Material.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CAnimator;
class CMaterial;
struct CB_GAMEOBJECT_INFO;

class CVertex
{
public:
	CVertex() { m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f); }
	CVertex(float x, float y, float z) { m_xmf3Position = XMFLOAT3(x, y, z); }
	CVertex(XMFLOAT3 xmf3Position) { m_xmf3Position = xmf3Position; }
	~CVertex() {}

public:
	XMFLOAT3						m_xmf3Position;
};

class CPolygon
{
public:
	CPolygon() {}
	CPolygon(int nVertices);
	~CPolygon();

	int							m_nVertices = 0;
	CVertex* m_pVertices = NULL;

	void SetVertex(int nIndex, CVertex& vertex);
};

class CDiffusedVertex : public CVertex
{
public:
	XMFLOAT4 m_xmf4Diffuse;
	XMFLOAT3 m_xmf3Normal;

public:
	CDiffusedVertex() { m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f); m_xmf4Diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f); }
	CDiffusedVertex(float x, float y, float z, XMFLOAT4 xmf4Diffuse) { m_xmf3Position = XMFLOAT3(x, y, z); m_xmf4Diffuse = xmf4Diffuse; }
	CDiffusedVertex(XMFLOAT3 xmf3Position, XMFLOAT4 xmf4Diffuse) { m_xmf3Position = xmf3Position; m_xmf4Diffuse = xmf4Diffuse; }
	~CDiffusedVertex() {}
};

struct BinMaterial
{
	std::string name;               // material 이름 (ex: "face")
	std::string diffuseTextureName; // texture base (ex: "face_00")
};

struct SubMesh
{
	// CPU (필요 시만 유지: 피킹/디버그)
	vector<XMFLOAT3> positions;
	vector<XMFLOAT3> normals;
	vector<XMFLOAT2> uvs;
	vector<XMUINT4>  boneIndices;
	vector<XMFLOAT4> boneWeights;
	vector<UINT>     indices;

	std::string meshName;
	std::string materialName;              // 로딩용/디버그용
	std::string diffuseTextureName;
	shared_ptr<CMaterial> material;         // (권장) 렌더용 연결
	uint32_t materialIndex = 0xFFFFFFFFu; // BIN의 g_Materials 인덱스
	UINT     materialId = 0xFFFFFFFFu; // 셰이더로 넘길 gnMaterialID (현재는 materialIndex와 동일)


	// GPU
	ID3D12Resource* vb = nullptr;
	ID3D12Resource* vbUpload = nullptr;
	ID3D12Resource* ib = nullptr;
	ID3D12Resource* ibUpload = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vbView{};
	D3D12_INDEX_BUFFER_VIEW  ibView{};
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CMesh
{
public:
	CMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual ~CMesh();

public:
	void ReleaseUploadBuffers();
	void SetPolygon(int nIndex, CPolygon* pPolygon);
	int CheckRayIntersection(XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection, float* pfNearHitDistance = nullptr);
	BOOL RayIntersectionByTriangle(XMVECTOR& xmRayOrigin, XMVECTOR& xmRayDirection, XMVECTOR v0, XMVECTOR v1, XMVECTOR v2, float* pfNearHitDistance);


protected:
	D3D12_PRIMITIVE_TOPOLOGY		m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	UINT							m_nSlot = 0;

	int							    m_nPolygons = 0;
	CPolygon**						m_ppPolygons = NULL;

	vector<Bone>					m_Bones;            // 본 배열
	unordered_map<string, int>		m_BoneNameToIndex;  // 본 이름 -> 인덱스

	XMUINT4*						m_pxu4BoneIndices = NULL;   // 정점별 본 인덱스(4)
	XMFLOAT4* 						m_pxmf4BoneWeights = NULL;  // 정점별 본 가중치(4)
	XMFLOAT4X4*						m_pxmf4x4BoneTransforms = NULL;  // CPU Bone Matrices
	ID3D12Resource*					m_pd3dcbBoneTransforms = NULL; // GPU Bone CB

	ID3D12Resource*					m_pd3dBoneIndexBuffer = NULL;
	ID3D12Resource*					m_pd3dBoneIndexUploadBuffer = NULL;
	ID3D12Resource*					m_pd3dBoneWeightBuffer = NULL;
	ID3D12Resource*					m_pd3dBoneWeightUploadBuffer = NULL;

	CAnimator*						m_pAnimator = nullptr;
	bool							m_bSkinnedMesh = false;
	ID3D12Device*					m_pd3dDevice = nullptr;

public:
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CB_GAMEOBJECT_INFO* pMappedGameObjectCB);

	D3D12_GPU_VIRTUAL_ADDRESS GetBoneCBAddress() const {
		return m_pd3dcbBoneTransforms
			? m_pd3dcbBoneTransforms->GetGPUVirtualAddress()
			: 0;
	}

	void LoadMeshFromBIN(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const char* filename);
	void EnableSkinning(int nBones);

	vector<SubMesh> m_SubMeshes;
	void LinkMaterials(
		std::unordered_map<std::string, std::shared_ptr<CMaterial>>& materialCache,
		const std::function<std::shared_ptr<CMaterial>(const std::string&)>& createMaterialIfMissing = nullptr
	);

	// CAnimator 연동을 위한 헬퍼 함수들
	int GetBoneCount() const;
	int GetBoneIndexByName(const std::string& boneName) const;
	int GetBoneParentIndex(int boneIndex) const;
	const std::vector<Bone>& GetBones() const { return m_Bones; }

	CAnimator* GetAnimator() const { return m_pAnimator; }
	CAnimator* EnsureAnimator();

	void UpdateBoneTransformsOnGPU(ID3D12GraphicsCommandList* cmdList,
		const XMFLOAT4X4* boneMatrices,
		int nBones);

	bool IsSkinnedMesh() const { return m_bSkinnedMesh; }
	bool HasBoneCB() const { return (m_pd3dcbBoneTransforms != nullptr); }

	bool LoadAnimationFromBIN(const char* filename,
		const std::string& clipName,
		AnimationClip& outClip,
		float timeScale = 1.0f);

	CAnimator* GetAnimator() { return m_pAnimator; }
	bool HasAnimator() const { return m_pAnimator != nullptr; }

public:
	const std::vector<BinMaterial>& GetBinMaterials() const { return m_BinMaterials; }

private:
	std::vector<BinMaterial> m_BinMaterials;
	std::unordered_map<std::string, uint32_t> m_BinMaterialNameToIndex; // 있으면 편함(선택)

};
