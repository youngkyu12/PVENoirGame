//-----------------------------------------------------------------------------
// File: GameScene.h
//-----------------------------------------------------------------------------

#pragma once

#include "Scene.h"
#include "Shader.h"
#include "LightTypes.h"
#include "SceneRenderTypes.h"
#include "ColliderComponent.h"
#include "Grid.h"
#include "DepthFog.h"
#include "GameSceneHUD.h"
//#include "ShadowMap.h"

#include <unordered_set>
#include <cstdint>

class CMaterial;
class CMesh;
class CFollowTransformComponent;
class CFollowBoneComponent;
class CArrowComponent;
class CBulletComponent;
class CGameObject;
class CCollisionSystem;
class CTexture;
class CNavMesh;

struct CB_GAMEOBJECT_INFO;
struct AttachmentBindSpec
{
    CGameObject* follower = nullptr;
    CGameObject* target = nullptr;
    std::string  boneName;
    XMFLOAT4X4   localOffset{};
};
struct StaticPlacementEntry
{
    std::string assetName;
    std::string objectName;

    XMFLOAT3 pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT4 rot = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

    float yawDeg = 0.0f;
};
struct StaticInstanceVertex
{
	XMFLOAT4 world0 = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
	XMFLOAT4 world1 = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	XMFLOAT4 world2 = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
	XMFLOAT4 world3 = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	UINT objectId = 0;
	UINT pad[3] = { 0, 0, 0 };
};

struct StaticInstanceGroup
{
	std::shared_ptr<CMesh> mesh;
	UINT subMeshIndex = 0;
	std::vector<UINT> objectIndices;

	UINT instanceBufferStart = 0;
	bool useTreeShader = false;
};

struct StaticWorldLodEntry
{
	CGameObject* object = nullptr;
	UINT staticBatchObjectIndex = UINT_MAX;

	std::string assetName;
	XMFLOAT3 lodReferencePosition = XMFLOAT3(0.0f, 0.0f, 0.0f);

	bool lodEnabled = false;
	bool useTreeShader = false;
	int currentLod = 0;

	float lodDistance01 = 40.0f;
	float lodDistance12 = 80.0f;

	std::array<std::shared_ptr<CMesh>, 3> lodMeshes = { nullptr, nullptr, nullptr };

	bool distanceCullEnabled = false;
	bool distanceCulled = false;
	float cullDistance = 1000000.0f;
};

struct StaticOcclusionEntry
{
	CGameObject* object = nullptr;
	UINT staticBatchObjectIndex = UINT_MAX;

	std::string assetName;
	bool enabled = false;

	BoundingOrientedBox worldBounds{};
	bool hasWorldBounds = false;
};

struct SkinnedWorldLodEntry
{
	CGameObject* object = nullptr;
	UINT skinnedBatchObjectIndex = UINT_MAX;

	std::string assetName;
	XMFLOAT3 lodReferencePosition = XMFLOAT3(0.0f, 0.0f, 0.0f);

	bool lodEnabled = false;
	int currentLod = 0;

	float lodDistance01 = 1.0f;
	float lodDistance12 = 2.0f;

	std::array<std::shared_ptr<CMesh>, 3> lodMeshes = { nullptr, nullptr, nullptr };

	bool distanceCullEnabled = false;
	bool distanceCulled = false;
	float cullDistance = 1000000.0f;
};

struct SkinnedOcclusionEntry
{
	CGameObject* object = nullptr;
	UINT skinnedBatchObjectIndex = UINT_MAX;

	std::string assetName;
	bool enabled = false;

	BoundingOrientedBox worldBounds{};
	bool hasWorldBounds = false;
};

struct SkinnedInstanceVertex
{
	XMFLOAT4 world0 = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
	XMFLOAT4 world1 = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	XMFLOAT4 world2 = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
	XMFLOAT4 world3 = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	UINT materialId = 0;
	UINT bonePaletteBase = 0;
	UINT pad[2] = { 0, 0 };
};

struct SkinnedInstanceGroup
{
	std::string geometryKey;
	std::shared_ptr<CMesh> mesh;
	UINT meshIndex = 0;
	UINT subMeshIndex = 0;
	std::vector<UINT> objectIndices;

	UINT instanceBufferStart = 0;
	bool useAlphaClipShader = false;
};

struct CB_SHADOW
{
	XMFLOAT4X4 shadowViewProj{};
	XMFLOAT4X4 shadowTransform{};

	XMFLOAT4 shadowLightPos = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	XMFLOAT4 shadowParams0 = XMFLOAT4(2048.0f, 0.0008f, 0.0040f, 1.0f);
	XMUINT4  shadowParams1 = XMUINT4(UINT_MAX, 0u, 0u, 0u);
};

// ============================================================================
// GameScene
// ============================================================================
class CGameScene final : public CScene
{
public:
    CGameScene();
    ~CGameScene() override;

    // Lifecycle / Release
public:
    void ReleaseObjects() override;
    void ReleaseShaderVariables() override;
    void ReleaseUploadBuffers() override;

    // Build
public:
    void BuildObjects(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;

protected:
    void CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* target) override;

private:
	//void InitShadowMap(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd);
    void BuildLightsAndMaterials();
    void BuildObjectsCollider() override;

    void CreateShaderVariables(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd);

    void BuildStaticBatch(
        ID3D12Device* dev,
        ID3D12GraphicsCommandList* cmd,
        const std::shared_ptr<CStaticObjectsShader>& shader,
        UINT rtCount,
        DXGI_FORMAT* rtvFormats,
        DXGI_FORMAT dsvFormat
    );

    void BuildSkinnedBatch(
        ID3D12Device* dev,
        ID3D12GraphicsCommandList* cmd,
        const std::shared_ptr<CSkinnedObjectsShader>& shader,
        UINT rtCount,
        DXGI_FORMAT* rtvFormats,
        DXGI_FORMAT dsvFormat
    );

	void BuildColliderBatch(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		const std::shared_ptr<CDiffusedShader>& shader,
		UINT rtCount,
		DXGI_FORMAT* rtvFormats,
		DXGI_FORMAT dsvFormat
	);

    void LinkSceneObjects();

	void UpdateShaderVariables(ID3D12GraphicsCommandList* cmd);
	void UpdateFrameRenderState(CCamera* camera);
	void BindFrameRootParameters(ID3D12GraphicsCommandList* cmd);

	void BuildStaticInstanceGroups();
	void ResetStaticWorldLodEntries();

	void ResetStaticOcclusionEntries();
	void BuildStaticOcclusionEntries();
	void BuildStaticOcclusionUnitBoxMesh(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd);
	void BuildStaticOcclusionGpuResources(ID3D12Device* dev);
	void ReleaseStaticOcclusionGpuResources();
	void BeginStaticOcclusionReadback();
	void RenderStaticOcclusionPass(ID3D12GraphicsCommandList* cmd, CCamera* camera);
	void ResolveStaticOcclusionQueries(ID3D12GraphicsCommandList* cmd);
	
	int ComputeStaticWorldLodLevel(const XMFLOAT3& cameraPosition, const StaticWorldLodEntry& entry) const;
	bool ComputeStaticWorldDistanceCulled(const XMFLOAT3& cameraPosition, const StaticWorldLodEntry& entry) const;
	void UpdateStaticWorldLodSelection(CCamera* camera);
	void UpdateStaticOcclusionCullSelection(CCamera* camera);
	void UpdateStaticTreeGridCullSelection(CCamera* camera);
	bool IsStaticTreeObject(const CGameObject* obj) const;
	void RenderStaticInstanceGroups(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	void BuildSkinnedInstanceGroups();
	void ResetSkinnedWorldLodEntries();

	void ResetSkinnedOcclusionEntries();
	void BuildSkinnedOcclusionEntries();
	void BuildSkinnedOcclusionGpuResources(ID3D12Device* dev);
	void ReleaseSkinnedOcclusionGpuResources();
	void BeginSkinnedOcclusionReadback();
	void RenderSkinnedOcclusionPass(ID3D12GraphicsCommandList* cmd, CCamera* camera);
	void ResolveSkinnedOcclusionQueries(ID3D12GraphicsCommandList* cmd);
	void UpdateSkinnedOcclusionCullSelection(CCamera* camera);

	int ComputeSkinnedWorldLodLevel(const XMFLOAT3& cameraPosition, const SkinnedWorldLodEntry& entry) const;
	bool ComputeSkinnedWorldDistanceCulled(const XMFLOAT3& cameraPosition, const SkinnedWorldLodEntry& entry) const;
	void UpdateSkinnedWorldLodSelection(CCamera* camera);
	void RenderSkinnedInstanceGroups(ID3D12GraphicsCommandList* cmd, CCamera* camera);

    // Frame / Render
public:
    bool ProcessInput(UCHAR* pKeysBuffer) override;
    void AnimateObjects(float dt) override;
    void CollisionObjects() override;
	//void RenderShadowMap(ID3D12GraphicsCommandList* cmd, const CGameTimer& gt);

public:
	void OnPrepareRender(ID3D12GraphicsCommandList* cmd, CCamera* camera) override;
	void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera = nullptr) override;
	void RenderShadowPrePass(ID3D12GraphicsCommandList* cmd, CCamera* camera);
	void RenderSceneGeometry(ID3D12GraphicsCommandList* cmd, CCamera* camera);
	void RenderSceneComposite(ID3D12GraphicsCommandList* cmd, CCamera* camera);

    // Input (messages) : 게임에서는 좌클릭 공격
public:
    bool OnProcessingMouseMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    bool OnProcessingKeyboardMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    // Framework 호환: 플레이어 제공
public:
    CGameObject* GetPlayer() const override { return GetPlayerBySlot(m_localPlayerSlot); }
    int GetLocalPlayerSlot() const { return m_localPlayerSlot; }

    // Framework 숫자키 공격(슬롯 0..3)
public:
    void RequestPlayerAttackBySlot(int slot) override;

    // Game-only API
public:
    void SetMaterialDiffuseSrvIndex(int materialId, UINT srvIndex);
	void SetInactiveOverlayVisible(bool visible)
	{
		m_bInactiveOverlayVisible = visible;
		m_hud.SetInactiveOverlayVisible(visible);
	}

	void SetDepthFogSourceSrvIndices(UINT sceneColorSrvIndex, UINT sceneDepthSrvIndex)
	{
		m_depthFog.SetSourceSrvIndices(sceneColorSrvIndex, sceneDepthSrvIndex);
	}

	void SetSceneRenderTargets(
	UINT count,
	const D3D12_CPU_DESCRIPTOR_HANDLE* rtvs,
	D3D12_CPU_DESCRIPTOR_HANDLE dsv)
	{
		m_sceneRenderTargetCount = ( count > static_cast< UINT >( m_sceneRtvHandles.size() ) )
			? static_cast< UINT >( m_sceneRtvHandles.size() )
			: count;

		for ( UINT i = 0; i < m_sceneRenderTargetCount; ++i )
			m_sceneRtvHandles[i] = rtvs[i];

		m_sceneDsvHandle = dsv;
		m_bSceneRenderTargetsReady = ( m_sceneRenderTargetCount > 0 );
	}

	void SetDepthFogPassEnabled(bool enabled) { m_depthFog.SetPassEnabled(enabled); }

	CNavMesh* GetNavMesh() { return m_navMesh.get(); }
	const CNavMesh* GetNavMesh() const { return m_navMesh.get(); }

    CGameObject* GetDemoFighter(int index) const;
    void RequestDemoFighterAttack(int index);

    CGameObject* GetPlayerBySlot(int slot) const; // slot: 0..3
    bool IsLocalPlayer(const CGameObject* obj) const;
	bool RollbackLocalPlayerMoveIfCollidingWorldStatic(const XMFLOAT3& previousPos);
    
	void RequestFireArrow(CGameObject* shooter, float speed, float lifeSec = 3.0f, float yOffset = 0.0f);
	bool IsLocalPlayerInsideMegaGridCenter() const;

#ifndef USING_NETWORK
	void SetMegaGridApproachZoneSize(int megaX, int megaZ, int widthCells, int heightCells);
	void SetMegaGridCleared(int megaX, int megaZ, bool cleared = true);
	void SetMegaGridEventOccurred(int megaX, int megaZ, bool occurred = true);

	bool HasMegaGridPlayerApproached(int megaX, int megaZ) const;
	bool IsMegaGridCleared(int megaX, int megaZ) const;
	bool HasMegaGridEventOccurred(int megaX, int megaZ) const;
#endif

private:
#ifndef USING_NETWORK
	using EGridDynamicKind = CSceneGrid::EDynamicKind;
	using GridDynamicTracker = CSceneGrid::DynamicTracker;

	void InitializeSpatialGrid();
	void ShutdownSpatialGrid();

	bool TryGetTreeCullReferenceGridCell(
		CCamera* camera,
		int& outCellX,
		int& outCellZ,
		int& outMegaX,
		int& outMegaZ) const;

	bool ShouldCullTreesByVillageGrid(CCamera* camera) const;

	void AddDynamicCount(int cellX, int cellZ, EGridDynamicKind kind, int delta);
	void RegisterStaticPlacementToGrid(const StaticPlacementEntry& placement, CGameObject* obj);

	void ResetDynamicGridCounts();
	bool TryGetTrackedCell(const CGameObject* obj, int& outCellX, int& outCellZ) const;
	void RefreshDynamicTracker(GridDynamicTracker& tracker, EGridDynamicKind kind);
	void BuildDynamicGridTrackers();
	void RebuildDynamicGridState();
	void UpdateDynamicGridState();
	void UpdateMegaGridState();
	void DumpStaticGridOccupancyLog() const;
#endif

#ifndef USING_NETWORK
	struct MonsterSpawnEntry
	{
		int index = -1;

		std::string type;

		int megaId = -1;
		int megaX = -1;
		int megaZ = -1;

		XMFLOAT3 pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
		float yawDeg = 0.0f;
	};

	bool LoadMonsterSpawnFile(const std::string& filePath);
	void ApplyMonsterSpawnCounts();
#endif

private:
	void BuildDepthFogResources(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd);
	void RenderDepthFog(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	void BuildShadowResources(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd);
	void UpdateShadowData();

	bool IsWorldOOBBInsideShadowBox(const BoundingOrientedBox& box) const;
	bool IsStaticObjectInsideShadowBox(UINT objectIndex) const;
	bool IsSkinnedObjectInsideShadowBox(UINT objectIndex) const;

	void RenderShadowMap(ID3D12GraphicsCommandList* cmd);
	void RenderStaticInstanceGroupsToShadowMap(ID3D12GraphicsCommandList* cmd);
	void RenderSkinnedInstanceGroupsToShadowMap(ID3D12GraphicsCommandList* cmd);
	void RestoreSceneRenderTargets(ID3D12GraphicsCommandList* cmd, CCamera* camera);

#ifndef USING_NETWORK
	int GetLocalPlayerMegaGridNumberForDepthFog() const;
#endif
	void UpdateDepthFogState(float dt);

    // slot 0..3 플레이어 포인터(소유는 m_skinnedObjects가 함)
    std::array<CGameObject*, 4> m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };

    int m_localPlayerSlot;
    // ------------------------------------------------------------------------
    // Build counts (현재는 BuildObjects()에서 결정, 추후 서버 동기화 값으로 대체)
    // ------------------------------------------------------------------------
    UINT m_grassCount = 1;
    UINT m_groundCount = 1;
    UINT m_villagewallCount = 1;
	UINT m_dirtRoadCount = 1;

    UINT m_building1Count = 1;
    UINT m_building2Count = 1;
    UINT m_building3Count = 1;
    UINT m_building4Count = 1;
    UINT m_building5Count = 1;
    UINT m_building6Count = 1;
    UINT m_building7Count = 1;
    UINT m_building8Count = 1;
    UINT m_building9Count = 1;
	UINT m_towerCount = 1;

    UINT m_ghoulCount = 4;
    UINT m_swordManCount = 3;
    UINT m_bowManCount = 3;
    UINT m_MutantCount = 2;
    UINT m_bossCount = 1;

    UINT m_PlayerCount = 4;

    UINT m_helmetCount = 0;

	UINT m_PlayerSwordCount = 4;
	UINT m_PlayerBowCount = 4;
	UINT m_PlayerAxeCount = 4;
	UINT m_PlayerGunCount = 4;
	UINT m_ColliderCount = 0;

    std::vector<std::unique_ptr<CGameObject>> m_staticObjects;
    std::vector<std::unique_ptr<CGameObject>> m_skinnedObjects;
	std::vector<std::unique_ptr<CGameObject>> m_colliderObjects;

    SCENE_STATIC_BATCH  m_staticBatch;
    SCENE_SKINNED_BATCH m_skinnedBatch;
	SCENE_COLLIDER_BATCH m_colliderBatch;

    std::vector<CGameObject*> m_swordManRefs;
    std::vector<CGameObject*> m_bowManRefs;
    std::vector<CGameObject*> m_MutantRefs;

    std::vector<CGameObject*> m_helmetRefs;

    std::vector<CGameObject*> m_PlayerSwordRefs;
    std::vector<CGameObject*> m_PlayerBowRefs;
    std::vector<CGameObject*> m_PlayerAxeRefs;
    std::vector<CGameObject*> m_PlayerGunRefs;

    std::vector<CGameObject*> m_EnemySwordRefs;
    std::vector<CGameObject*> m_EnemyBowRefs;

    std::vector<AttachmentBindSpec> m_attachmentBinds;

	static constexpr UINT kArrowPoolSize = 32;
	static constexpr UINT kBulletPoolSize = 32;
	std::vector<CGameObject*> m_arrowRefs;
	std::vector<CGameObject*> m_bulletRefs;

	std::array<CGameObject*, 4> m_preparedPlayerArrows = { nullptr, nullptr, nullptr, nullptr };
	std::array<bool, 4> m_prevBowReleasePhase = { false, false, false, false };
	std::vector<CGameObject*> m_preparedBowmanArrows;
	std::vector<bool> m_prevEnemyBowReleasePhase;

	int GetPlayerSlotFromObject(const CGameObject* obj) const;
	int GetBowManIndexFromObject(const CGameObject* obj) const;

	void RequestPrepareArrow(CGameObject* shooter, float pullBackDistance);
	void RequestReleasePreparedArrow(CGameObject* shooter, float speed, float lifeSec = 3.0f);

	void RequestPrepareBowmanArrow(CGameObject* bowman, float pullBackDistance);
	void RequestReleasePreparedBowmanArrow(CGameObject* bowman, float speed, float lifeSec = 3.0f);
	void RequestFireBullet(CGameObject* shooter, float speed, float lifeSec = 3.0f);

	void UpdatePreparedBowArrows();

    std::array<CGameObject*, 3> m_demoFighters = { nullptr, nullptr, nullptr };

    std::vector<std::unique_ptr<CGameObject>> m_lightObjects;
    CFollowTransformComponent* m_pPlayerSpotFollower = nullptr;

    // GPU / Shader Variables (Game 전용)
    ComPtr<ID3D12Resource> m_pd3dcbLights;
    LIGHTS* m_pcbMappedLights = nullptr;

    std::unique_ptr<MATERIALS> m_pMaterials;

    ComPtr<ID3D12Resource> m_pd3dcbMaterials;
    MATERIAL* m_pcbMappedMaterials = nullptr;

	CDepthFogSystem                 m_depthFog;
	float                           m_fElapsedTime = 0.0f;

    unique_ptr<CCollisionSystem> m_Collision;
	std::unique_ptr<CNavMesh> m_navMesh;

#ifndef USING_NETWORK
	std::vector<MonsterSpawnEntry>	m_monsterSpawnEntries;

	CSceneGrid m_sceneGrid;

	std::array<GridDynamicTracker, 4> m_playerGridTrackers = {};
	std::vector<GridDynamicTracker> m_monsterGridTrackers;
	std::vector<GridDynamicTracker> m_arrowGridTrackers;
	std::vector<GridDynamicTracker> m_bulletGridTrackers;
#endif

	/*std::unique_ptr<ShadowMap> mShadowMap;
	std::shared_ptr<CShadowShader> mShadowShader;
	ComPtr<ID3D12DescriptorHeap> m_pd3dShadowDsvDescriptorHeap;*/

private:
    bool LoadStaticPlacementFile(const std::string& filePath);
	bool LoadSceneCubeBoxColliderReport(const std::string& filePath);
	bool ExportStaticWorldLocalOOBBReport(
	const std::string& filePath,
	const std::vector<size_t>& placementIndices,
	const std::vector<CGameObject*>& objects
	) const;
	void ResetStaticPlacementCounts();
    void ApplyStaticPlacementCounts();
    static float QuaternionToYawDegrees(const XMFLOAT4& q);

private:
    std::vector<StaticPlacementEntry>   m_staticPlacementEntries;
	std::unordered_map<std::string, std::unordered_map<std::string, std::vector<AuthoredSubMeshOOBB>>> mSceneCubeBoxColliderTable;

	std::vector<StaticInstanceGroup>    m_staticInstanceGroups;
	std::vector<StaticWorldLodEntry>    m_staticWorldLodEntries;
	std::vector<StaticOcclusionEntry>   m_staticOcclusionEntries;

	std::vector<uint8_t>                m_staticDistanceCullFlags;
	std::vector<uint8_t>                m_staticOcclusionCullFlags;
	std::vector<uint8_t>                m_staticTreeGridCullFlags;

	std::vector<uint8_t>                m_staticShadowCasterFlags;
	std::vector<UINT>                   m_staticTreeObjectIndices;
	std::vector<int>                    m_staticShadowOcclusionEntryIndices;

	std::vector<UINT64>                 m_staticOcclusionQuerySampleCounts;
	std::vector<uint8_t>                m_staticOcclusionLastFrameIssuedFlags;
	std::vector<uint8_t>                m_staticOcclusionCurrentFrameIssuedFlags;
	std::vector<uint8_t>                m_staticOcclusionZeroSampleFrameCounts;
	ComPtr<ID3D12QueryHeap>             m_pd3dStaticOcclusionQueryHeap;
	ComPtr<ID3D12Resource>              m_pd3dStaticOcclusionReadbackBuffer;
	UINT64* m_pMappedStaticOcclusionReadbackBuffer = nullptr;
	UINT                                m_staticOcclusionQueryCapacity = 0;
	bool                                m_bStaticOcclusionQueryResourcesReady = false;
	bool                                m_bStaticOcclusionQueryResultsValid = false;
	bool                                m_staticWorldLodDirty = false;
	bool                                m_bStaticOcclusionCullingEnabled = true;
	bool                                m_bStaticTreeGridCullingEnabled = true;
	UINT                                m_staticOcclusionHideFrameThreshold = 8;
	float                               m_staticOcclusionMinTestDistance = 50.0f;
	float                               m_staticOcclusionMaxCullExtentDistanceRatio = 0.20f;
	float                               m_staticLodDistance01 = 40.0f;
	float                               m_staticLodDistance12 = 80.0f;
	float                               m_staticLodHysteresis = 15.0f;
	float                               m_staticCullHysteresis = 20.0f;

	std::shared_ptr<CMesh>              m_staticOcclusionUnitBoxMesh;
	ComPtr<ID3D12Resource>              m_pd3dStaticOcclusionInstanceBuffer;
	StaticInstanceVertex* m_pMappedStaticOcclusionInstanceBuffer = nullptr;

	ComPtr<ID3D12Resource>              m_pd3dStaticInstanceBuffer;
	StaticInstanceVertex* m_pMappedStaticInstanceBuffer = nullptr;
	UINT                                m_staticInstanceBufferCapacity = 0;

	std::shared_ptr<CStaticObjectsShader>	m_treeStaticShader;
	std::unordered_set<const CGameObject*>	m_treeAlphaClipObjects;
	std::unordered_set<const CGameObject*>	m_skinnedAlphaClipObjects;

	std::shared_ptr<COcclusionStaticShader>               m_occlusionStaticShader;
	std::shared_ptr<CShadowMapStaticShader>               m_shadowStaticShader;
	std::shared_ptr<CShadowMapAlphaClipStaticShader>      m_shadowAlphaClipStaticShader;
	std::shared_ptr<CShadowMapSkinnedShader>              m_shadowSkinnedShader;
	std::shared_ptr<CShadowMapAlphaClipSkinnedShader>     m_shadowAlphaClipSkinnedShader;

	CGameSceneHUD                       m_hud;
	ComPtr<ID3D12DescriptorHeap>        m_pd3dShadowDsvHeap;
	ComPtr<ID3D12Resource>              m_pd3dShadowMap;
	ComPtr<ID3D12Resource>              m_pd3dcbShadow;
	CB_SHADOW* m_pcbMappedShadow = nullptr;
	CB_SHADOW                           m_shadowData{};

	XMFLOAT4X4                          m_shadowView{};

	UINT                                m_shadowMapSize = 2048;
	UINT                                m_shadowMapSrvIndex = UINT_MAX;
	float                               m_shadowOrthoHalfSize = 45.0f;
	float                               m_shadowNearZ = 1.0f;
	float                               m_shadowFarZ = 160.0f;

	D3D12_VIEWPORT                      m_shadowViewport = { 0.0f, 0.0f, 2048.0f, 2048.0f, 0.0f, 1.0f };
	D3D12_RECT                          m_shadowScissorRect = { 0, 0, 2048, 2048 };
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 8> m_sceneRtvHandles = {};
	D3D12_CPU_DESCRIPTOR_HANDLE                m_sceneDsvHandle = {};
	UINT                                       m_sceneRenderTargetCount = 0;
	bool                                       m_bSceneRenderTargetsReady = false;

	bool                                m_bInactiveOverlayVisible = false;
	bool                                m_bStartedGameplayMusic = false;
	bool                                m_bWasLocalPlayerInsideMegaGridCenter = false;
	bool                                m_bShowShadowMapOverlay = true;

	bool GetPauseOverlayRect(XMFLOAT4& outRect) const;

	std::vector<SkinnedInstanceGroup>   m_skinnedInstanceGroups;

	ComPtr<ID3D12Resource>              m_pd3dSkinnedInstanceBuffer;
	std::vector<SkinnedWorldLodEntry>   m_skinnedWorldLodEntries;
	std::vector<uint8_t>                m_skinnedDistanceCullFlags;

	std::vector<SkinnedOcclusionEntry>  m_skinnedOcclusionEntries;
	std::vector<uint8_t>                m_skinnedOcclusionCullFlags;
	std::vector<int>                    m_skinnedShadowOcclusionEntryIndices;
	std::vector<UINT64>                 m_skinnedOcclusionQuerySampleCounts;
	std::vector<uint8_t>                m_skinnedOcclusionLastFrameIssuedFlags;
	std::vector<uint8_t>                m_skinnedOcclusionCurrentFrameIssuedFlags;
	std::vector<uint8_t>                m_skinnedOcclusionZeroSampleFrameCounts;
	ComPtr<ID3D12QueryHeap>             m_pd3dSkinnedOcclusionQueryHeap;
	ComPtr<ID3D12Resource>              m_pd3dSkinnedOcclusionReadbackBuffer;
	UINT64* m_pMappedSkinnedOcclusionReadbackBuffer = nullptr;
	ComPtr<ID3D12Resource>              m_pd3dSkinnedOcclusionInstanceBuffer;
	StaticInstanceVertex* m_pMappedSkinnedOcclusionInstanceBuffer = nullptr;
	UINT                                m_skinnedOcclusionQueryCapacity = 0;
	bool                                m_bSkinnedOcclusionQueryResourcesReady = false;
	bool                                m_bSkinnedOcclusionQueryResultsValid = false;
	bool                                m_bSkinnedOcclusionCullingEnabled = true;
	UINT                                m_skinnedOcclusionHideFrameThreshold = 8;
	float                               m_skinnedOcclusionMinTestDistance = 35.0f;
	float                               m_skinnedOcclusionMaxCullExtentDistanceRatio = 0.30f;

	bool                                m_skinnedWorldLodDirty = false;
	float                               m_skinnedLodHysteresis = 5.0f;
	float                               m_skinnedCullHysteresis = 10.0f;
	SkinnedInstanceVertex* m_pMappedSkinnedInstanceBuffer = nullptr;
	UINT                                m_skinnedInstanceBufferCapacity = 0;

	ComPtr<ID3D12Resource>              m_pd3dSkinnedBonePaletteBuffer;
	XMFLOAT4X4* m_pMappedSkinnedBonePaletteBuffer = nullptr;
	UINT                                m_skinnedBonePaletteStride = 0;
	UINT                                m_skinnedBonePaletteCapacity = 0;
	
	void BuildStaticWorldSubmeshOOBBDebugObjects(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd
	);
public:
    bool IsPointInPauseOverlay(POINT clientPt) const;

};