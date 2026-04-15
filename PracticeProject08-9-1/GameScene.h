//-----------------------------------------------------------------------------
// File: GameScene.h
//-----------------------------------------------------------------------------

#pragma once

#include "Scene.h"
#include "Shader.h"
#include "LightTypes.h"
#include "SceneRenderTypes.h"
#include "ColliderComponent.h"

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
};

struct SkinnedWorldLodEntry
{
	CGameObject* object = nullptr;
	UINT skinnedBatchObjectIndex = UINT_MAX;

	std::string assetName;
	XMFLOAT3 lodReferencePosition = XMFLOAT3(0.0f, 0.0f, 0.0f);

	bool lodEnabled = false;
	int currentLod = 0;

	float lodDistance01 = 80.0f;
	float lodDistance12 = 180.0f;

	std::array<std::shared_ptr<CMesh>, 3> lodMeshes = { nullptr, nullptr, nullptr };
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
    static XMFLOAT4X4 BuildAttachmentOffsetMatrix(
        const XMFLOAT3& pos,
        const XMFLOAT3& rotDeg,
        const XMFLOAT3& scale = XMFLOAT3(1.0f, 1.0f, 1.0f)
    );

    void UpdateShaderVariables(ID3D12GraphicsCommandList* cmd);
	void BuildStaticInstanceGroups();
	void ResetStaticWorldLodEntries();
	int ComputeStaticWorldLodLevel(const XMFLOAT3& cameraPosition, const StaticWorldLodEntry& entry) const;
	void UpdateStaticWorldLodSelection(CCamera* camera);
	void RenderStaticInstanceGroups(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	void BuildSkinnedInstanceGroups();
	void ResetSkinnedWorldLodEntries();
	int ComputeSkinnedWorldLodLevel(const XMFLOAT3& cameraPosition, const SkinnedWorldLodEntry& entry) const;
	void UpdateSkinnedWorldLodSelection(CCamera* camera);
	void RenderSkinnedInstanceGroups(ID3D12GraphicsCommandList* cmd, CCamera* camera);

    // Frame / Render
public:
    bool ProcessInput(UCHAR* pKeysBuffer) override;
    void AnimateObjects(float dt) override;
    void CollisionObjects() override;

    void OnPrepareRender(ID3D12GraphicsCommandList* cmd, CCamera* camera) override;
    void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera = nullptr) override;

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
    void SetInactiveOverlayVisible(bool visible) { m_bInactiveOverlayVisible = visible; }

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
	static constexpr int kGridMinX = -600;
	static constexpr int kGridMaxX = 600;
	static constexpr int kGridMinZ = -200;
	static constexpr int kGridMaxZ = 1000;

	static constexpr int kGridWidth = ( kGridMaxX - kGridMinX );
	static constexpr int kGridHeight = ( kGridMaxZ - kGridMinZ );
	static constexpr int kGridCellCount = ( kGridWidth * kGridHeight );

	static constexpr int kMegaGridCols = 3;
	static constexpr int kMegaGridRows = 3;
	static constexpr int kMegaGridCount = ( kMegaGridCols * kMegaGridRows );

	static constexpr int kMegaGridCellWidth = ( kGridWidth / kMegaGridCols );   // 400
	static constexpr int kMegaGridCellHeight = ( kGridHeight / kMegaGridRows ); // 400

	static constexpr int kDefaultMegaGridApproachWidth = 200;
	static constexpr int kDefaultMegaGridApproachHeight = 200;

	struct MegaGridCell
	{
		bool hasPlayerApproached = false;
		bool isCleared = false;
		bool hasEventOccurred = false;

		int approachWidthCells = kDefaultMegaGridApproachWidth;
		int approachHeightCells = kDefaultMegaGridApproachHeight;
	};

	struct GridStaticCell
	{
		uint16_t buildingCount = 0;
		float floorHeight = 0.0f; // 지금은 고정 0
	};

	struct GridDynamicCell
	{
		uint16_t playerCount = 0;
		uint16_t monsterCount = 0;
		uint16_t arrowCount = 0;
		uint16_t bulletCount = 0;
	};

	enum class EGridDynamicKind : uint8_t
	{
		Player,
		Monster,
		Arrow,
		Bullet
	};

	struct GridDynamicTracker
	{
		CGameObject* object = nullptr;
		int prevCellX = -1;
		int prevCellZ = -1;
		bool occupied = false;
	};

	void InitializeSpatialGrid();
	void ShutdownSpatialGrid();
	void InitializeMegaGridState();

	bool WorldToGridCell(float worldX, float worldZ, int& outCellX, int& outCellZ) const;
	int GridCellIndex(int cellX, int cellZ) const;

	int MegaGridIndex(int megaX, int megaZ) const;
	bool FineCellToMegaGridCell(int cellX, int cellZ, int& outMegaX, int& outMegaZ) const;
	bool IsFineCellInsideMegaGridApproachZone(int megaX, int megaZ, int cellX, int cellZ) const;

	void AddDynamicCount(int cellX, int cellZ, EGridDynamicKind kind, int delta);
	void StampBuildingCellsFromOOBB(const BoundingOrientedBox& box, std::unordered_set<int>& touchedCells);
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
	enum class EUIRenderLayer : uint8_t
	{
		Frame = 0,
		Content = 1,
		Pause = 2
	};

	struct UISpriteEntry
	{
		std::string name;
		std::shared_ptr<CTexture> texture;
		UINT srvIndex = UINT_MAX;

		// x=centerX, y=centerY, z=width, w=height (pixel)
		XMFLOAT4 rect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		EUIRenderLayer layer = EUIRenderLayer::Frame;
		bool visible = true;
	};

	void BuildUIResources(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd);
	int AddUISprite(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		const char* name,
		const wchar_t* texturePath,
		const XMFLOAT4& rect,
		EUIRenderLayer layer,
		bool visible = true
	);
	void RenderUI(ID3D12GraphicsCommandList* cmd, CCamera* camera);

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

    unique_ptr<CCollisionSystem> m_Collision;
	std::unique_ptr<CNavMesh> m_navMesh;

#ifndef USING_NETWORK
	std::vector<MonsterSpawnEntry> m_monsterSpawnEntries;

	bool m_spatialGridInitialized = false;
	std::vector<GridStaticCell> m_gridStaticCells;
	std::vector<GridDynamicCell> m_gridDynamicCells;
	std::array<MegaGridCell, kMegaGridCount> m_megaGridCells = {};

	std::array<GridDynamicTracker, 4> m_playerGridTrackers = {};
	std::vector<GridDynamicTracker> m_monsterGridTrackers;
	std::vector<GridDynamicTracker> m_arrowGridTrackers;
	std::vector<GridDynamicTracker> m_bulletGridTrackers;
#endif

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
	bool                                m_staticWorldLodDirty = false;
	float                               m_staticLodDistance01 = 40.0f;
	float                               m_staticLodDistance12 = 80.0f;
	float                               m_staticLodHysteresis = 15.0f;

	ComPtr<ID3D12Resource>              m_pd3dStaticInstanceBuffer;
	StaticInstanceVertex* m_pMappedStaticInstanceBuffer = nullptr;
	UINT                                m_staticInstanceBufferCapacity = 0;

	std::shared_ptr<CStaticObjectsShader>	m_treeStaticShader;
	std::unordered_set<const CGameObject*>	m_treeAlphaClipObjects;

	std::shared_ptr<CRectUIShader>      m_uiRectShader;
	std::vector<UISpriteEntry>          m_uiSprites;
	int                                 m_pauseUISpriteIndex = -1;

	bool                                m_bInactiveOverlayVisible = false;
	bool                                m_bStartedGameplayMusic = false;
	bool                                m_bWasLocalPlayerInsideMegaGridCenter = false;

	bool GetPauseOverlayRect(XMFLOAT4& outRect) const;

	std::vector<SkinnedInstanceGroup>   m_skinnedInstanceGroups;

	ComPtr<ID3D12Resource>              m_pd3dSkinnedInstanceBuffer;
	std::vector<SkinnedWorldLodEntry>   m_skinnedWorldLodEntries;
	bool                                m_skinnedWorldLodDirty = false;
	float                               m_skinnedLodHysteresis = 5.0f;
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