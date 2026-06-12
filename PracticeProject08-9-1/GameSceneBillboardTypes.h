//-----------------------------------------------------------------------------
// File: GameSceneBillboardTypes.h
//-----------------------------------------------------------------------------

#pragma once

#include "SceneRenderTypes.h"
#include "GameSceneFrameUploadBuffer.h"

#include <DirectXMath.h>

#include <array>
#include <cstddef>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

using DirectX::XMFLOAT2;
using DirectX::XMFLOAT3;
using DirectX::XMFLOAT4;

class CGameObject;
class CTexture;
class CMesh;

class CItemBillboardShader;
class CTransparentItemBillboardShader;
class CMuzzleFlashBillboardShader;
class CBossPoisonProjectileBillboardShader;
class CSwordTrailShader;
class CGunSmokeBillboardShader;

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
	PoisonDust = 4,
	BossMeleeSlash = 5,
	MagicCircleGlow = 6,
	MagicCircleAfterimage = 7,
	GoldFirework = 8,
	GunSmoke = 9,
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

	// x = kind
	// 0=core, 1=ring, 2=spark, 3=blood, 4=poison dust,
	// 5=boss melee slash, 6=magic circle glow, 7=magic circle afterimage,
	// 8=gold firework, 9=gun smoke
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

	XMFLOAT3 axisRight = XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMFLOAT3 axisUp = XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMFLOAT3 axisForward = XMFLOAT3(0.0f, 0.0f, 1.0f);

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

struct MuzzleFlashEffectState
{
	std::shared_ptr<CMuzzleFlashBillboardShader> shader;
	std::vector<MuzzleFlashEntry> entries;
	FrameUploadVertexBuffer<MuzzleFlashInstanceVertex, kSceneBatchFrameResourceCount> instanceBuffer;
};

struct GunSmokeEffectState
{
	std::shared_ptr<CGunSmokeBillboardShader> shader;
	std::vector<MuzzleFlashEntry> entries;
	FrameUploadVertexBuffer<MuzzleFlashInstanceVertex, kSceneBatchFrameResourceCount> instanceBuffer;
};

struct BossPoisonProjectileEntry
{
	bool active = false;

	CGameObject* owner = nullptr;

	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	XMFLOAT3 velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float coreDiameter = 4.0f;
	float coreRadius = 2.0f;
	float gasDiameter = 6.0f;
	float speed = 18.0f;
	float visualSeed = 0.0f;
	float dustEmitAccumulatorSec = 0.0f;

	std::array<bool, 4> hitPlayerSlots = { false, false, false, false };
};

struct BossPoisonSpellCastState
{
	bool wasSpellPhase = false;
	bool pendingFire = false;
	bool fired = false;
	float spellAgeSec = 0.0f;
};

struct BossPoisonProjectileEffectState
{
	std::shared_ptr<CBossPoisonProjectileBillboardShader> shader;
	std::vector<BossPoisonProjectileEntry> entries;
	std::unordered_map<CGameObject*, BossPoisonSpellCastState> spellCastStates;
	FrameUploadVertexBuffer<MuzzleFlashInstanceVertex, kSceneBatchFrameResourceCount> instanceBuffer;
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

	CGameObject* weaponObject = nullptr;

	float age = 0.0f;

	float startDelay = 0.340f;
	float sampleDuration = 0.240f;
	float fadeDuration = 0.120f;

	// 무기 local space에서 trail ribbon의 양 끝점.
	XMFLOAT3 rootLocal = XMFLOAT3(0.0f, 0.0f, 0.10f);
	XMFLOAT3 tipLocal = XMFLOAT3(0.0f, 0.0f, 1.45f);

	// root/tip 사이 폭 조절.
	float widthScale = 1.0f;

	XMFLOAT4 color = XMFLOAT4(0.55f, 0.80f, 1.0f, 1.0f);

	float alphaScale = 0.75f;

	std::vector<SwordTrailSample> samples;
};

struct SwordTrailEffectState
{
	std::shared_ptr<CSwordTrailShader> shader;
	std::vector<SwordTrailEntry> entries;
	FrameUploadVertexBuffer<SwordTrailVertex, kSceneBatchFrameResourceCount> vertexBuffer;
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

	float startDelay = 0.340f;
	float sampleDuration = 0.240f;
	float fadeDuration = 0.120f;

	XMFLOAT3 rootLocal = XMFLOAT3(0.0f, 0.0f, 0.15f);
	XMFLOAT3 tipLocal = XMFLOAT3(0.0f, 0.0f, 2.175f);

	float widthScale = 1.0f;

	XMFLOAT4 color = XMFLOAT4(0.55f, 0.80f, 1.0f, 1.0f);

	std::vector<MonsterSwordTrailSample> samples;
};

struct MonsterSwordTrailEffectState
{
	std::shared_ptr<CSwordTrailShader> shader;
	std::vector<MonsterSwordTrailEntry> entries;
	FrameUploadVertexBuffer<MonsterSwordTrailVertex, kSceneBatchFrameResourceCount> vertexBuffer;
};

struct ArrowTrailSample
{
	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float age = 0.0f;
};

struct ArrowTrailEntry
{
	float sampleLifetimeSec = 0.260f;
	float halfWidth = 0.075f;
	XMFLOAT4 color = XMFLOAT4(0.86f, 0.94f, 1.0f, 1.0f);
	float tailAlpha = 0.20f;
	float headAlpha = 0.80f;
	float alphaScale = 0.72f;

	bool active = false;

	CGameObject* arrowObject = nullptr;

	std::deque<ArrowTrailSample> samples;
};

struct ArrowTrailEffectState
{
	std::shared_ptr<CSwordTrailShader> shader;
	std::vector<ArrowTrailEntry> entries;
	FrameUploadVertexBuffer<SwordTrailVertex, kSceneBatchFrameResourceCount> vertexBuffer;
};

static constexpr UINT kBossCallSummonWwwPeakCount = 16;

struct BossCallSummonWwwEntry
{
	bool active = false;

	XMFLOAT3 center = XMFLOAT3(0.0f, 0.0f, 0.0f);

	// 마법진 정사각형 한 변이 지름이므로 radius = circleSize * 0.5f.
	float radius = 1.0f;

	// 요구사항상 WWW 높이는 마법진 반지름 길이.
	float maxHeight = 1.0f;

	float age = 0.0f;
	float lifetime = 1.10f;

	float retargetTimer = 0.0f;

	std::array<float, kBossCallSummonWwwPeakCount> peakHeights = {};
	std::array<float, kBossCallSummonWwwPeakCount> targetPeakHeights = {};
	std::array<float, kBossCallSummonWwwPeakCount> peakMoveSpeeds = {};

	float seed = 0.0f;

	XMFLOAT4 color = XMFLOAT4(0.10f, 0.90f, 0.18f, 0.75f);
};

struct BossCallSummonWwwEffectState
{
	std::shared_ptr<CSwordTrailShader> shader;
	std::vector<BossCallSummonWwwEntry> entries;
	FrameUploadVertexBuffer<SwordTrailVertex, kSceneBatchFrameResourceCount> vertexBuffer;
};

enum class EItemBillboardKind : UINT
{
	Key = 0,
	BossSummonCircle = 1,
	BossSummonGlow = 2,
	BossShockwave = 3,
	BossShockwaveWall = 4,
	BossCallSummonCircle = 5,
	HealPotion = 6,
	AttackPowerPotion = 7,
	DefensePotion = 8,
	MoveSpeedPotion = 9,
	BossDeathCircle = 10,
	BossDeathRing = 11,
};

struct ItemBillboardEntry
{
	bool active = true;
	bool distanceCulled = false;

	bool transparent = false;

	EItemBillboardKind kind = EItemBillboardKind::Key;

	uint64_t serverId = 0;

	// 1~9. 유효하지 않으면 -1.
	int megaGridNumber = -1;

	// 인벤토리 슬롯 0~3. 아이템 획득 대상이 아니면 -1.
	int inventorySlot = -1;

	XMFLOAT3 position = XMFLOAT3(0.0f, 0.0f, 0.0f);

	float width = 1.0f;
	float height = 1.0f;
	float yOffset = 1.0f;

	float cullDistance = 35.0f;

	float pickupRadius = 1.25f;
	float pickupHeightTolerance = 2.0f;

	UINT materialId = 0;
};

struct BossCallSummonCircleVisualState
{
	bool active = false;
	bool fadingIn = false;
	bool fadingOut = false;

	float ageSec = 0.0f;
	float durationSec = 0.0f;
	float alpha = 0.0f;
};

struct ItemBillboardState
{
	std::shared_ptr<CTexture> keyTexture;
	std::array<std::shared_ptr<CTexture>, 4> potionTextures = {};
	std::shared_ptr<CTexture> bossSummonCircleTexture;

	std::shared_ptr<CItemBillboardShader> shader;
	std::shared_ptr<CTransparentItemBillboardShader> transparentShader;

	std::shared_ptr<CMesh> quadMesh;

	std::vector<ItemBillboardEntry> entries;

	FrameUploadVertexBuffer<ItemBillboardInstanceVertex, kSceneBatchFrameResourceCount> instanceBuffer;
	FrameUploadVertexBuffer<ItemBillboardInstanceVertex, kSceneBatchFrameResourceCount> transparentInstanceBuffer;

	BossCallSummonCircleVisualState bossCallSummonCircleVisual;
	std::vector<size_t> activeBossCallSummonCircleItemIndices;

	float bossSummonGlowParticleEmitAccumulatorSec = 0.0f;
	float bossCallSummonGlowParticleEmitAccumulatorSec = 0.0f;
};

struct MonsterHpGaugeState
{
	std::shared_ptr<CTexture> hpTexture;
	std::shared_ptr<CTexture> emptyHpTexture;
	std::array<std::shared_ptr<CTexture>, 4> playerNameTextures;
	std::shared_ptr<CItemBillboardShader> shader;
	std::shared_ptr<CMesh> quadMesh;
	FrameUploadVertexBuffer<ItemBillboardInstanceVertex, kSceneBatchFrameResourceCount> instanceBuffer;
};