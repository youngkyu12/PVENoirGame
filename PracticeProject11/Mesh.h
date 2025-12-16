#pragma once
#include "AnimatorData.h"
//정점을 표현하기 위한 클래스를 선언한다.
class CVertex {
public:
	CVertex() { m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f); }
	CVertex(XMFLOAT3 xmf3Position) { m_xmf3Position = xmf3Position; }
	~CVertex() {}

protected:
	//정점의 위치 벡터이다(모든 정점은 최소한 위치 벡터를 가져야 한다).
	XMFLOAT3 m_xmf3Position{};
};

class CDiffusedVertex : public CVertex {
public:
	CDiffusedVertex() {
		m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
		m_xmf4Diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	CDiffusedVertex(float x, float y, float z, XMFLOAT4 xmf4Diffuse) {
		m_xmf3Position = XMFLOAT3(x, y, z); m_xmf4Diffuse = xmf4Diffuse;
	}

	CDiffusedVertex(XMFLOAT3 xmf3Position, XMFLOAT4 xmf4Diffuse) {
		m_xmf3Position = xmf3Position; m_xmf4Diffuse = xmf4Diffuse;
	}

	~CDiffusedVertex() {}

protected:
	//정점의 색상이다.
	XMFLOAT4 m_xmf4Diffuse;

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

struct SubMesh
{
	vector<XMFLOAT3> positions;
	vector<XMFLOAT3> normals;
	vector<XMFLOAT2> uvs;
	vector<XMUINT4>  boneIndices;
	vector<XMFLOAT4> boneWeights;
	vector<UINT>     indices;

	UINT textureIndex = UINT_MAX;       // 이 SubMesh가 사용할 텍스처의 SRV 인덱스

	std::string meshName;
	std::string materialName;

	// GPU 리소스
	ID3D12Resource* vb = nullptr;
	ID3D12Resource* vbUpload = nullptr;
	ID3D12Resource* ib = nullptr;
	ID3D12Resource* ibUpload = nullptr;

	ID3D12Resource* texture = nullptr;
	ID3D12Resource* textureUpload = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vbView{};
	D3D12_INDEX_BUFFER_VIEW  ibView{};
};

class CAnimator;
class CMesh {
public:
	CMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual ~CMesh() {}

	void ReleaseObjects();
	void ReleaseUploadBuffers();
// BIN Export
	void LoadMeshFromBIN(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const char* filename);
	void EnableSkinning(int nBones);

	void LoadTextureFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
		ID3D12DescriptorHeap* srvHeap, UINT descriptorIndex, const wchar_t* fileName, int subMeshIndex);

	// CAnimator 연동을 위한 헬퍼 함수들
	// 본 개수 반환
	int GetBoneCount() const;

	// 본 이름으로 인덱스를 찾는 함수
	// 존재하지 않으면 -1 반환
	int GetBoneIndexByName(const std::string& boneName) const;

	// 본의 부모 인덱스 반환
	int GetBoneParentIndex(int boneIndex) const;

	// 전체 본 배열 반환 (읽기 전용)
	const std::vector<Bone>& GetBones() const { return m_Bones; }

	// 애니메이터 포인터 반환
	CAnimator* GetAnimator() const { return m_pAnimator; }

	// 애니메이터가 없으면 자동 생성하여 반환
	CAnimator* EnsureAnimator();

	// --- Bone CBV 관련 헬퍼 ---

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

// Render
public:
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList);

protected:
	ComPtr<ID3D12Resource> m_pd3dVertexBuffer;
	ComPtr<ID3D12Resource> m_pd3dVertexUploadBuffer;

	D3D12_VERTEX_BUFFER_VIEW m_d3dVertexBufferView;
	D3D12_PRIMITIVE_TOPOLOGY m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT m_nSlot = 0;
	UINT m_nVertices = 0;
	UINT m_nStride = 0;
	UINT m_nOffset = 0;

	ComPtr<ID3D12Resource> m_pd3dIndexBuffer;
	ComPtr<ID3D12Resource> m_pd3dIndexUploadBuffer;

	D3D12_INDEX_BUFFER_VIEW m_d3dIndexBufferView;

	UINT m_nIndices = 0;
	UINT m_nStartIndex = 0;
	int m_nBaseVertex = 0;

	int							    m_nPolygons = 0;
	CPolygon** m_ppPolygons = NULL;

	vector<Bone>				m_Bones;            // 본 정보 배열
	unordered_map<string, int>	m_BoneNameToIndex;  // 본 이름→인덱스 매핑

	XMUINT4*					m_pxu4BoneIndices = NULL;   // 정점별 본 인덱스 (최대 4)

	XMFLOAT4*					m_pxmf4BoneWeights = NULL;  // 정점별 본 가중치 (최대 4)
	XMFLOAT4X4*					m_pxmf4x4BoneTransforms = NULL;  // 최종 본 행렬
	ID3D12Resource*				m_pd3dcbBoneTransforms = NULL; // GPU용 상수 버퍼

	vector<SubMesh>				m_SubMeshes;

	CAnimator*					m_pAnimator = nullptr;   // 애니메이션 관리자
	bool						m_bSkinnedMesh = false;        // 스키닝 메시 여부
	ID3D12DescriptorHeap*		m_pd3dSrvDescriptorHeap = nullptr;
	UINT						m_nSrvDescriptorIncrementSize = 0;
	UINT						m_nTextureRootParameterIndex = 5;  // t0이 RootParam5라고 가정
	ID3D12Device*				m_pd3dDevice = nullptr;
};

class CTriangleMesh : public CMesh {
public:
	CTriangleMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual ~CTriangleMesh() { }
};

class CCubeMeshDiffused : public CMesh {
public:
	CCubeMeshDiffused(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, float fWidth = 2.0f, float fHeight = 2.0f, float fDepth = 2.0f);
	virtual ~CCubeMeshDiffused() { }
};

class CAirplaneMeshDiffused : public CMesh {
public:
	CAirplaneMeshDiffused(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList,
		float fWidth = 20.0f, float fHeight = 20.0f, float fDepth = 4.0f, XMFLOAT4 xmf4Color = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f));
	virtual ~CAirplaneMeshDiffused() {}
};