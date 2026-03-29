//------------------------------------------------------- ----------------------
// File: Mesh.h
//-----------------------------------------------------------------------------
#pragma once
#include "stdafx.h"
#include "AnimatorData.h"
#include "Material.h"
#include "ColliderComponent.h"

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

struct BinMaterialTexTransform
{
	XMFLOAT4 uvST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f); // x=scaleU, y=scaleV, z=offsetU, w=offsetV
	UINT wrapModeU = 0; // 0=Repeat, 1=Clamp
	UINT wrapModeV = 0; // 0=Repeat, 1=Clamp
};

struct BinMaterial
{
	std::string name;
	std::string diffuseTextureName;
	std::string normalTextureName;
	std::string emissiveTextureName;
	std::string specularTextureName;

	XMFLOAT4 diffuseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	XMFLOAT4 emissiveColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	XMFLOAT4 specularColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f); // a=shininess

	BinMaterialTexTransform diffuseTransform{};
	BinMaterialTexTransform normalTransform{};
	BinMaterialTexTransform emissiveTransform{};
	BinMaterialTexTransform specularTransform{};
};

struct SubMesh
{
	// CPU (필요 시만 유지: 피킹/디버그)
	vector<XMFLOAT3> positions;
	vector<XMFLOAT3> normals;
	vector<XMFLOAT2> uvs;
	vector<XMFLOAT4> tangents;
	vector<XMUINT4>  boneIndices;
	vector<XMFLOAT4> boneWeights;
	vector<UINT>     indices;
	XMFLOAT3 subMeshMin{ FLT_MAX,  FLT_MAX,  FLT_MAX };
	XMFLOAT3 subMeshMax{ -FLT_MAX, -FLT_MAX, -FLT_MAX };

	std::string meshName;
	std::string materialName;
	std::string diffuseTextureName;
	std::string normalTextureName;
	std::string emissiveTextureName;
	std::string specularTextureName;

	XMFLOAT4 diffuseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	XMFLOAT4 emissiveColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	XMFLOAT4 specularColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	BinMaterialTexTransform diffuseTransform{};
	BinMaterialTexTransform normalTransform{};
	BinMaterialTexTransform emissiveTransform{};
	BinMaterialTexTransform specularTransform{};

	shared_ptr<CMaterial> material;
	uint32_t materialIndex = 0xFFFFFFFFu;
	UINT     materialId = 0xFFFFFFFFu;

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
	XMFLOAT3 GetMeshMin() const;
	XMFLOAT3 GetMeshMax() const;

protected:
	D3D12_PRIMITIVE_TOPOLOGY		m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	UINT							m_nSlot = 0;

	int							    m_nPolygons = 0;
	CPolygon**						m_ppPolygons = NULL;

	vector<Bone>					m_Bones;            // 본 배열
	unordered_map<string, int>		m_BoneNameToIndex;  // 본 이름 -> 인덱스

	XMUINT4*						m_pxu4BoneIndices = NULL;   // 정점별 본 인덱스(4)
	XMFLOAT4* 						m_pxmf4BoneWeights = NULL;  // 정점별 본 가중치(4)
	
	ID3D12Resource*					m_pd3dBoneIndexBuffer = NULL;
	ID3D12Resource*					m_pd3dBoneIndexUploadBuffer = NULL;
	ID3D12Resource*					m_pd3dBoneWeightBuffer = NULL;
	ID3D12Resource*					m_pd3dBoneWeightUploadBuffer = NULL;

	bool							m_bSkinnedMesh = false;
	ID3D12Device*					m_pd3dDevice = nullptr;

	XMFLOAT3 MeshMin{ FLT_MAX,  FLT_MAX,  FLT_MAX };
	XMFLOAT3 MeshMax{ -FLT_MAX, -FLT_MAX, -FLT_MAX };

public:
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CB_GAMEOBJECT_INFO* pMappedGameObjectCB);


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
	const std::unordered_map<std::string, int>& GetBoneNameToIndex() const { return m_BoneNameToIndex; }

	bool IsSkinnedMesh() const { return m_bSkinnedMesh; }
	
	bool LoadAnimationFromBIN(const char* filename,
		const std::string& clipName,
		AnimationClip& outClip,
		float timeScale = 1.0f);
private:
	int m_nBones;
public:
	const std::vector<BinMaterial>& GetBinMaterials() const { return m_BinMaterials; }

private:
	std::vector<BinMaterial> m_BinMaterials;
	std::unordered_map<std::string, uint32_t> m_BinMaterialNameToIndex; // 있으면 편함(선택)
};

class CBoxMeshDiffused : public CMesh
{
public:
	CBoxMeshDiffused(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, CColliderComponent* Collider);
	virtual ~CBoxMeshDiffused() {}
};

class CCapsuleMeshDiffused : public CMesh
{
public:
	CCapsuleMeshDiffused(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, CColliderComponent* Collider);
	virtual ~CCapsuleMeshDiffused() {}
};