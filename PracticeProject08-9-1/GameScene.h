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
#include "ShadowMap.h"

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
class EnemySpawner;
class CSkinnedMeshRendererComponent;
class CSkinningComponent;
class CAnimatorComponent;
class CHealthComponent;
class CActorTagComponent;

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

struct ItemBillboardInstanceVertex
{
	XMFLOAT4 world0 = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
	XMFLOAT4 world1 = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	XMFLOAT4 world2 = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
	XMFLOAT4 world3 = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	UINT materialId = 0;
	UINT pad[3] = { 0, 0, 0 };
};

enum class EMuzzleFlashKind : UINT
{
	Core = 0,
	Ring = 1,
	Spark = 2,
	Blood = 3,
};

struct MuzzleFlashInstanceVertex
{
	XMFLOAT4 world0 = XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
	XMFLOAT4 world1 = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	XMFLOAT4 world2 = XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);
	XMFLOAT4 world3 = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	// rgb = 색상, a = 전체 alpha 계수
	XMFLOAT4 color = XMFLOAT4(1.0f, 0.65f, 0.12f, 1.0f);

	// x = ageRatio, y = intensity, z = rotationRad, w = seed
	XMFLOAT4 params0 = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);

	// x = kind (0=core, 1=ring, 2=spark)
	// y = reserved
	// z = reserved
	// w = reserved
	XMFLOAT4 params1 = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
};

struct MuzzleFlashEntry
{
	bool active = false;
	EMuzzleFlashKind kind = EMuzzleFlashKind::Core;

	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float age = 0.0f;
	float lifetime = 0.08f;

	float startWidth = 0.65f;
	float startHeight = 0.65f;

	float endWidth = 1.25f;
	float endHeight = 1.25f;

	float rotationRad = 0.0f;
	float intensity = 1.0f;
	float drag = 0.0f;
	float gravity = 0.0f;
	float seed = 0.0f;

	XMFLOAT4 color = XMFLOAT4(1.0f, 0.62f, 0.10f, 1.0f);
};

struct SwordTrailVertex
{
	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT2 uv = XMFLOAT2(0.0f, 0.0f);
	XMFLOAT4 color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
};

struct SwordTrailSample
{
	XMFLOAT3 root = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 tip = XMFLOAT3(0.0f, 0.0f, 0.0f);
};

enum class EWeaponTrailKind : UINT
{
	Sword = 0,
	Axe = 1,
};

struct SwordTrailEntry
{
	bool active = false;

	EWeaponTrailKind kind = EWeaponTrailKind::Sword;

	CGameObject* owner = nullptr;

	// 이름은 SwordTrailEntry지만, 실제로는 검/도끼 공용 weapon trail로 사용한다.
	CGameObject* weaponObject = nullptr;

	float age = 0.0f;

	float startDelay = 0.340f;
	float sampleDuration = 0.240f;
	float fadeDuration = 0.120f;

	// 무기 local space에서 trail ribbon의 양 끝점.
	// sword: 손잡이 쪽 ~ 칼끝
	// axe: 도끼날 근처 짧은 구간
	XMFLOAT3 rootLocal = XMFLOAT3(0.0f, 0.0f, 0.10f);
	XMFLOAT3 tipLocal = XMFLOAT3(0.0f, 0.0f, 1.45f);

	// root/tip 사이 폭 조절.
	// 도끼는 날이 끝자락에만 있으므로 이 값을 줄이면 된다.
	float widthScale = 1.0f;

	XMFLOAT4 color = XMFLOAT4(0.55f, 0.80f, 1.0f, 1.0f);

	std::vector<SwordTrailSample> samples;
};

struct MonsterSwordTrailVertex
{
	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT2 uv = XMFLOAT2(0.0f, 0.0f);
	XMFLOAT4 color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
};

struct MonsterSwordTrailSample
{
	XMFLOAT3 root = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 tip = XMFLOAT3(0.0f, 0.0f, 0.0f);
};

struct MonsterSwordTrailEntry
{
	bool active = false;

	CGameObject* owner = nullptr;
	CGameObject* weaponObject = nullptr;

	float age = 0.0f;

	// SwordMan은 플레이어 sword attack과 애니메이션 타이밍이 동일하다.
	float startDelay = 0.340f;
	float sampleDuration = 0.240f;
	float fadeDuration = 0.120f;

	// SwordMan 에셋/메시가 플레이어 대비 1.5배이므로 local trail 구간도 1.5배.
	XMFLOAT3 rootLocal = XMFLOAT3(0.0f, 0.0f, 0.15f);
	XMFLOAT3 tipLocal = XMFLOAT3(0.0f, 0.0f, 2.175f);

	float widthScale = 1.0f;

	XMFLOAT4 color = XMFLOAT4(0.55f, 0.80f, 1.0f, 1.0f);

	std::vector<MonsterSwordTrailSample> samples;
};

enum class EItemBillboardKind : UINT
{
	Key = 0,
	BossSummonCircle = 1,
	BossSummonGlow = 2,
	BossShockwave = 3,
	BossShockwaveWall = 4,
};

struct ItemBillboardEntry
{
	bool active = true;
	bool distanceCulled = false;

	// 추가: true면 transparent pass에서 그림
	bool transparent = false;

	EItemBillboardKind kind = EItemBillboardKind::Key;

	// 1~9. 유효하지 않으면 -1.
	int megaGridNumber = -1;

	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float width = 1.0f;
	float height = 1.0f;
	float yOffset = 1.0f;

	float cullDistance = 35.0f;

	float pickupRadius = 1.25f;
	float pickupHeightTolerance = 2.0f;

	UINT materialId = 0;
};

struct StaticInstanceGroup
{
	std::shared_ptr<CMesh> mesh;
	UINT subMeshIndex = 0;
	std::vector<UINT> objectIndices;

	UINT instanceBufferStart = 0;
	bool useTreeShader = false;

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

	std::shared_ptr<CTexture> m_keyItemTexture;
	std::shared_ptr<CTexture> m_bossSummonCircleTexture;
	void BuildItemBillboardBatch(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		UINT rtCount,
		DXGI_FORMAT* rtvFormats,
		DXGI_FORMAT dsvFormat
	);

	void ReleaseItemBillboardGpuResources();

	void UpdateItemBillboardDistanceCullSelection(CCamera* camera);
	void RenderItemBillboards(ID3D12GraphicsCommandList* cmd, CCamera* camera);
	void RenderTransparentItemBillboards(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	void BuildMuzzleFlashBatch(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		DXGI_FORMAT dsvFormat
	);

	void ReleaseMuzzleFlashGpuResources();

	void SpawnMuzzleFlash(const XMFLOAT3& position, const XMFLOAT3& direction);
	void SpawnBloodSplash(
		CGameObject* victim,
		const XMFLOAT3* hitPosition = nullptr,
		const XMFLOAT3* hitDirection = nullptr
	);
	void UpdateMuzzleFlashes(float dt);
	void RenderMuzzleFlashes(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	void BuildSwordTrailBatch(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		DXGI_FORMAT dsvFormat
	);

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

	void UpdateItemBillboardPickupCollision();
	bool DoesPlayerOverlapItemBillboard(const CGameObject* player, const ItemBillboardEntry& item) const;

	void SetBossSummonCircleAlpha(float alpha);
	void SetBossSummonGlowAlpha(float alpha);
	void SetBossSummonVisualAlpha(float alpha);
	void SetBossSummonVisualActive(bool active);

	void SpawnBossSummonCircle(const XMFLOAT3& center, float alpha);
	void SpawnBossSummonGlow(const XMFLOAT3& center, float alpha);
	void SpawnBossSummonVisuals(const XMFLOAT3& center, float alpha);

	void SetBossShockwaveAlpha(float alpha);
	void SpawnBossShockwave(const XMFLOAT3& center);
	void UpdateBossShockwave(float dt);
	void SetBossShockwaveWallAlpha(float alpha);

	void ApplyBossShockwavePushToLocalPlayer(
		float previousRadius,
		float currentRadius
	);

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
	bool IsLocalPlayerDead() const { return m_bLocalPlayerDead; }
	bool RollbackLocalPlayerMoveIfCollidingWorldStatic(const XMFLOAT3& previousPos);
    
	void RequestFireArrow(CGameObject* shooter, float speed, float lifeSec = 3.0f, float yOffset = 0.0f);
	bool IsLocalPlayerInsideMegaGridCenter() const;
	bool IsLocalMonsterChaseEnabled() const { return m_bSimulateLocalMonsterChase; }

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

	bool TryTeleportLocalPlayerToMegaGridByNumber(int megaGridNumber);
	XMFLOAT3 ComputeLocalStageTeleportPosition(int megaGridNumber) const;

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

	bool IsStaticObjectInsideShadowBox(UINT objectIndex) const;
	bool IsSkinnedObjectInsideShadowBox(UINT objectIndex) const;

	void RenderShadowMap(ID3D12GraphicsCommandList* cmd);
	void RenderStaticInstanceGroupsToShadowMap(ID3D12GraphicsCommandList* cmd);
	void RenderSkinnedInstanceGroupsToShadowMap(ID3D12GraphicsCommandList* cmd);
	void RestoreSceneRenderTargets(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	int GetLocalPlayerMegaGridNumberForDepthFog() const;
	void UpdateDepthFogState(float dt);

    // slot 0..3 플레이어 포인터(소유는 m_skinnedObjects가 함)
    std::array<CGameObject*, 4> m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };

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
	UINT m_EnemySpawnCount = 0;

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

	static constexpr UINT kKeyItemBillboardCount = 7;

	std::shared_ptr<CItemBillboardShader> m_itemBillboardShader;
	std::shared_ptr<CTransparentItemBillboardShader> m_transparentItemBillboardShader;

	std::shared_ptr<CMesh>                m_itemBillboardQuadMesh;

	std::vector<ItemBillboardEntry>       m_itemBillboards;

	std::array<ComPtr<ID3D12Resource>, kSceneBatchFrameResourceCount> m_pd3dItemBillboardInstanceBuffer;
	std::array<ItemBillboardInstanceVertex*, kSceneBatchFrameResourceCount> m_pMappedItemBillboardInstanceBuffer = {};
	UINT                                  m_itemBillboardInstanceBufferCapacity = 0;

	std::array<ComPtr<ID3D12Resource>, kSceneBatchFrameResourceCount> m_pd3dTransparentItemBillboardInstanceBuffer;
	std::array<ItemBillboardInstanceVertex*, kSceneBatchFrameResourceCount> m_pMappedTransparentItemBillboardInstanceBuffer = {};
	UINT                                  m_transparentItemBillboardInstanceBufferCapacity = 0;

	static constexpr UINT kMuzzleFlashMaxCount = 160;

	std::shared_ptr<CMuzzleFlashBillboardShader> m_muzzleFlashShader;

	std::vector<MuzzleFlashEntry> m_muzzleFlashes;

	std::array<ComPtr<ID3D12Resource>, kSceneBatchFrameResourceCount> m_pd3dMuzzleFlashInstanceBuffer;
	std::array<MuzzleFlashInstanceVertex*, kSceneBatchFrameResourceCount> m_pMappedMuzzleFlashInstanceBuffer = {};
	UINT m_muzzleFlashInstanceBufferCapacity = 0;

	static constexpr UINT kSwordTrailMaxCount = 16;
	static constexpr UINT kSwordTrailMaxSamples = 12;
	static constexpr UINT kSwordTrailMaxVertices =
		kSwordTrailMaxCount * kSwordTrailMaxSamples * 2;

	std::shared_ptr<CSwordTrailShader> m_swordTrailShader;

	std::vector<SwordTrailEntry> m_swordTrails;

	std::array<ComPtr<ID3D12Resource>, kSceneBatchFrameResourceCount> m_pd3dSwordTrailVertexBuffer;
	std::array<SwordTrailVertex*, kSceneBatchFrameResourceCount> m_pMappedSwordTrailVertexBuffer = {};
	UINT m_swordTrailVertexBufferCapacity = 0;

	static constexpr UINT kMonsterSwordTrailMaxCount = 32;
	static constexpr UINT kMonsterSwordTrailMaxSamples = 12;
	static constexpr UINT kMonsterSwordTrailMaxVertices =
		kMonsterSwordTrailMaxCount * kMonsterSwordTrailMaxSamples * 2;

	std::shared_ptr<CSwordTrailShader> m_monsterSwordTrailShader;

	std::vector<MonsterSwordTrailEntry> m_monsterSwordTrails;

	std::array<ComPtr<ID3D12Resource>, kSceneBatchFrameResourceCount> m_pd3dMonsterSwordTrailVertexBuffer;
	std::array<MonsterSwordTrailVertex*, kSceneBatchFrameResourceCount> m_pMappedMonsterSwordTrailVertexBuffer = {};
	UINT m_monsterSwordTrailVertexBufferCapacity = 0;

	std::vector<CGameObject*> m_ghoulRefs;
	std::vector<CGameObject*> m_swordManRefs;
	std::vector<CGameObject*> m_bowManRefs;
	std::vector<CGameObject*> m_MutantRefs;
	std::vector<CGameObject*> m_bossRefs;

	// 6, 8번 메가그리드에서 열쇠를 해금하는 첫 Mutant.
	// key = mutant object, value = megaGridNumber.
	std::unordered_map<CGameObject*, int> m_mutantKeyTriggerMegaByObject;
	std::array<bool, CSceneGrid::kMegaGridCount + 1> m_mutantKeyTriggerRegisteredByMega = {};

    std::vector<CGameObject*> m_helmetRefs;

    std::vector<CGameObject*> m_PlayerSwordRefs;
    std::vector<CGameObject*> m_PlayerBowRefs;
    std::vector<CGameObject*> m_PlayerAxeRefs;
    std::vector<CGameObject*> m_PlayerGunRefs;

    std::vector<CGameObject*> m_EnemySwordRefs;
    std::vector<CGameObject*> m_EnemyBowRefs;
	std::vector<CGameObject*> m_EnemySpawnRefs;

    std::vector<AttachmentBindSpec> m_attachmentBinds;

	static constexpr UINT kArrowPoolSize = 32;
	static constexpr UINT kBulletPoolSize = 32;
	std::vector<CGameObject*> m_arrowRefs;
	std::vector<CGameObject*> m_bulletRefs;

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
		BowRelease
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

	UINT m_nFrameResourceIndex = 0;

	CDepthFogSystem                 m_depthFog;
	float                           m_fElapsedTime = 0.0f;

	bool                            m_bLocalPlayerDead = false;
	bool                            m_bLocalPlayerRespawnUsed = false;
	float                           m_localPlayerRespawnTimer = 0.0f;

#ifdef USING_NETWORK
	std::deque<FrameSnapshot> m_frameSnapshotBuffer;
	uint64_t m_lastReceivedServerTick = 0;

	static constexpr uint64_t kNetworkInterpolationDelayTicks = 2;
	static constexpr size_t kMaxNetworkFrameSnapshotBufferSize = 8;

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

	std::unordered_map<uint64_t, uint32_t> m_prevPlayerNetworkStateCode;
#endif

	std::unordered_set<CGameObject*> m_deadMonsters;

    unique_ptr<CCollisionSystem> m_Collision;
	std::unique_ptr<CNavMesh> m_navMesh;

	std::unique_ptr<EnemySpawner> m_enemySpawner;
	float m_enemySpawnAccumulatorSec = 0.0f;
	float m_enemySpawnIntervalSec = 5.0f;

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
	std::shared_ptr<CShadowMapSkinnedShader>              m_shadowSkinnedShader;
	std::shared_ptr<CShadowMapAlphaClipSkinnedShader>     m_shadowAlphaClipSkinnedShader;

	CGameSceneHUD                       m_hud;
	CShadowMapSystem					m_shadowMap;

	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 8> m_sceneRtvHandles = {};
	D3D12_CPU_DESCRIPTOR_HANDLE                m_sceneDsvHandle = {};
	UINT                                       m_sceneRenderTargetCount = 0;
	bool                                       m_bSceneRenderTargetsReady = false;

	bool                                m_bInactiveOverlayVisible = false;
	bool                                m_bStartedGameplayMusic = false;
	bool                                m_bWasLocalPlayerInsideMegaGridCenter = false;
	bool                                m_bShowShadowMapOverlay = true;

	bool m_bLocalPlayerInsideCastleCenterMegaGrid = false;

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

	// objectIndex -> bone palette 시작 offset
	std::vector<UINT>                   m_skinnedBonePaletteBaseByObject;

	// objectIndex -> 이 object에 예약된 bone matrix 개수
	std::vector<UINT>                   m_skinnedBonePaletteCountByObject;

	UINT                                m_skinnedBonePaletteCapacity = 0;
	
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

	bool m_bSimulateLocalMonsterChase = true;
	bool m_bPrevLocalMonsterChaseToggleKeyDown = false;
	bool m_bPrevDebugDamageMegaGrid5KeyDown = false;
	bool m_bBossStageBossActivated = false;

	static constexpr float kBossStageBossHiddenYOffset = -100.0f;
	static constexpr float kBossSummonCircleFadeInDurationSec = 3.0f;
	static constexpr float kBossSummonCircleFadeOutDurationSec = 1.0f;

	bool m_bBossSummonSequenceStarted = false;
	float m_bBossSummonCircleFadeAgeSec = 0.0f;
	CGameObject* m_pendingBossStageBoss = nullptr;

	bool m_bBossSummonVisualFadeOutStarted = false;
	float m_bBossSummonVisualFadeOutAgeSec = 0.0f;

	static constexpr float kBossShockwaveStartRadius = 3.0f;
	static constexpr float kBossShockwaveMaxRadius = 50.0f;
	static constexpr float kBossShockwaveExpandDurationSec = 0.80f;
	static constexpr float kBossShockwaveFadeDurationSec = 0.40f;

	// HLSL에서 충격파 ring 중심이 uv 반지름 약 0.94 지점에 있으므로,
	// CPU billboard 크기 보정에 사용한다.
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

	struct BossStageBossPositionState
	{
		XMFLOAT3 originalPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);

		int restoreFramesRemaining = 0;
		bool pendingRestore = false;

		// 보스 등장 직후 Appear action이 실제로 시작되기 전까지
		// skinned render/shadow render에서 아예 제외하기 위한 게이트.
		bool renderAllowed = false;
		bool waitAppearBeforeRender = false;
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
