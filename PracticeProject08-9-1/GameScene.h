//-----------------------------------------------------------------------------
// File: GameScene.h
//-----------------------------------------------------------------------------

#pragma once

#include "Scene.h"
#include "Shader.h"
#include "LightTypes.h"
#include "SceneRenderTypes.h"
#include "GameSceneBillboardTypes.h"
#include "ColliderComponent.h"
#include "Grid.h"
#include "DepthFog.h"
#include "GameSceneHUD.h"
#include "ShadowMap.h"
#include "EnemySpawner.h"
#include "Ssao.h"

#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <deque>

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
class CStaticMeshRendererComponent;
class CSkinnedMeshRendererComponent;
class CSkinningComponent;
class CAnimatorComponent;
class CHealthComponent;
class CActorTagComponent;
class TerrainData;
class CInventoryComponent;
namespace FMOD
{
	class Channel;
}

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
	bool useTerrainShader = false;
	bool useWaterShader = false;

	int lodLevel = 0;

	std::vector<UINT> visibleSceneObjectIndices;
	std::vector<UINT> visibleShadowObjectIndices;
};

struct StaticRenderObjectCache
{
	CGameObject* object = nullptr;
	CStaticMeshRendererComponent* renderer = nullptr;

	bool dynamicWorldMatrix = false;

	XMFLOAT4 world0 = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
	XMFLOAT4 world1 = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	XMFLOAT4 world2 = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
	XMFLOAT4 world3 = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
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

enum class EStaticOcclusionEntryKind : uint8_t
{
	Object = 0,
	TreeDoorProbe
};

struct StaticOcclusionEntry
{
	EStaticOcclusionEntryKind kind = EStaticOcclusionEntryKind::Object;

	CGameObject* object = nullptr;
	UINT staticBatchObjectIndex = UINT_MAX;

	std::string assetName;
	bool enabled = false;

	BoundingOrientedBox worldBounds{};
	bool hasWorldBounds = false;

	int treeProbeMegaGridNumber = -1;
	int treeProbeDoorIndex = -1;
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

	bool distanceCullEnabled = true;
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

struct SkinnedComponentCache
{
	CGameObject* object = nullptr;

	CSkinnedMeshRendererComponent* renderer = nullptr;
	CSkinningComponent* skinning = nullptr;
	CAnimatorComponent* animator = nullptr;
	CHealthComponent* health = nullptr;
	CActorTagComponent* actorTag = nullptr;
	CColliderComponent* collider = nullptr;

	bool isNpc = false;
	bool isPlayer = false;
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

	void CreateTerrainData(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd);
	void CreateWaterTextures(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd);

	void OnResize(int Width, int Height) override;

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
	void ApplyAttachmentCullFromSkinnedOwners();

	void UpdateShaderVariables(ID3D12GraphicsCommandList* cmd);
	void UpdateBossHpGaugeHud();
	bool ShouldRenderBossHpGaugeHud(CGameObject* boss) const;
	bool IsBossStageBossAppearFinishedForHud(CGameObject* boss) const;
	void UpdateFrameRenderState(CCamera* camera);
	void BindFrameRootParameters(ID3D12GraphicsCommandList* cmd);
	void BuildSkyBox(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd);
	void RenderSkyBox(ID3D12GraphicsCommandList* cmd, CCamera* camera);
	void ReleaseSkyBoxResources();
	void ReleaseSkyBoxUploadBuffers();

	void BuildStaticInstanceGroups();

	void BuildStaticRenderObjectCache();
	void BuildStaticGameplayTickList();
	bool WriteStaticInstanceVertexFromCache(StaticInstanceVertex& dst, UINT objectIndex) const;
	void BuildStaticVisibleListsForFrame(CCamera* camera);
	void BuildStaticShadowVisibleListsForFrame();

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

	void BuildItemBillboardBatch(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		UINT rtCount,
		DXGI_FORMAT* rtvFormats,
		DXGI_FORMAT dsvFormat
	);

	void AddPotionItemBillboardEntries();
	XMFLOAT3 AdjustItemBillboardPositionToTerrain(const XMFLOAT3& position) const;

	float GetTerrainGroundYOrFallback(float worldX, float worldZ, float fallbackY) const;
	XMFLOAT3 AlignPositionYToTerrainGround(const XMFLOAT3& position, float yOffset = 0.0f) const;

	void ReleaseItemBillboardGpuResources();
	void ReleaseMonsterHpGaugeGpuResources();
	void ReleaseAllGameSceneEffectGpuResources();

	void UpdateItemBillboardDistanceCullSelection(CCamera* camera);
	void RenderItemBillboards(ID3D12GraphicsCommandList* cmd, CCamera* camera);
	void RenderTransparentItemBillboards(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	void BuildMonsterHpGaugeBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, UINT rtCount, DXGI_FORMAT* rtvFormats, DXGI_FORMAT dsvFormat);
	void UpdateMonsterHpGaugeTimers(float dt);
	void ResetMonsterHpGaugeVisibilityState();
	void RenderMonsterHpGauges(ID3D12GraphicsCommandList* cmd, CCamera* camera);
	bool IsSkinnedMonsterHpGaugeRenderAllowed(const SkinnedWorldLodEntry& entry) const;
	bool GetMonsterHpGaugeDesc(const SkinnedWorldLodEntry& entry, float& outYOffset, float& outMaxWidth, float& outHeight) const;
	bool FindSkinnedBatchObjectIndex(const CGameObject* object, UINT& outObjectIndex) const;
	bool IsOtherPlayerSkinnedBodyRenderedThisFrame(int playerSlot, CCamera* camera, UINT& outSkinnedBatchObjectIndex) const;
	void UpdateOtherPlayerWorldHpGaugeVisibilityForHud(CCamera* camera);
	bool IsOtherPlayerWorldHpGaugeRenderAllowed(int playerSlot, CCamera* camera, UINT& outSkinnedBatchObjectIndex) const;
	UINT GetPlayerWorldHpNameMaterialId(int playerSlot) const;

	void BuildMuzzleFlashBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, DXGI_FORMAT dsvFormat);

	void ReleaseMuzzleFlashGpuResources();

	void SpawnMuzzleFlash(const XMFLOAT3& position, const XMFLOAT3& direction);
	void BuildGunSmokeBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, DXGI_FORMAT dsvFormat);
	void ReleaseGunSmokeGpuResources();
	void SpawnGunSmoke(const XMFLOAT3& position, const XMFLOAT3& direction);
	void UpdateGunSmokes(float dt);
	void RenderGunSmokes(ID3D12GraphicsCommandList* cmd, CCamera* camera);
	void SpawnBloodSplash(CGameObject* victim, const XMFLOAT3* hitPosition = nullptr, const XMFLOAT3* hitDirection = nullptr);
	void SpawnBossMeleeSlashEffect(CGameObject* boss);

	void SpawnWeaponLevelUpFireworks();
	void SpawnGoldFireworkBurstAtWeapon(CGameObject* weaponObject);

	void SpawnInventoryUseBurst(CGameObject* player, int inventorySlot);
	void UpdateInventoryBuffAmbientParticles(float dt);
	void EmitInventoryBuffAmbientParticles(CGameObject* player, int inventorySlot, float dt, float& accumulatorSec);

	void SpawnMagicCircleGlowParticle(
		const XMFLOAT3& center,
		float circleSize,
		float alpha,
		float intensityScale,
		float glowSizeScale = 1.0f,
		float afterimageSizeScale = 1.0f,
		float lifetimeScale = 1.0f
	);

	void EmitMagicCircleGlowParticles(
		const XMFLOAT3& center,
		float circleSize,
		float alpha,
		float dt,
		float& accumulator,
		float emitIntervalSec,
		int particlesPerEmit,
		float intensityScale
	);

	void EmitBossCallSummonCircleGlowParticles(
		float dt,
		float alpha
	);

	void UpdateMuzzleFlashes(float dt);
	void RenderMuzzleFlashes(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	void BuildSwordTrailBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, DXGI_FORMAT dsvFormat);

	void ReleaseSwordTrailGpuResources();

	void BeginSwordTrail(CGameObject* owner);
	void BeginAxeTrail(CGameObject* owner);
	void UpdateSwordTrails(float dt);
	void RenderSwordTrails(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	void BuildMonsterSwordTrailBatch(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		DXGI_FORMAT dsvFormat
	);

	void ReleaseMonsterSwordTrailGpuResources();

	void BeginSwordManSwordTrail(CGameObject* swordman);
	void UpdateMonsterSwordTrails(float dt);
	void RenderMonsterSwordTrails(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	void BuildArrowTrailBatch(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		DXGI_FORMAT dsvFormat
	);

	void BuildMonsterArrowTrailBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, DXGI_FORMAT dsvFormat);

	void ReleaseArrowTrailGpuResources();
	void ReleaseMonsterArrowTrailGpuResources();

	void UpdateArrowTrails(float dt);
	void RenderArrowTrails(ID3D12GraphicsCommandList* cmd, CCamera* camera);
	void RenderMonsterArrowTrails(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	void BuildBossCallSummonWwwBatch(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		DXGI_FORMAT dsvFormat
	);

	void ReleaseBossCallSummonWwwGpuResources();

	void SpawnBossCallSummonWwwEffect(const XMFLOAT3& center, EEnemySpawnerEnemyKind kind);

	void UpdateBossCallSummonWwwEffects(float dt);
	void RenderBossCallSummonWwwEffects(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	void UpdateItemBillboardPickupCollision();
	bool DoesPlayerOverlapItemBillboard(const CGameObject* player, const ItemBillboardEntry& item) const;

	void SetBossSummonCircleAlpha(float alpha);
	void SetBossSummonGlowAlpha(float alpha);
	void SetBossSummonVisualAlpha(float alpha);
	void SetBossSummonVisualActive(bool active);

	void SetBossCallSummonCircleAlpha(float alpha);
	void StartBossCallSummonCircleFadeOut();
	void UpdateBossCallSummonCircles(float dt);
	void ClearBossCallSummonCircleVisuals();

	float GetBossCallSummonCircleSize(EEnemySpawnerEnemyKind kind) const;
	void AddBossCallSummonCircle(const XMFLOAT3& center, EEnemySpawnerEnemyKind kind);

	void SpawnBossSummonCircle(const XMFLOAT3& center, float alpha);
	void SpawnBossSummonGlow(const XMFLOAT3& center, float alpha);
	void SpawnBossSummonVisuals(const XMFLOAT3& center, float alpha);

	void SetBossDeathCircleAlpha(float alpha);
	void SetBossDeathRingAlpha(float alpha);
	void BeginBossDeathEffect(CGameObject* boss);
	void UpdateBossDeathEffect(float dt);
	void ClearBossDeathVisualBillboards();
	void SetBossDeathRendererVisible(CGameObject* boss, bool visible);
	void SpawnBossDeathAbsorbParticles(const XMFLOAT3& center, float alpha, int count);
	void SpawnBossDeathBlackSmoke(const XMFLOAT3& center, int count, float sizeScale, float upwardBias);
	void SpawnBossDeathGreenBurst(const XMFLOAT3& center);

	void SetBossShockwaveAlpha(float alpha);
	void SpawnBossShockwave(const XMFLOAT3& center);
	void UpdateBossShockwave(float dt);
	void SetBossShockwaveWallAlpha(float alpha);

	void ApplyBossShockwavePushToLocalPlayer(
		float previousRadius,
		float currentRadius
	);

	void ResetBossPoisonProjectileState();

	void BuildBossPoisonProjectileBatch(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		DXGI_FORMAT dsvFormat
	);

	void ReleaseBossPoisonProjectileGpuResources();

	void UpdateBossPoisonProjectileSpellCasts(float dt);
	void UpdateBossMeleeSlashCasts(float dt);
	void SpawnBossPoisonProjectile(CGameObject* boss);
	void UpdateBossPoisonProjectiles(float dt);
	void RenderBossPoisonProjectiles(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	void SpawnBossPoisonProjectileDust(BossPoisonProjectileEntry& entry);

	void ApplyBossPoisonProjectilePlayerHits(BossPoisonProjectileEntry& entry);
	bool DoesBossPoisonProjectileOverlapPlayer(
		const BossPoisonProjectileEntry& entry,
		const CGameObject* player
	) const;

	void ApplyBossPoisonProjectileHitToPlayer(
		BossPoisonProjectileEntry& entry,
		int playerSlot,
		CGameObject* player
	);

	bool IsBossPoisonProjectilePlayerRollInvincible(
		const CGameObject* player
	) const;

	BossPoisonProjectileEntry* AcquireFreeBossPoisonProjectileEntry();

	CGameObject* FindBossStageBossInMegaGrid(int megaGridNumber) const;
	
	std::shared_ptr<CMesh> CreateItemBillboardQuadMesh(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd
	);

	void BuildSkinnedComponentCache();
	const SkinnedComponentCache* GetSkinnedComponentCache(UINT objectIndex) const;

	bool WriteSkinnedInstanceVertexFromCache(
		SkinnedInstanceVertex& dst,
		const SkinnedComponentCache& cache,
		UINT objectIndex,
		UINT meshIndex,
		UINT subMeshIndex,
		XMFLOAT4X4* mappedSkinnedBonePaletteBuffer
	) const;

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

	int ComputeSkinnedWorldLodLevel(
	const XMFLOAT3& cameraPosition,
	const XMFLOAT3& objectPosition,
	const SkinnedWorldLodEntry& entry
	) const;

	bool ComputeSkinnedWorldDistanceCulled(
		const XMFLOAT3& cameraPosition,
		const XMFLOAT3& objectPosition,
		const SkinnedWorldLodEntry& entry
	) const; 
	
	void UpdateSkinnedWorldLodSelection(CCamera* camera);
	void RenderSkinnedInstanceGroups(ID3D12GraphicsCommandList* cmd, CCamera* camera);
	bool ShouldEvaluateSkinnedPoseThisFrame(UINT objectIndex, CCamera* camera) const;

	int ResolveStaticWorldLodLevel(const StaticWorldLodEntry& entry, int desiredLod) const;
	int GetStaticObjectActiveLodLevel(UINT objectIndex) const;
	void BuildStaticWorldLodEntryIndexMap();

    // Frame / Render
public:
    bool ProcessInput(UCHAR* pKeysBuffer) override;
    void AnimateObjects(float dt) override;
    void CollisionObjects() override;
	//void RenderShadowMap(ID3D12GraphicsCommandList* cmd, const CGameTimer& gt);

public:
	public:
		void OnPrepareRender(ID3D12GraphicsCommandList* cmd, CCamera* camera) override;
		void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera = nullptr) override;
		void RenderShadowPrePass(ID3D12GraphicsCommandList* cmd, CCamera* camera);
		void RenderSceneGeometry(ID3D12GraphicsCommandList* cmd, CCamera* camera);
		void RenderSsao(ID3D12GraphicsCommandList* cmd, CCamera* camera);
		void RenderSceneComposite(ID3D12GraphicsCommandList* cmd, CCamera* camera);

		void RebindFrameRenderState(ID3D12GraphicsCommandList* cmd, CCamera* camera);
		void SetFrameResourceIndex(UINT frameResourceIndex);

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
	void SetKeyItemDiffuseSrvIndex(UINT srvIndex);
	void SetTransparentItemDiffuseSrvIndex(UINT srvIndex);
	void SetBossSummonCircleDiffuseSrvIndex(UINT srvIndex);
	void SetBossCallSummonCircleDiffuseSrvIndex(UINT srvIndex);

	void SetInactiveOverlayVisible(bool visible)
	{
		m_bInactiveOverlayVisible = visible;
		m_hud.SetInactiveOverlayVisible(visible);
	}

	void SetDepthFogSourceSrvIndices(UINT sceneColorSrvIndex, UINT sceneNormalSrvIndex, UINT sceneDepthSrvIndex)
	{
		m_depthFog.SetSourceSrvIndices(sceneColorSrvIndex, sceneDepthSrvIndex);
		mSsaoSceneNormalMapSrvIndex = sceneNormalSrvIndex;
		mSsaoDepthMapSrvIndex = sceneDepthSrvIndex;

		if ( mSsao )
			mSsao->SetDepthSrvIndex(sceneDepthSrvIndex);
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

		if ( m_pd3dSsaoDevice )
			CreateSsaoRtvs(m_pd3dSsaoDevice);
	}

	void SetDepthFogPassEnabled(bool enabled) { m_depthFog.SetPassEnabled(enabled); }

	bool RequestUseInventoryItemSlot(int slot);
	void SetInventoryItemCounts(const std::array<int, CGameSceneHUD::kInventorySlotCount>& counts);

	CNavMesh* GetNavMesh() { return m_navMesh.get(); }
	const CNavMesh* GetNavMesh() const { return m_navMesh.get(); }

    CGameObject* GetDemoFighter(int index) const;
    void RequestDemoFighterAttack(int index);

	CGameObject* GetPlayerBySlot(int slot) const; // slot: 0..3
	bool IsLocalPlayer(const CGameObject* obj) const;
	bool IsPlayerInsideMegaGridCenter(const CGameObject* player) const;
	bool IsPlayerInsideBossStageBattleArea(const CGameObject* player) const;
	bool IsLocalPlayerDead() const { return m_bLocalPlayerDead; }
	bool RollbackLocalPlayerMoveIfCollidingWorldStatic(const XMFLOAT3& previousPos);
#ifdef USING_NETWORK
	void ApplyNetworkPredictedTerrainY(CGameObject* obj);
#endif
    
	void RequestFireArrow(CGameObject* shooter, float speed, float lifeSec = 3.0f, float yOffset = 0.0f);
	bool IsLocalPlayerInsideMegaGridCenter() const;
	bool IsLocalMonsterChaseEnabled() const { return m_bSimulateLocalMonsterChase; }

	const std::vector<CGameObject*>& GetMegaGridMonstersByWorldPosition(
		const XMFLOAT3& worldPos
	) const;

	void BeginBossCallMonsterSummonVisuals(
		int callIndex,
		float fadeInDurationSec
	);

	int SpawnBossCallMonsters(int callIndex);

	float GetBossCallMonsterSpawnDelaySec() const
	{
		return kBossCallMonsterSpawnDelaySec;
	}

	void NotifyMonsterChaseStarted(CGameObject* monster);

	void SetMegaGridApproachZoneSize(int megaX, int megaZ, int widthCells, int heightCells);
	void SetMegaGridCleared(int megaX, int megaZ, bool cleared = true);
	void SetMegaGridEventOccurred(int megaX, int megaZ, bool occurred = true);

	bool HasMegaGridPlayerApproached(int megaX, int megaZ) const;
	bool IsMegaGridCleared(int megaX, int megaZ) const;
	bool HasMegaGridEventOccurred(int megaX, int megaZ) const;

private:
#ifndef USING_NETWORK
	struct DoorPortalSubBoxRef
	{
		size_t meshSetIndex = static_cast< size_t >( -1 );
		size_t subIndex = static_cast< size_t >( -1 );
	};

	using TowerDoorSubBoxRef = DoorPortalSubBoxRef;

	struct TowerDoorPortalEntry
	{
		CGameObject* tower = nullptr;
		CColliderComponent* collider = nullptr;

		std::vector<DoorPortalSubBoxRef> doorARefs;
		std::vector<DoorPortalSubBoxRef> doorBRefs;

		int cooldownFrames = 0;
	};

	struct CastleDoorPortalPair
	{
		int sourceDoorIndex = -1;
		int targetDoorIndex = -1;

		std::vector<DoorPortalSubBoxRef> sourceRefs;
		std::vector<DoorPortalSubBoxRef> targetRefs;
	};

	struct CastleDoorPortalEntry
	{
		CGameObject* castle = nullptr;
		CColliderComponent* collider = nullptr;

		std::array<std::vector<DoorPortalSubBoxRef>, 8> doorRefsByIndex;
		std::vector<CastleDoorPortalPair> pairs;

		int cooldownFrames = 0;
	};

	void RegisterTowerDoorPortal(CGameObject* tower);
	void RegisterCastleDoorPortal(CGameObject* castle);

	void TickTowerDoorPortalCooldowns();
	bool IsTowerDoorPortalOnCooldown() const;

	int CountClearedMegaGrids() const;
	bool CanUseCastleDoorPortal() const;

	bool TryTeleportLocalPlayerByTowerDoorPortal(bool forceLog = false);
	bool TryTeleportLocalPlayerByCastleDoorPortal(bool forceLog = false);
#endif

	using EGridDynamicKind = CSceneGrid::EDynamicKind;
	using GridDynamicTracker = CSceneGrid::DynamicTracker;

	void InitializeSpatialGrid();
	void ShutdownSpatialGrid();

	bool ShouldCullTreesByVillageDoorProbes(CCamera* camera) const;
	bool IsAnyVillageWallTreeCullDoorProbeVisible(int megaGridNumber, CCamera* camera) const;

	void AddDynamicCount(int cellX, int cellZ, EGridDynamicKind kind, int delta);
	void RegisterStaticPlacementToGrid(const StaticPlacementEntry& placement, CGameObject* obj);

	void ResetDynamicGridCounts();
	bool TryGetTrackedCell(const CGameObject* obj, int& outCellX, int& outCellZ) const;
	void RefreshDynamicTracker(GridDynamicTracker& tracker, EGridDynamicKind kind);
	void BuildDynamicGridTrackers();
	void RebuildDynamicGridState();
	void UpdateDynamicGridState();
	void UpdateMegaGridState();

	void UpdateMegaGrid5DirectionalLightState();
	void ApplyMegaGrid5DirectionalLightProfile(bool enabled);

	bool ShouldUseBossStageBgm() const;
	void UpdateBossStageBgmState();

	void UpdateMegaGrid4LowYPoison(float dt);
	bool IsPlayerInsideMegaGrid4LowYPoisonArea(const CGameObject* player) const;

	bool TryTeleportLocalPlayerToMegaGridByNumber(int megaGridNumber);
	XMFLOAT3 ComputeLocalStageTeleportPosition(int megaGridNumber) const;
	XMFLOAT3 ComputeMegaGridCenterPosition(int megaGridNumber, float y) const;

	XMFLOAT3 ComputeEnemySpawnerSpawnPosition(
		int megaGridNumber,
		UINT localIndex,
		UINT localCount
	) const;

	XMFLOAT3 ComputeBossCallMonsterSpawnPosition() const;
	float ComputeBossCallMonsterSpawnYawDeg() const;

	bool IsMegaGridNumberCleared(int megaGridNumber) const;

	bool ShouldBlockEnemySpawnerByClearedPrerequisite(
		int targetMegaGridNumber,
		int& outBlockerMegaGridNumber
	) const;

	int SpawnPreparedEnemiesInMegaGrid(int megaGridNumber);
	int TryRunEnemySpawnerEventForMegaGrid(int megaGridNumber);

	bool BeginEnemySpawnerTimedGhoulWave(int megaGridNumber);
	void UpdateEnemySpawnerTimedGhoulWaves(float dt);

	int SpawnEnemySpawnerDoorGhoulBatch(
		int megaGridNumber,
		int batchIndex
	);

	XMFLOAT3 ComputeEnemySpawnerDoorGhoulSpawnPosition(
		int megaGridNumber,
		int wallIndex,
		int slotIndex
	) const;

	float ComputeEnemySpawnerDoorGhoulSpawnYawDeg(
		int wallIndex
	) const;

	void ResetEnemySpawnerTimedGhoulWaveStates();

	void RegisterMonsterToMegaGrid(CGameObject* monster, const XMFLOAT3& spawnPosition, UINT skinnedBatchObjectIndex);
	int GetLocalPlayerMegaGridNumberForMonsterTick() const;
	bool ShouldSkipMonsterByMegaGrid(const CGameObject* monster, UINT skinnedBatchObjectIndex, int activeMegaGridNumber) const;
	void ResetMonsterToHomeForMegaGridSkip(CGameObject* monster) const;

	void SetLocalMonsterChaseEnabled(bool enabled);
	void StopAllLocalMonsterChaseAndReturnHome();
	void StopMonsterChaseAndReturnHome(CGameObject* monster) const;

	uint16_t ComputeStaticObjectMegaGridMask(CGameObject* obj) const;
	uint16_t ComputeObjectCurrentMegaGridMask(const CGameObject* obj) const;

	void SetObjectCollisionMegaGridMask(
		CGameObject* obj,
		uint16_t mask,
		bool fixedMask
	);

	void RefreshDynamicCollisionMegaGridMasks();

	bool ShouldKeepCollisionPairByMegaGrid(
		const CColliderComponent* a,
		const CColliderComponent* b
	) const;

	void MarkLocalPlayerEnteredCastleCenterMegaGrid(); 
	bool IsLocalPlayerInsideCastleCenterMegaGridFullArea() const;
	void UpdateCastleCenterMegaGridState();

	void DumpStaticGridOccupancyLog() const;

	struct EnemySpawnerTimedGhoulWaveState
	{
		bool active = false;

		// 0~9. Begin 시 0번 batch를 즉시 생성하고, 이후 nextBatchIndex는 1부터 시작한다.
		int nextBatchIndex = 0;

		float accumulatorSec = 0.0f;
	};

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
	void BuildSsaoResources(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd);
	void CreateSsaoRtvs(ID3D12Device* dev);
	void ReleaseSsaoResources();
	void ReleaseSsaoConstantBuffer();
	void UpdateSsaoCB(CCamera* camera);
	void RenderDepthFog(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	bool IsStaticObjectInsideShadowBox(UINT objectIndex) const;
	bool IsSkinnedObjectInsideShadowBox(UINT objectIndex) const;

	void RenderShadowMap(ID3D12GraphicsCommandList* cmd);
	void RenderStaticInstanceGroupsToShadowMap(ID3D12GraphicsCommandList* cmd);
	void RenderSkinnedInstanceGroupsToShadowMap(ID3D12GraphicsCommandList* cmd);
	void RestoreSceneRenderTargets(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	int GetLocalPlayerMegaGridNumberForDepthFog() const;
	void UpdateDepthFogState(float dt);

	void AttachInventoryComponentsToPlayers();
	CInventoryComponent* GetInventoryByPlayerSlot(int slot) const;
	CInventoryComponent* GetLocalPlayerInventory() const;
	void SyncLocalInventoryToHud();

	void InitializeInventoryItemCounts();
	int ApplyPlayerAttackPowerPotionMultiplier(int playerSlot, int attackPower) const;

	// slot 0..3 플레이어 포인터(소유는 m_skinnedObjects가 함)
	std::array<CGameObject*, 4> m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };
	std::array<bool, 4> m_otherPlayerWorldHpGaugeVisibleForHud = { false, false, false, false };

	std::array<bool, 4> m_playerFootstepTrackingValid = { false, false, false, false };
	std::array<int, 4> m_playerFootstepMode = { 0, 0, 0, 0 }; // 0=None, 1=Walk, 2=Run
	std::array<float, 4> m_playerFootstepPrevNormalizedTime = { 0.0f, 0.0f, 0.0f, 0.0f };

    int m_localPlayerSlot;
    // ------------------------------------------------------------------------
    // Build counts (현재는 BuildObjects()에서 결정, 추후 서버 동기화 값으로 대체)
    // ------------------------------------------------------------------------
    UINT m_grassCount = 1;
    UINT m_groundCount = 1;
    UINT m_villagewallCount = 1;
	UINT m_castleCount = 1;
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
	UINT m_terrainCount = 1;
	UINT m_waterCount = 1;

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

	static constexpr UINT kEnemySpawnerMega6GhoulCount = 200;
	static constexpr UINT kEnemySpawnerMega8GhoulCount = 200;

	static constexpr float kEnemySpawnerInactiveY = -100.0f;

	static constexpr int kEnemySpawnerDoorWallCount = 4;
	static constexpr int kEnemySpawnerDoorSlotsPerWall = 5;
	static constexpr int kEnemySpawnerDoorBatchCount = 10;
	static constexpr float kEnemySpawnerDoorBatchIntervalSec = 1.0f;

	// 200x200 벽의 원래 경계는 center +/- 100.
	// 요구사항에 따라 300 -> 301, 500 -> 499로 치환하므로 center +/- 99.
	static constexpr float kEnemySpawnerDoorWallHalfExtent = 99.0f;

	// 문 중앙 10m 안에서 5마리를 2m 간격으로 배치.
	// 실제 offset은 -4, -2, 0, +2, +4.
	static constexpr float kEnemySpawnerDoorSlotSpacing = 2.0f;

	static constexpr UINT kEnemySpawnerMega5GhoulCount = 70;
	static constexpr UINT kEnemySpawnerMega5BowManCount = 10;
	static constexpr UINT kEnemySpawnerMega5SwordManCount = 10;
	static constexpr UINT kEnemySpawnerMega5MutantCount = 5;

	UINT m_EnemySpawnCount = 0;

	UINT m_EnemySpawnGhoulCount = 0;
	UINT m_EnemySpawnBowManCount = 0;
	UINT m_EnemySpawnSwordManCount = 0;
	UINT m_EnemySpawnMutantCount = 0;

    std::vector<std::unique_ptr<CGameObject>> m_staticObjects;
    std::vector<std::unique_ptr<CGameObject>> m_skinnedObjects;
	std::vector<std::unique_ptr<CGameObject>> m_colliderObjects;

    SCENE_STATIC_BATCH  m_staticBatch;
    SCENE_SKINNED_BATCH m_skinnedBatch;
	SCENE_COLLIDER_BATCH m_colliderBatch;

	static constexpr UINT kItemBillboardKeyMaterialId = MAX_MATERIALS - 1;
	static constexpr UINT kTransparentItemBillboardMaterialId = MAX_MATERIALS - 2;
	static constexpr UINT kBossSummonCircleMaterialId = MAX_MATERIALS - 3;
	static constexpr UINT kBossSummonGlowMaterialId = MAX_MATERIALS - 4;
	static constexpr UINT kBossShockwaveMaterialId = MAX_MATERIALS - 5;
	static constexpr UINT kBossShockwaveWallMaterialId = MAX_MATERIALS - 6;
	static constexpr UINT kBossCallSummonCircleMaterialId = MAX_MATERIALS - 7;
	static constexpr UINT kBossDeathCircleMaterialId = MAX_MATERIALS - 18;
	static constexpr UINT kBossDeathRingMaterialId = MAX_MATERIALS - 19;
	static constexpr UINT kMonsterHpGaugeMaterialId = MAX_MATERIALS - 12;
	static constexpr UINT kMonsterHpGaugeEmptyMaterialId = MAX_MATERIALS - 13;
	static constexpr UINT kPlayerWorldHpNameMaterialBaseId = MAX_MATERIALS - 17;

	static constexpr UINT kPotionItemBillboardMaterialBaseId = MAX_MATERIALS - 11;
	static constexpr UINT kHealPotionItemBillboardMaterialId = MAX_MATERIALS - 11;
	static constexpr UINT kAttackPotionItemBillboardMaterialId = MAX_MATERIALS - 10;
	static constexpr UINT kDefensePotionItemBillboardMaterialId = MAX_MATERIALS - 9;
	static constexpr UINT kMoveSpeedPotionItemBillboardMaterialId = MAX_MATERIALS - 8;

	static constexpr UINT kKeyItemBillboardCount = 7;
	static constexpr UINT kPotionItemKindCount = 4;
	static constexpr UINT kPotionItemMaxCountPerKind = 50;
	static constexpr UINT kPotionItemCountPerMegaGrid = 6;
	static constexpr UINT kPotionItemSpawnMegaGridCount = 8;
	static constexpr UINT kPotionItemSpawnCountPerKind = kPotionItemCountPerMegaGrid * kPotionItemSpawnMegaGridCount;
	static constexpr UINT kPotionItemBillboardCount = kPotionItemKindCount * kPotionItemSpawnCountPerKind;
	static_assert( kPotionItemSpawnCountPerKind <= kPotionItemMaxCountPerKind, "Potion item spawn count exceeds max count per kind." );

	ItemBillboardState m_itemBillboardState;
	MonsterHpGaugeState m_monsterHpGaugeState;

	struct MonsterHpGaugeRuntimeState
	{
		int previousHp = -1;
		float visibleTimerSec = 0.0f;
	};

	static constexpr float kMonsterHpGaugeVisibleDurationSec = 5.0f;

	std::unordered_map<CGameObject*, MonsterHpGaugeRuntimeState> m_monsterHpGaugeRuntimeStates;

	static constexpr UINT kMuzzleFlashMaxCount = 4096;

	static constexpr int kPlayerWeaponEffectLevelCount = 3;

	struct PlayerMeleeTrailVisualDesc
	{
		XMFLOAT3 rootLocal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		XMFLOAT3 tipLocal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		float widthScale = 1.0f;
		XMFLOAT4 color = XMFLOAT4(0.55f, 0.80f, 1.0f, 1.0f);
		float alphaScale = 0.75f;
	};

	struct PlayerArrowTrailVisualDesc
	{
		float sampleLifetimeSec = 0.260f;
		float halfWidth = 0.075f;
		XMFLOAT4 color = XMFLOAT4(0.86f, 0.94f, 1.0f, 1.0f);
		float tailAlpha = 0.20f;
		float headAlpha = 0.80f;
		float alphaScale = 0.72f;
	};

	struct PlayerGunMuzzleFlashCoreVisualDesc
	{
		float size = 1.0f;
		float endSizeScale = 1.70f;
		float intensity = 1.0f;
		XMFLOAT4 color = XMFLOAT4(1.0f, 0.32f, 0.04f, 1.0f);
	};

	struct PlayerGunMuzzleFlashRingVisualDesc
	{
		float startSize = 0.20f * 1.10f;
		float endSize = 1.15f * 1.10f;
		float intensity = 1.20f;
		XMFLOAT4 color = XMFLOAT4(1.0f, 0.28f, 0.03f, 0.75f);
	};

	struct PlayerGunMuzzleFlashSparkVisualDesc
	{
		int count = 14;
		float sideScale = 0.35f;
		float liftScale = 0.18f;
		float liftBase = 0.12f;
		float speedScale = 1.10f;
		float startWidth = 0.10f;
		float startHeight = 0.42f;
		float endWidth = 0.05f;
		float endHeight = 0.28f;
		float rotationJitter = 0.35f;
		float intensity = 1.40f;
		float drag = 5.50f;
		XMFLOAT4 color = XMFLOAT4(1.0f, 0.52f, 0.08f, 1.0f);
	};

	struct PlayerGunSmokeVisualDesc
	{
		int count = 5;
		float lifetimeMin = 0.42f;
		float lifetimeMax = 0.62f;
		float startSizeMin = 0.20f;
		float startSizeMax = 0.34f;
		float endSizeScaleMin = 2.10f;
		float endSizeScaleMax = 2.70f;
		float rightSpeedMin = 0.95f;
		float rightSpeedMax = 1.80f;
		float forwardSpeedMin = -0.18f;
		float forwardSpeedMax = 0.42f;
		float liftSpeedMin = 0.20f;
		float liftSpeedMax = 0.55f;
		float spawnRightOffsetMin = 0.05f;
		float spawnRightOffsetMax = 0.18f;
		float spawnForwardJitter = 0.10f;
		float spawnUpJitter = 0.07f;
		float drag = 1.10f;
		float gravity = 0.03f;
		XMFLOAT4 color = XMFLOAT4(0.015f, 0.014f, 0.013f, 0.38f);
	};

	struct PlayerGunMuzzleFlashVisualDesc
	{
		std::array<PlayerGunMuzzleFlashCoreVisualDesc, 2> cores = {};
		PlayerGunMuzzleFlashRingVisualDesc ring = {};
		PlayerGunMuzzleFlashSparkVisualDesc spark = {};
		PlayerGunSmokeVisualDesc smoke = {};
	};

	int GetPlayerWeaponEffectLevelIndex() const;
	const PlayerMeleeTrailVisualDesc& GetPlayerSwordTrailVisualDesc() const;
	const PlayerMeleeTrailVisualDesc& GetPlayerAxeTrailVisualDesc() const;
	const PlayerArrowTrailVisualDesc& GetPlayerArrowTrailVisualDesc() const;
	const PlayerGunMuzzleFlashVisualDesc& GetPlayerGunMuzzleFlashVisualDesc() const;

	const std::array<PlayerMeleeTrailVisualDesc, kPlayerWeaponEffectLevelCount> m_playerSwordTrailVisualDescs =
	{ {
		PlayerMeleeTrailVisualDesc{ XMFLOAT3(0.0f, 0.0f, 0.10f), XMFLOAT3(0.0f, 0.0f, 1.45f), 1.00f, XMFLOAT4(0.55f, 0.80f, 1.00f, 1.0f), 0.75f },
		PlayerMeleeTrailVisualDesc{ XMFLOAT3(0.0f, 0.0f, 0.10f), XMFLOAT3(0.0f, 0.0f, 1.52f), 1.12f, XMFLOAT4(0.18f, 0.62f, 1.00f, 1.0f), 0.86f },
		PlayerMeleeTrailVisualDesc{ XMFLOAT3(0.0f, 0.0f, 0.10f), XMFLOAT3(0.0f, 0.0f, 1.60f), 1.25f, XMFLOAT4(1.00f, 0.00f, 0.00f, 1.0f), 0.94f }
	} };

	const std::array<PlayerMeleeTrailVisualDesc, kPlayerWeaponEffectLevelCount> m_playerAxeTrailVisualDescs =
	{ {
		PlayerMeleeTrailVisualDesc{ XMFLOAT3(0.0f, 0.0f, 0.80f), XMFLOAT3(0.0f, 0.0f, 1.45f), 0.80f, XMFLOAT4(0.55f, 0.80f, 1.00f, 1.0f), 0.75f },
		PlayerMeleeTrailVisualDesc{ XMFLOAT3(0.0f, 0.0f, 0.78f), XMFLOAT3(0.0f, 0.0f, 1.52f), 0.92f, XMFLOAT4(0.18f, 0.62f, 1.00f, 1.0f), 0.86f },
		PlayerMeleeTrailVisualDesc{ XMFLOAT3(0.0f, 0.0f, 0.76f), XMFLOAT3(0.0f, 0.0f, 1.60f), 1.05f, XMFLOAT4(1.00f, 0.00f, 0.00f, 1.0f), 0.94f }
	} };

	const std::array<PlayerArrowTrailVisualDesc, kPlayerWeaponEffectLevelCount> m_playerArrowTrailVisualDescs =
	{ {
		PlayerArrowTrailVisualDesc{ 0.260f, 0.075f, XMFLOAT4(0.86f, 0.94f, 1.00f, 1.0f), 0.20f, 0.80f, 0.72f },
		PlayerArrowTrailVisualDesc{ 0.340f, 0.085f, XMFLOAT4(0.18f, 0.62f, 1.00f, 1.0f), 0.20f, 0.80f, 0.72f },
		PlayerArrowTrailVisualDesc{ 0.440f, 0.096f, XMFLOAT4(1.00f, 0.00f, 0.00f, 1.0f), 0.20f, 0.80f, 0.72f }
	} };

	const std::array<PlayerGunMuzzleFlashVisualDesc, kPlayerWeaponEffectLevelCount> m_playerGunMuzzleFlashVisualDescs =
	{ {
		PlayerGunMuzzleFlashVisualDesc{ {{ PlayerGunMuzzleFlashCoreVisualDesc{ 0.55f * 1.10f, 1.70f, 2.20f, XMFLOAT4(1.00f, 0.32f, 0.04f, 1.00f) }, PlayerGunMuzzleFlashCoreVisualDesc{ 0.80f * 1.10f, 1.70f, 1.50f, XMFLOAT4(1.00f, 0.32f, 0.04f, 0.75f) } }}, PlayerGunMuzzleFlashRingVisualDesc{ 0.20f * 1.10f, 1.15f * 1.10f, 1.20f, XMFLOAT4(1.00f, 0.28f, 0.03f, 0.75f) }, PlayerGunMuzzleFlashSparkVisualDesc{ 14, 0.35f, 0.18f, 0.12f, 1.10f, 0.10f, 0.42f, 0.05f, 0.28f, 0.35f, 1.40f, 5.50f, XMFLOAT4(1.00f, 0.52f, 0.08f, 1.00f) }, PlayerGunSmokeVisualDesc{ 5, 0.42f, 0.62f, 0.20f, 0.34f, 2.10f, 2.70f, 0.95f, 1.80f, -0.18f, 0.42f, 0.20f, 0.55f, 0.05f, 0.18f, 0.10f, 0.07f, 1.10f, 0.03f, XMFLOAT4(0.015f, 0.014f, 0.013f, 0.38f) } },
		PlayerGunMuzzleFlashVisualDesc{ {{ PlayerGunMuzzleFlashCoreVisualDesc{ 0.70f, 1.82f, 2.55f, XMFLOAT4(1.00f, 0.44f, 0.05f, 1.00f) }, PlayerGunMuzzleFlashCoreVisualDesc{ 1.02f, 1.82f, 1.85f, XMFLOAT4(1.00f, 0.36f, 0.04f, 0.84f) } }}, PlayerGunMuzzleFlashRingVisualDesc{ 0.27f, 1.48f, 1.45f, XMFLOAT4(1.00f, 0.34f, 0.03f, 0.82f) }, PlayerGunMuzzleFlashSparkVisualDesc{ 18, 0.42f, 0.22f, 0.15f, 1.22f, 0.12f, 0.50f, 0.055f, 0.32f, 0.42f, 1.62f, 5.20f, XMFLOAT4(1.00f, 0.62f, 0.10f, 1.00f) }, PlayerGunSmokeVisualDesc{ 8, 0.50f, 0.76f, 0.26f, 0.44f, 2.25f, 2.95f, 1.20f, 2.35f, -0.22f, 0.52f, 0.25f, 0.70f, 0.07f, 0.24f, 0.14f, 0.10f, 1.00f, 0.02f, XMFLOAT4(0.018f, 0.017f, 0.016f, 0.44f) } },
		PlayerGunMuzzleFlashVisualDesc{ {{ PlayerGunMuzzleFlashCoreVisualDesc{ 0.86f, 1.95f, 2.95f, XMFLOAT4(1.00f, 0.55f, 0.08f, 1.00f) }, PlayerGunMuzzleFlashCoreVisualDesc{ 1.24f, 1.95f, 2.18f, XMFLOAT4(1.00f, 0.28f, 0.02f, 0.90f) } }}, PlayerGunMuzzleFlashRingVisualDesc{ 0.34f, 1.78f, 1.75f, XMFLOAT4(1.00f, 0.25f, 0.02f, 0.88f) }, PlayerGunMuzzleFlashSparkVisualDesc{ 24, 0.50f, 0.28f, 0.18f, 1.36f, 0.145f, 0.62f, 0.065f, 0.38f, 0.52f, 1.88f, 4.85f, XMFLOAT4(1.00f, 0.70f, 0.12f, 1.00f) }, PlayerGunSmokeVisualDesc{ 12, 0.60f, 0.90f, 0.34f, 0.58f, 2.45f, 3.25f, 1.45f, 2.90f, -0.28f, 0.64f, 0.30f, 0.88f, 0.09f, 0.31f, 0.18f, 0.13f, 0.90f, 0.015f, XMFLOAT4(0.020f, 0.019f, 0.018f, 0.52f) } }
	} };

	MuzzleFlashEffectState m_muzzleFlashEffect;
	static constexpr UINT kGunSmokeMaxCount = 512;
	GunSmokeEffectState m_gunSmokeEffect;

	BossPoisonProjectileEffectState m_bossPoisonProjectileEffect;

	static constexpr UINT kSwordTrailMaxCount = 16;
	static constexpr UINT kSwordTrailMaxSamples = 12;
	static constexpr UINT kSwordTrailMaxVertices =
		kSwordTrailMaxCount * kSwordTrailMaxSamples * 2;

	SwordTrailEffectState m_swordTrailEffect;

	static constexpr UINT kMonsterSwordTrailMaxCount = 32;
	static constexpr UINT kMonsterSwordTrailMaxSamples = 12;
	static constexpr UINT kMonsterSwordTrailMaxVertices =
		kMonsterSwordTrailMaxCount * kMonsterSwordTrailMaxSamples * 2;

	MonsterSwordTrailEffectState m_monsterSwordTrailEffect;

	static constexpr UINT kArrowTrailMaxCount = 32;
	static constexpr UINT kArrowTrailMaxSamples = 12;
	static constexpr UINT kArrowTrailMaxVertices =
		kArrowTrailMaxCount * kArrowTrailMaxSamples * 2;

	ArrowTrailEffectState m_arrowTrailEffect;

	static constexpr UINT kMonsterArrowTrailMaxCount = 32;
	static constexpr UINT kMonsterArrowTrailMaxSamples = 12;
	static constexpr UINT kMonsterArrowTrailMaxVertices =
		kMonsterArrowTrailMaxCount * kMonsterArrowTrailMaxSamples * 2;

	ArrowTrailEffectState m_monsterArrowTrailEffect;

	static constexpr UINT kBossCallSummonWwwMaxCount = 64;

	static constexpr UINT kBossCallSummonWwwPathPointCount =
		kBossCallSummonWwwPeakCount * 2 + 1;

	// 외곽선: path point마다 안쪽/바깥쪽 2개 vertex.
	static constexpr UINT kBossCallSummonWwwOutlineVertexCount =
		kBossCallSummonWwwPathPointCount * 2;

	// 내부 채움: W 한 개당 삼각형 1개, 삼각형당 vertex 3개.
	static constexpr UINT kBossCallSummonWwwFillVertexCount =
		kBossCallSummonWwwPeakCount * 3;

	static constexpr UINT kBossCallSummonWwwMaxVertices =
		kBossCallSummonWwwMaxCount *
		(
			kBossCallSummonWwwOutlineVertexCount +
			kBossCallSummonWwwFillVertexCount
		);

	BossCallSummonWwwEffectState m_bossCallSummonWwwEffect;

	std::vector<CGameObject*> m_ghoulRefs;
	std::vector<CGameObject*> m_swordManRefs;
	std::vector<CGameObject*> m_bowManRefs;
	std::vector<CGameObject*> m_MutantRefs;
	std::vector<CGameObject*> m_bossRefs;

	std::unordered_map<CGameObject*, int> m_mutantKeyTriggerMegaByObject;
	std::array<bool, CSceneGrid::kMegaGridCount + 1> m_mutantKeyTriggerRegisteredByMega = {};

    std::vector<CGameObject*> m_helmetRefs;

    std::vector<CGameObject*> m_PlayerSwordRefs;
    std::vector<CGameObject*> m_PlayerBowRefs;
    std::vector<CGameObject*> m_PlayerAxeRefs;
    std::vector<CGameObject*> m_PlayerGunRefs;

	std::unordered_map<CGameObject*, CGameObject*> m_playerWeaponOwnerByObject;

    std::vector<CGameObject*> m_EnemySwordRefs;
    std::vector<CGameObject*> m_EnemyBowRefs;
	std::vector<CGameObject*> m_EnemySpawnRefs;
	std::vector<EnemySpawnerPoolEntry> m_enemySpawnPoolEntries;

    std::vector<AttachmentBindSpec> m_attachmentBinds;

	static constexpr UINT kArrowPoolSize = 32;
	static constexpr UINT kBulletPoolSize = 32;
	std::vector<CGameObject*> m_arrowRefs;
	std::vector<CGameObject*> m_bulletRefs;
	std::unordered_map<uint64_t, CGameObject*> m_networkArrowById;
	std::unordered_map<uint64_t, CGameObject*> m_networkBulletById;
	std::unordered_map<uint64_t, int>          m_networkBossPoisonById;

	std::array<CGameObject*, 4> m_preparedPlayerArrows = { nullptr, nullptr, nullptr, nullptr };
	std::array<bool, 4> m_prevBowReleasePhase = { false, false, false, false };
	std::array<bool, 4> m_prevBowLoadPhase = { false, false, false, false };

	std::vector<CGameObject*> m_preparedBowmanArrows;

	std::vector<bool> m_prevEnemyBowReleasePhase;

	enum class EMonsterSfxKind : uint8_t
	{
		None = 0,
		Footstep,
		SwordWhoosh,
		MutantWhoosh,
		GhoulWhoosh,
		BowLoading,
		BowRelease,
		BossAttack
	};

	enum class EMonsterFootstepProfile : uint8_t
	{
		Humanoid = 0, // SwordMan / BowMan
		Ghoul,
		Mutant
	};

	struct MonsterFootstepSfxState
	{
		bool valid = false;
		int mode = 0; // 0=None, 1=Walk, 2=Run
		float prevNormalizedTime = 0.0f;
	};

	struct PendingMonsterSfx
	{
		EMonsterSfxKind kind = EMonsterSfxKind::None;
		CGameObject* owner = nullptr;
		const char* path = nullptr;

		float timer = 0.0f;
		float originalDelay = 0.0f;
		float volume = 1.0f;

		bool followOwner = true;
	};

	struct ActiveMonsterSfx
	{
		EMonsterSfxKind kind = EMonsterSfxKind::None;
		FMOD::Channel* channel = nullptr;

		CGameObject* followTarget = nullptr;

		XMFLOAT3 prevPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
		bool hasPrevPosition = false;
	};

	std::vector<MonsterFootstepSfxState> m_monsterFootstepSfxStates;

	std::vector<bool> m_prevGhoulAttackPhase;
	std::vector<bool> m_prevSwordManAttackPhase;
	std::vector<bool> m_prevMutantAttackPhase;
	std::vector<bool> m_prevBowManSfxLoadPhase;

	std::vector<PendingMonsterSfx> m_pendingMonsterSfxList;
	std::vector<ActiveMonsterSfx> m_activeMonsterSfxList;

	int GetPlayerSlotFromObject(const CGameObject* obj) const;
	int GetBowManIndexFromObject(const CGameObject* obj) const;
	int GetSwordManIndexFromObject(const CGameObject* obj) const;

	CGameObject* ResolvePlayerAttackerFromPlayerWeapon(CGameObject* weaponObject) const;
	bool ForceMonsterAIChaseTarget(CGameObject* monster, CGameObject* target) const;

	void RequestPrepareArrow(CGameObject* shooter, float pullBackDistance);
	void RequestReleasePreparedArrow(CGameObject* shooter, float speed, float lifeSec = 3.0f);

	void RequestPrepareBowmanArrow(CGameObject* bowman, float pullBackDistance);
	void RequestReleasePreparedBowmanArrow(CGameObject* bowman, float speed, float lifeSec = 3.0f);
	void RequestFireBullet(CGameObject* shooter, float speed, float lifeSec = 3.0f);
	void RequestPlayerAttackSfx(CGameObject* player);

	void UpdatePlayerBowSfxOnly();
	void UpdatePreparedBowArrows();
	void UpdateLocalPlayerDeathAndRespawn(float dt);
	void BeginLocalPlayerDeath(CGameObject* player);
	void RespawnLocalPlayer(CGameObject* player);
	void SetLocalPlayerControlEnabled(bool enabled);
	void CancelLocalPlayerPreparedActions();

	void UpdatePlayerFootstepSfx();
	void ResetPlayerFootstepSfxState();
	void PlayPlayerFootstepSfx(CGameObject* player);
	void PlayPlayerDeathSfxAt(const XMFLOAT3& position);

	void ResetMonsterSfxState();

	void UpdateMonsterSfx(float dt);
	void UpdateMonsterFootstepSfx();
	void UpdateMonsterAttackSfx();

	void TrackMonsterFootstepSfx(
		CGameObject* monster,
		MonsterFootstepSfxState& state,
		EMonsterFootstepProfile profile
	);

	void PlayMonsterFootstepSfx(CGameObject* monster);

	void RequestGhoulAttackSfx(CGameObject* ghoul);
	void RequestSwordManAttackSfx(CGameObject* swordman);
	void RequestMutantAttackSfx(CGameObject* mutant);
	void RequestBowManLoadSfx(CGameObject* bowman);
	void RequestBossAttackSfx(CGameObject* boss);

	void PlayBossSummonSfxAt(const XMFLOAT3& position);
	void PlayBossSummonCircleSfxAt(const XMFLOAT3& position);

	void PlayBossCallSummonCircleSfxAt(const XMFLOAT3& position);
	void PlayBossCallMonsterSpawnSfxAt(const XMFLOAT3& position);

	void PlayEnemySpawnerSirenSfxAt(const XMFLOAT3& position);
	void PlayBossSpellSfxAt(const XMFLOAT3& position);
	void PlayBossDeathSfxAt(const XMFLOAT3& position);

	void PlayBossShockwaveWindSfxAt(const XMFLOAT3& position);
	void UpdateBossShockwaveWindSfx(float currentRadius);
	void ResetBossShockwaveWindSfxTracking();

	void ScheduleMonsterSfx(
		EMonsterSfxKind kind,
		CGameObject* owner,
		const char* soundPath,
		float delaySeconds,
		float volume,
		bool followOwner = true
	);

	void UpdatePendingMonsterSfx(float dt);
	void PlayPendingMonsterSfxAt(size_t index);
	void UpdateActiveMonsterSfx();

	void UpdateMonsterDeathStates();
	void BeginMonsterDeath(CGameObject* monster);
	void CancelMonsterPreparedActions(CGameObject* monster);
	bool IsMonsterDead(const CGameObject* monster) const;

	void MarkMegaGridClearedByNumber(int megaGridNumber);
	bool AreAllMonstersInMegaGridDead(int megaGridNumber) const;
	void UpdateMegaGridClearStateFromMonsterDeaths();

	int ComputePlayerWeaponDamageTierIndexFromClearedMegaGrids() const;
	void RefreshPlayerWeaponDamageTierFromClearedMegaGrids();
	void RefreshPlayerWeaponAttackPowers();
	void RefreshPlayerWeaponAttackPowersForSlot(int playerSlot);
	void RefreshPlayerWeaponEffectVisuals();

	int GetPlayerSwordAttackPower(int playerSlot) const;
	int GetPlayerAxeAttackPower(int playerSlot) const;
	int GetPlayerArrowAttackPower(int playerSlot) const;
	int GetPlayerBulletAttackPower(int playerSlot) const;

	bool IsBossMonsterObject(const CGameObject* monster) const;
	bool IsEnemySpawnerMonsterObject(const CGameObject* monster) const;

	bool AreAllPreBossMonstersInMegaGridDead(int megaGridNumber) const;
	void DamagePreBossMonstersInMegaGrid(int megaGridNumber, int damage);

	void SetBossStageBossActive(CGameObject* boss, bool active, bool playAppear);

	bool TryBeginBossStageSummonSequence();
	bool TryActivateBossStageBoss();
	void UpdateBossStageSummonSequence(float dt);

	void StartBossSummonVisualFadeOut();
	void UpdateBossSummonVisualFadeOut(float dt);

	void SetBossStageBossAIEnabled(CGameObject* boss, bool enabled); 
	void RegisterBossStageBossOriginalPosition(CGameObject* boss, const XMFLOAT3& originalPosition);
	void MoveBossStageBossToHiddenPosition(CGameObject* boss);
	void ScheduleBossStageBossPositionRestore(CGameObject* boss, int delayFrames);
	void UpdateBossStageBossPositionRestores();

	bool IsBossStageBossRenderAllowed(const CGameObject* boss) const;
	void SetBossStageBossRenderAllowed(CGameObject* boss, bool allowed);
	void UpdateBossStageBossRenderGate();

	void RegisterMutantKeyTriggerIfNeeded(CGameObject* mutant, int megaGridNumber);
	void UnlockKeyBillboardForMegaGrid(int megaGridNumber);
	void HandleMutantKeyTriggerDeath(CGameObject* monster);

    std::array<CGameObject*, 3> m_demoFighters = { nullptr, nullptr, nullptr };

    std::vector<std::unique_ptr<CGameObject>> m_lightObjects;
    CFollowTransformComponent* m_pPlayerSpotFollower = nullptr;

	// GPU / Shader Variables (Game 전용)
	static constexpr UINT kFrameResourceCount = 2;

	std::array<ComPtr<ID3D12Resource>, kFrameResourceCount> m_pd3dcbLights;
	std::array<LIGHTS*, kFrameResourceCount> m_pcbMappedLights = {};
	UINT m_nLightsCBElementBytes = 0;

	std::unique_ptr<MATERIALS> m_pMaterials;

	std::array<ComPtr<ID3D12Resource>, kFrameResourceCount> m_pd3dcbMaterials;
	std::array<MATERIALS*, kFrameResourceCount> m_pcbMappedMaterials = {};
	UINT m_nMaterialsCBElementBytes = 0;

	std::array<ComPtr<ID3D12Resource>, kFrameResourceCount> m_pd3dcbTerrain;
	std::array<TERRAIN*, kFrameResourceCount> m_pcbMappedTerrain = {};
	UINT m_nTerrainCBElementBytes = 0;

	std::array<ComPtr<ID3D12Resource>, kFrameResourceCount> m_pd3dcbWater;
	std::array<WATER*, kFrameResourceCount> m_pcbMappedWater = {};
	UINT m_nWaterCBElementBytes = 0;

	std::array<ComPtr<ID3D12Resource>, kFrameResourceCount> m_pd3dcbSsao;
	std::array<SsaoCB*, kFrameResourceCount> m_pcbMappedSsao = {};
	UINT m_nSsaoCBElementBytes = 0;

	std::shared_ptr<CTexture> m_waterBaseTexture;
	std::shared_ptr<CTexture> m_waterDetail0Texture;
	std::shared_ptr<CTexture> m_waterDetail1Texture;

	UINT m_waterBaseSrvIndex = UINT_MAX;
	UINT m_waterDetail0SrvIndex = UINT_MAX;
	UINT m_waterDetail1SrvIndex = UINT_MAX;

	std::unique_ptr<Ssao> mSsao;
	std::shared_ptr<CTexture> mSsaoNormalMap;
	std::shared_ptr<CTexture> mSsaoAmbientMap0;
	std::shared_ptr<CTexture> mSsaoAmbientMap1;
	std::shared_ptr<CTexture> mSsaoRandomVectorMap;
	std::shared_ptr<CSsaoShader> mSsaoShader;
	std::shared_ptr<CSsaoBlurShader> mSsaoBlurShader;

	UINT mSsaoNormalMapSrvIndex = UINT_MAX;
	UINT mSsaoSceneNormalMapSrvIndex = UINT_MAX;
	UINT mSsaoAmbientMap0SrvIndex = UINT_MAX;
	UINT mSsaoAmbientMap1SrvIndex = UINT_MAX;
	UINT mSsaoRandomVectorMapSrvIndex = UINT_MAX;
	UINT mSsaoDepthMapSrvIndex = UINT_MAX;
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 3> mSsaoRtvHandles = {};
	bool m_bSsaoResourcesReady = false;
	bool m_bSsaoRtvsReady = false;
	ID3D12Device* m_pd3dSsaoDevice = nullptr;

	float m_waterAccumulatedTime = 0.0f;
	float m_waterHeight = -0.01f;
	float m_waterBaseUvScale = 1.0f;
	float m_waterAlpha = 1.0f;

	UINT m_nFrameResourceIndex = 0;

	CDepthFogSystem                 m_depthFog;
	float                           m_fElapsedTime = 0.0f;

	bool                            m_bLocalPlayerDead = false;
	bool                            m_bLocalPlayerRespawnUsed = false;
	float                           m_localPlayerRespawnTimer = 0.0f;

#ifdef USING_NETWORK
	std::deque<FrameSnapshot> m_frameSnapshotBuffer;
	uint64_t m_lastReceivedServerTick = 0;
	float m_timeSinceLastFramePacket = 0.0f;

	static constexpr uint64_t kNetworkInterpolationDelayTicks = 6;
	static constexpr size_t kMaxNetworkFrameSnapshotBufferSize = 8;
	static constexpr float kServerTickSeconds = 1.0f / 60.0f;

	void PushNetworkFrameSnapshot(const FrameSnapshot& snapshot);
	FrameSnapshot BuildInterpolatedFrameSnapshot(const FrameSnapshot& latestSnapshot) const;
	bool GetInterpolationSnapshots(
		uint64_t renderTick,
		const FrameSnapshot*& older,
		const FrameSnapshot*& newer,
		float& alpha) const;

	static const PlayerState* FindPlayerState(const FrameSnapshot& snapshot, uint64_t id);
	static const EnemyState* FindEnemyState(const FrameSnapshot& snapshot, uint64_t id);
	static XMFLOAT3 LerpPosition(const XMFLOAT3& a, const XMFLOAT3& b, float t);
	static float LerpYawDegrees(float a, float b, float t);

	struct NetworkActorYState
	{
		bool useServerY = false;
		float serverYHoldSec = 0.0f;
	};

	float SampleClientTerrainY(float worldX, float worldZ, float fallbackY) const;
	XMFLOAT3 ResolveNetworkActorY(
		uint64_t actorId,
		bool isPlayer,
		const XMFLOAT3& serverPos,
		float dt);

	std::unordered_map<uint64_t, uint32_t> m_prevPlayerNetworkStateCode;
	std::unordered_map<uint64_t, uint32_t> m_prevEnemyNetworkStateCode;
	std::unordered_map<uint64_t, int>      m_prevPlayerAnimTick;
	std::unordered_map<uint64_t, NetworkActorYState> m_networkPlayerYStates;
	std::unordered_map<uint64_t, NetworkActorYState> m_networkEnemyYStates;

	struct EnemyDRState
	{
		XMFLOAT3 predictedPos = {};
		XMFLOAT3 moveDir     = { 0.0f, 0.0f, 1.0f };
		float    speed       = 0.0f;
		bool     initialized = false;
	};
	std::unordered_map<uint64_t, EnemyDRState> m_enemyDRStates;
#endif

	int m_playerWeaponDamageTierIndex = 0; 
	std::unordered_set<CGameObject*> m_deadMonsters;

    unique_ptr<CCollisionSystem> m_Collision;
	std::unique_ptr<CNavMesh> m_navMesh;

	std::unique_ptr<EnemySpawner> m_enemySpawner;

	std::array<
		EnemySpawnerTimedGhoulWaveState,
		CSceneGrid::kMegaGridCount + 1
	> m_enemySpawnerTimedGhoulWaves = {};

#ifndef USING_NETWORK
	std::vector<MonsterSpawnEntry>	m_monsterSpawnEntries;

	std::vector<TowerDoorPortalEntry> m_towerDoorPortals;
	std::vector<CastleDoorPortalEntry> m_castleDoorPortals;
#endif

	CSceneGrid m_sceneGrid;

	std::array<GridDynamicTracker, 4> m_playerGridTrackers = {};
	std::vector<GridDynamicTracker> m_monsterGridTrackers;
	std::vector<GridDynamicTracker> m_arrowGridTrackers;
	std::vector<GridDynamicTracker> m_bulletGridTrackers;

	std::vector<int> m_skinnedMonsterMegaGridNumbers;

private:
    bool LoadStaticPlacementFile(const std::string& filePath);
	bool LoadSceneCubeBoxColliderReport(const std::string& filePath);
	bool ExportStaticWorldLocalOOBBReport(
	const std::string& filePath,
	const std::vector<size_t>& placementIndices,
	const std::vector<CGameObject*>& objects
	) const;
	void ResetStaticPlacementCounts();
	void ReleaseBuildOnlySceneData();
    void ApplyStaticPlacementCounts();
    static float QuaternionToYawDegrees(const XMFLOAT4& q);

private:
    std::vector<StaticPlacementEntry>   m_staticPlacementEntries;
	std::unordered_map<std::string, std::unordered_map<std::string, std::vector<AuthoredSubMeshOOBB>>> mSceneCubeBoxColliderTable;

	std::vector<StaticInstanceGroup>    m_staticInstanceGroups;
	std::vector<StaticRenderObjectCache> m_staticRenderObjectCache;
	std::vector<CGameObject*>            m_staticGameplayTickObjects;

	std::vector<StaticWorldLodEntry>    m_staticWorldLodEntries;
	std::vector<StaticOcclusionEntry>   m_staticOcclusionEntries;
	std::vector<int> m_staticWorldLodEntryIndexByObjectIndex;

	std::vector<uint8_t>                m_staticDistanceCullFlags;
	std::vector<uint8_t>                m_staticOcclusionCullFlags;
	std::vector<uint8_t>                m_staticTreeGridCullFlags;

	std::vector<uint8_t>                m_staticDynamicWorldMatrixFlags;
	std::vector<uint8_t>                m_staticShadowCasterFlags;
	std::vector<UINT>                   m_staticTreeObjectIndices;
	std::vector<int>                    m_staticShadowOcclusionEntryIndices;

	std::vector<uint16_t>               m_staticCollisionMegaGridMasks;

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
	std::array<ComPtr<ID3D12Resource>, kSceneBatchFrameResourceCount> m_pd3dStaticOcclusionInstanceBuffer;
	std::array<StaticInstanceVertex*, kSceneBatchFrameResourceCount> m_pMappedStaticOcclusionInstanceBuffer = {};

	std::array<ComPtr<ID3D12Resource>, kSceneBatchFrameResourceCount> m_pd3dStaticInstanceBuffer;
	std::array<StaticInstanceVertex*, kSceneBatchFrameResourceCount> m_pMappedStaticInstanceBuffer = {};
	UINT                                m_staticInstanceBufferCapacity = 0;

	std::shared_ptr<CStaticObjectsShader>             m_treeStaticShader;
	std::shared_ptr<CAlphaClipSkinnedObjectsShader>   m_skinnedAlphaClipShader;
	std::unordered_set<const CGameObject*>	m_treeAlphaClipObjects;
	std::unordered_set<const CGameObject*>	m_skinnedAlphaClipObjects;

	std::shared_ptr<COcclusionStaticShader>               m_occlusionStaticShader;
	std::shared_ptr<CShadowMapStaticShader>               m_shadowStaticShader;
	std::shared_ptr<CShadowMapAlphaClipStaticShader>      m_shadowAlphaClipStaticShader;
	std::shared_ptr<CShadowMapTerrainShader>			  m_shadowTerrainShader;
	std::shared_ptr<CShadowMapSkinnedShader>              m_shadowSkinnedShader;
	std::shared_ptr<CShadowMapAlphaClipSkinnedShader>     m_shadowAlphaClipSkinnedShader;

	struct SkyBoxVertex
	{
		XMFLOAT3 position;
		XMFLOAT2 uv;
	};

	struct SkyBoxState
	{
		std::shared_ptr<CSkyBoxShader> shader;
		std::shared_ptr<CTexture> texture;
		ComPtr<ID3D12Resource> vertexBuffer;
		ComPtr<ID3D12Resource> vertexUploadBuffer;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
		UINT vertexCount = 0;
		UINT textureBaseSrvIndex = UINT_MAX;
		CB_GAMEOBJECT_INFO objectCB{};
	};

	SkyBoxState m_skyBox;

	std::array<int, CGameSceneHUD::kInventorySlotCount> m_inventoryItemCounts = { 0, 0, 0, 0 };
	std::array<bool, CGameSceneHUD::kInventorySlotCount> m_bPrevInventoryUseKeyDown = { false, false, false, false };
	std::array<std::array<float, CGameSceneHUD::kInventorySlotCount>, 4> m_inventoryBuffParticleEmitAccumulators = {};

	CGameSceneHUD                       m_hud;
	CShadowMapSystem					m_shadowMap;

	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 8> m_sceneRtvHandles = {};
	D3D12_CPU_DESCRIPTOR_HANDLE                m_sceneDsvHandle = {};
	UINT                                       m_sceneRenderTargetCount = 0;
	bool                                       m_bSceneRenderTargetsReady = false;

	bool                                m_bInactiveOverlayVisible = false;
	bool                                m_bStartedGameplayMusic = false;
	bool                                m_bBossStageBgmActive = false;
	bool                                m_bWasLocalPlayerInsideMegaGridCenter = false;
	bool                                m_bShowShadowMapOverlay = true;

	bool m_bLocalPlayerInsideCastleCenterMegaGrid = false;
	bool m_bMegaGrid5DirectionalLightProfileActive = false;

	struct MegaGrid4LowYPoisonState
	{
		float exposureSec = 0.0f;
		float damageAccumulatorSec = 0.0f;
		bool poisoned = false;
	};

	static constexpr int   kMegaGrid4LowYPoisonMegaGridNumber = 4;
	static constexpr float kMegaGrid4LowYPoisonHalfExtent = 100.0f; // 중앙 200 x 200
	static constexpr float kMegaGrid4LowYPoisonMaxY = 2.8f;
	static constexpr float kMegaGrid4LowYPoisonGraceSec = 1.0f;
	static constexpr float kMegaGrid4LowYPoisonDamageIntervalSec = 1.0f;
	static constexpr int   kMegaGrid4LowYPoisonDamagePerTick = 5;

	std::array<MegaGrid4LowYPoisonState, 4> m_megaGrid4LowYPoisonStates = {};

	bool GetPauseOverlayRect(XMFLOAT4& outRect) const;

	std::vector<SkinnedInstanceGroup>   m_skinnedInstanceGroups;
	std::vector<SkinnedComponentCache>  m_skinnedComponentCache;

	std::array<ComPtr<ID3D12Resource>, kSceneBatchFrameResourceCount> m_pd3dSkinnedInstanceBuffer;
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
	std::array<ComPtr<ID3D12Resource>, kSceneBatchFrameResourceCount> m_pd3dSkinnedOcclusionInstanceBuffer;
	std::array<StaticInstanceVertex*, kSceneBatchFrameResourceCount> m_pMappedSkinnedOcclusionInstanceBuffer = {};
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
	std::array<SkinnedInstanceVertex*, kSceneBatchFrameResourceCount> m_pMappedSkinnedInstanceBuffer = {};
	UINT                                m_skinnedInstanceBufferCapacity = 0;

	std::array<ComPtr<ID3D12Resource>, kSceneBatchFrameResourceCount> m_pd3dSkinnedBonePaletteBuffer;
	std::array<XMFLOAT4X4*, kSceneBatchFrameResourceCount> m_pMappedSkinnedBonePaletteBuffer = {};

	std::vector<UINT>                   m_skinnedBonePaletteBaseByObject;

	std::vector<UINT>                   m_skinnedBonePaletteCountByObject;

	UINT                                m_skinnedBonePaletteCapacity = 0;

	// Terrain
	std::shared_ptr<TerrainData> m_TerrainData;
	std::shared_ptr<CTerrainShader> m_terrainShader;
	std::unordered_set<CGameObject*> m_terrainObjects;

	// Water
	std::shared_ptr<CWaterShader> m_waterShader;
	std::unordered_set<CGameObject*> m_waterObjects;

	// Ssao
	//std::unique_ptr<Ssao> mSsao;


	bool ShouldAttachObjectToTerrain(const std::string& assetName)
	{
		return assetName != "Terrain" &&
			assetName != "Water" ;
	}
	
	void BuildStaticWorldSubmeshOOBBDebugObjects(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd
	);
public:
	bool IsPointInPauseOverlay(POINT clientPt) const;
	bool IsPointInResumeButton(POINT clientPt) const;
	bool IsPointInExitButton(POINT clientPt) const;

private:
	bool m_bSimulateLocalPlayerMonsterAttackCollision = true;

	bool m_bSimulateLocalAI = true;
	bool m_bSimulateLocalGhoulAI = true;
	bool m_bSimulateLocalBowManAI = true;
	bool m_bSimulateLocalSwordManAI = true;
	bool m_bSimulateLocalMutantAI = true;
	bool m_bSimulateLocalBossAI = true;
	bool m_bSimulateLocalBossSummon = true;
	bool m_bSimulateLocalBossStageMonsterAI = true;

	bool m_bSimulateLocalMonsterChase = true;
	bool m_bPrevLocalMonsterChaseToggleKeyDown = false;
	bool m_bPrevDebugDamageMegaGrid5KeyDown = false;

	static constexpr float kBossCallMonsterSpawnDelaySec = 1.0f;

	bool m_bBossStageBossActivated = false;
	uint32_t m_serverBossRoomState = 0;

	static constexpr float kBossStageBossHiddenYOffset = -100.0f;

	static constexpr float kBossSummonCircleFadeInDurationSec = 3.0f;
	static constexpr float kBossSummonCircleFadeOutDurationSec = 1.0f;

	static constexpr UINT kBossCallSummonCircleMaxCount = 64;
	static constexpr float kBossCallSummonCircleFadeOutDurationSec = 1.8f;

	static constexpr float kMagicCircleGlowParticleYOffset = 0.12f;

	// 보스 본인 등장 마법진: 1개짜리 대형 마법진이므로 입자 수는 조금 유지하되 강도/크기를 낮춘다.
	static constexpr float kBossSummonGlowParticleEmitIntervalSec = 0.060f;
	static constexpr int   kBossSummonGlowParticlesPerEmit = 4;
	static constexpr float kBossSummonGlowParticleIntensityScale = 0.75f;

	// 보스 Call 몬스터 소환 마법진: 30~35개가 동시에 뜨므로 개별 마법진당 발생량을 낮게 유지한다.
	static constexpr float kBossCallSummonGlowParticleEmitIntervalSec = 0.090f;
	static constexpr int   kBossCallSummonGlowParticlesPerEmit = 1;
	static constexpr float kBossCallSummonGlowParticleIntensityScale = 0.65f;

	// 보스 Call 몬스터 소환 마법진 전용 파티클 크기 배율.
	// 보스 본인 등장 마법진에는 적용하지 않는다.
	static constexpr float kBossCallSummonGlowParticleSizeScale = 2.60f;
	static constexpr float kBossCallSummonAfterimageParticleSizeScale = 2.20f;

	// 보스 Call 몬스터 소환 마법진 전용 파티클 유지 시간 배율.
	// 보스 본인 등장 마법진에는 적용하지 않는다.
	static constexpr float kBossCallSummonGlowParticleLifetimeScale = 1.80f;

	int m_bossCallSummonPlanCallIndex = -1;
	std::vector<EnemySpawnerPreviewEntry> m_bossCallSummonPlanEntries;

	bool m_bBossSummonSequenceStarted = false;
	float m_bBossSummonCircleFadeAgeSec = 0.0f;
	CGameObject* m_pendingBossStageBoss = nullptr;

	bool m_bBossSummonVisualFadeOutStarted = false;
	float m_bBossSummonVisualFadeOutAgeSec = 0.0f;

	static constexpr float kBossShockwaveStartRadius = 3.0f;
	static constexpr float kBossShockwaveMaxRadius = 50.0f;
	static constexpr float kBossShockwaveExpandDurationSec = 0.80f;
	static constexpr float kBossShockwaveFadeDurationSec = 0.40f;

	static constexpr float kBossShockwaveShaderRingCenter = 0.94f;

	bool m_bBossShockwaveActive = false;
	float m_bossShockwaveAgeSec = 0.0f;

	static constexpr UINT  kBossShockwaveWallSegmentCount = 48;
	static constexpr float kBossShockwaveWallMaxHeight = 3.5f;
	static constexpr float kBossShockwaveWallMinWidth = 1.5f;
	static constexpr float kBossShockwaveWallWidthScale = 1.35f;

	XMFLOAT3 m_bossShockwaveCenter = XMFLOAT3(0.0f, 0.0f, 0.0f);

	static constexpr float kBossShockwavePlayerRangePadding = 1.25f;
	static constexpr float kBossShockwavePlayerMinDirectionDistance = 0.25f;

	bool m_bBossShockwavePushLocalPlayer = false;
	float m_bossShockwavePrevRadius = 0.0f;
	float m_bossShockwavePlayerInitialDistance = 0.0f;
	XMFLOAT3 m_bossShockwavePlayerPushDir = XMFLOAT3(0.0f, 0.0f, 1.0f);

	bool m_bBossShockwaveWindSfxTrackingActive = false;
	FMOD::Channel* m_bossShockwaveWindSfxChannel = nullptr;
	XMFLOAT3 m_bossShockwaveWindSfxDirection = XMFLOAT3(0.0f, 0.0f, 1.0f);
	XMFLOAT3 m_bossShockwaveWindSfxPrevPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
	bool m_bossShockwaveWindSfxHasPrevPosition = false;

	static constexpr float kBossDeathEffectDurationSec = 2.0f;
	static constexpr float kBossDeathCircleStartSec = 0.10f;
	static constexpr float kBossDeathCircleFadeInSec = 0.35f;
	static constexpr float kBossDeathCircleFadeOutStartSec = 1.45f;
	static constexpr float kBossDeathCircleFadeOutSec = 0.55f;
	static constexpr float kBossDeathLiftStartSec = 0.20f;
	static constexpr float kBossDeathLiftDurationSec = 0.65f;
	static constexpr float kBossDeathMaxLift = 0.45f;
	static constexpr float kBossDeathAbsorbStartSec = 0.35f;
	static constexpr float kBossDeathSmokeStartSec = 0.55f;
	static constexpr float kBossDeathRendererHideTimeSec = 1.00f;
	static constexpr float kBossDeathBurstTimeSec = 1.00f;
	static constexpr float kBossDeathGlowEmitIntervalSec = 0.055f;
	static constexpr float kBossDeathSmokeEmitIntervalSec = 0.070f;
	static constexpr float kBossDeathCircleSize = 26.0f;
	static constexpr float kBossDeathRingStartSize = 6.0f;
	static constexpr float kBossDeathRingEndSize = 42.0f;
	static constexpr float kBossDeathRingExpandSec = 0.45f;
	static constexpr float kBossDeathRingFadeSec = 0.55f;
	static constexpr int kBossDeathAbsorbParticlesPerEmit = 5;

	struct BossDeathEffectState
	{
		bool active = false;
		CGameObject* boss = nullptr;
		XMFLOAT3 originalPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
		XMFLOAT3 groundCenter = XMFLOAT3(0.0f, 0.0f, 0.0f);
		float ageSec = 0.0f;
		float glowEmitAccumulatorSec = 0.0f;
		float smokeEmitAccumulatorSec = 0.0f;
		bool rendererHidden = false;
		bool burstSpawned = false;
	};

	BossDeathEffectState m_bossDeathEffect;

	static constexpr UINT  kBossPoisonProjectileMaxCount = 8;

	static constexpr float kBossPoisonProjectileCoreDiameter = 4.0f;
	static constexpr float kBossPoisonProjectileCoreRadius = 2.0f;
	static constexpr float kBossPoisonProjectileGasDiameter = 8.0f;

	static constexpr float kBossPoisonProjectileLaunchDelaySec = 1.025f;
	static constexpr float kBossPoisonProjectileLaunchHeight = 3.3f;
	static constexpr float kBossPoisonProjectileSpeed = 18.0f;
	static constexpr float kBossPoisonProjectileForwardOffset = 4.0f;

	static constexpr int   kBossPoisonProjectileDamage = 50;

	static constexpr float kBossPoisonProjectilePlayerHitCenterYOffset = 1.0f;

	static constexpr float kBossPoisonProjectilePlayerCollisionRadius = 0.65f;

	static constexpr float kBossPoisonProjectilePlayerHalfHeight = 1.15f;

	static constexpr float kBossPoisonProjectileStageHalfExtent = 110.0f;

	static constexpr float kBossPoisonDustEmitIntervalSec = 0.20f;
	static constexpr UINT  kBossPoisonDustParticlesPerEmit = 5;

	static constexpr float kBossPoisonDustMinLifetimeSec = 1.05f;
	static constexpr float kBossPoisonDustMaxLifetimeSec = 1.55f;

	static constexpr float kBossPoisonDustMinSize = 1.60f;
	static constexpr float kBossPoisonDustMaxSize = 3.00f;

	static constexpr float kBossPoisonDustMinScatterSpeed = 0.75f;
	static constexpr float kBossPoisonDustMaxScatterSpeed = 2.25f;

	static constexpr float kBossPoisonDustProjectileVelocityInherit = 0.08f;

	static constexpr float kBossPoisonDustGravity = 1.20f;

	static constexpr float kBossPoisonDustDrag = 0.75f;

	static constexpr float kBossPoisonDustSpawnOffsetRadius = 0.35f;

	static constexpr float kBossMeleeSlashLaunchDelaySec = 0.430f;

	struct BossMeleeSlashCastState
	{
		bool wasMeleePhase = false;
		bool pendingSpawn = false;
		bool spawned = false;
		float meleeAgeSec = 0.0f;
	};

	std::unordered_map<CGameObject*, BossMeleeSlashCastState> m_bossMeleeSlashCastStates;

	
	struct BossStageBossPositionState
	{
		XMFLOAT3 originalPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);

		int restoreFramesRemaining = 0;
		bool pendingRestore = false;

		bool renderAllowed = false;
		bool waitAppearBeforeRender = false;
		bool appearPhaseSeen = false;
		bool appearFinished = false;
	};

	std::unordered_map<CGameObject*, BossStageBossPositionState> m_bossStageBossPositionStates;

	bool m_bSimulateLocalEnemySpawner = true;
	bool m_bSimulateLocalPlayerWorldStaticRollback = true;
	bool m_bSimulateLocalTeleport = true;
	bool m_bSimulateLocalItemPickup = true;
	bool m_bCanBossStageDirectly = false;

	bool m_bSimulateLocalStageTeleport = true;
	std::array<bool, CSceneGrid::kMegaGridCount + 1> m_bPrevLocalStageTeleportKeyDown = {};

	void ConfigureLocalGameplaySimulationSwitches();
};
