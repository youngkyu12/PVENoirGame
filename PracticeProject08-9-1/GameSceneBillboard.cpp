//-----------------------------------------------------------------------------
// File: GameSceneBillboard.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"
#include "GameSceneBillboardCommon.h"

using namespace GameSceneBillboardCommon;

namespace
{
	static MuzzleFlashEntry* AcquireFreeMuzzleFlashEntry(
		std::vector<MuzzleFlashEntry>& flashes)
	{
		for ( MuzzleFlashEntry& flash : flashes )
		{
			if ( !flash.active )
				return &flash;
		}

		return nullptr;
	}

	static XMFLOAT3 TransformLocalPoint(
	const XMFLOAT4X4& world,
	const XMFLOAT3& localPoint)
	{
		XMVECTOR p = XMLoadFloat3(&localPoint);
		XMMATRIX W = XMLoadFloat4x4(&world);

		XMVECTOR out = XMVector3TransformCoord(p, W);

		XMFLOAT3 result{};
		XMStoreFloat3(&result, out);
		return result;
	}

	static void ComputeWeaponTrailRootTip(
	const SwordTrailEntry& trail,
	XMFLOAT3& outRoot,
	XMFLOAT3& outTip)
	{
		if ( !trail.weaponObject )
		{
			outRoot = XMFLOAT3(0.0f, 0.0f, 0.0f);
			outTip = XMFLOAT3(0.0f, 0.0f, 0.0f);
			return;
		}

		const XMFLOAT4X4& W = trail.weaponObject->GetWorldMatrix();

		XMFLOAT3 root = TransformLocalPoint(W, trail.rootLocal);
		XMFLOAT3 tip = TransformLocalPoint(W, trail.tipLocal);

		if ( trail.widthScale != 1.0f )
		{
			XMVECTOR rootV = XMLoadFloat3(&root);
			XMVECTOR tipV = XMLoadFloat3(&tip);

			XMVECTOR center = XMVectorScale(rootV + tipV, 0.5f);
			XMVECTOR half = XMVectorScale(tipV - rootV, 0.5f * trail.widthScale);

			rootV = center - half;
			tipV = center + half;

			XMStoreFloat3(&root, rootV);
			XMStoreFloat3(&tip, tipV);
		}

		outRoot = root;
		outTip = tip;
	}

	static SwordTrailEntry* AcquireFreeSwordTrailEntry(
		std::vector<SwordTrailEntry>& trails)
	{
		for ( SwordTrailEntry& trail : trails )
		{
			if ( !trail.active )
				return &trail;
		}

		return nullptr;
	}

	static bool AppendSwordTrailSample(
	SwordTrailEntry& trail,
	UINT maxSamples)
	{
		if ( !trail.weaponObject )
			return false;

		SwordTrailSample sample{};
		ComputeWeaponTrailRootTip(
			trail,
			sample.root,
			sample.tip
		);

		if ( !trail.samples.empty() )
		{
			const SwordTrailSample& last = trail.samples.back();

			const float dx = sample.tip.x - last.tip.x;
			const float dy = sample.tip.y - last.tip.y;
			const float dz = sample.tip.z - last.tip.z;

			const float movedSq = dx * dx + dy * dy + dz * dz;

			if ( movedSq < 0.0004f )
				return true;
		}

		trail.samples.push_back(sample);

		while ( trail.samples.size() > maxSamples )
			trail.samples.erase(trail.samples.begin());

		return true;
	}

	static void SetPlayerMeleeWeaponHitboxActive(CGameObject* weaponObject, bool active)
	{
		if ( !weaponObject )
			return;

		if ( auto* hitbox = weaponObject->GetComponent<CWeaponHitboxComponent>() )
		{
			hitbox->SetHitboxActive(active);
			return;
		}

		if ( auto* collider = weaponObject->GetComponent<CColliderComponent>() )
			collider->SetCollisionEnabled(false);
	}

	static void ResetPlayerMeleeWeaponHitbox(CGameObject* weaponObject)
	{
		if ( !weaponObject )
			return;

		if ( auto* hitbox = weaponObject->GetComponent<CWeaponHitboxComponent>() )
		{
			hitbox->SetHitboxActive(false);
			hitbox->ClearHitTargets();
			return;
		}

		if ( auto* collider = weaponObject->GetComponent<CColliderComponent>() )
			collider->SetCollisionEnabled(false);
	}

	static void FinishSwordTrailEntry(SwordTrailEntry& trail)
	{
		SetPlayerMeleeWeaponHitboxActive(trail.weaponObject, false);

		trail.active = false;
		trail.owner = nullptr;
		trail.weaponObject = nullptr;
		trail.age = 0.0f;
		trail.samples.clear();
	}

	static XMFLOAT3 TransformMonsterSwordTrailLocalPoint(
	const XMFLOAT4X4& world,
	const XMFLOAT3& localPoint)
	{
		XMVECTOR p = XMLoadFloat3(&localPoint);
		XMMATRIX W = XMLoadFloat4x4(&world);

		XMVECTOR out = XMVector3TransformCoord(p, W);

		XMFLOAT3 result{};
		XMStoreFloat3(&result, out);
		return result;
	}

	static void ComputeMonsterSwordTrailRootTip(
		const MonsterSwordTrailEntry& trail,
		XMFLOAT3& outRoot,
		XMFLOAT3& outTip)
	{
		if ( !trail.weaponObject )
		{
			outRoot = XMFLOAT3(0.0f, 0.0f, 0.0f);
			outTip = XMFLOAT3(0.0f, 0.0f, 0.0f);
			return;
		}

		const XMFLOAT4X4& W = trail.weaponObject->GetWorldMatrix();

		XMFLOAT3 root = TransformMonsterSwordTrailLocalPoint(W, trail.rootLocal);
		XMFLOAT3 tip = TransformMonsterSwordTrailLocalPoint(W, trail.tipLocal);

		if ( trail.widthScale != 1.0f )
		{
			XMVECTOR rootV = XMLoadFloat3(&root);
			XMVECTOR tipV = XMLoadFloat3(&tip);

			XMVECTOR center = XMVectorScale(rootV + tipV, 0.5f);
			XMVECTOR half = XMVectorScale(tipV - rootV, 0.5f * trail.widthScale);

			rootV = center - half;
			tipV = center + half;

			XMStoreFloat3(&root, rootV);
			XMStoreFloat3(&tip, tipV);
		}

		outRoot = root;
		outTip = tip;
	}

	static MonsterSwordTrailEntry* AcquireFreeMonsterSwordTrailEntry(
		std::vector<MonsterSwordTrailEntry>& trails)
	{
		for ( MonsterSwordTrailEntry& trail : trails )
		{
			if ( !trail.active )
				return &trail;
		}

		return nullptr;
	}

	static bool AppendMonsterSwordTrailSample(
		MonsterSwordTrailEntry& trail,
		UINT maxSamples)
	{
		if ( !trail.weaponObject )
			return false;

		MonsterSwordTrailSample sample{};
		ComputeMonsterSwordTrailRootTip(
			trail,
			sample.root,
			sample.tip
		);

		if ( !trail.samples.empty() )
		{
			const MonsterSwordTrailSample& last = trail.samples.back();

			const float dx = sample.tip.x - last.tip.x;
			const float dy = sample.tip.y - last.tip.y;
			const float dz = sample.tip.z - last.tip.z;

			const float movedSq = dx * dx + dy * dy + dz * dz;

			if ( movedSq < 0.0004f )
				return true;
		}

		trail.samples.push_back(sample);

		while ( trail.samples.size() > maxSamples )
			trail.samples.erase(trail.samples.begin());

		return true;
	}

	static void FinishMonsterSwordTrailEntry(MonsterSwordTrailEntry& trail)
	{
		trail.active = false;
		trail.owner = nullptr;
		trail.weaponObject = nullptr;
		trail.age = 0.0f;
		trail.samples.clear();
	}

	static ArrowTrailEntry* FindArrowTrailEntry(
	std::vector<ArrowTrailEntry>& trails,
	CGameObject* arrowObject)
	{
		if ( !arrowObject )
			return nullptr;

		for ( ArrowTrailEntry& trail : trails )
		{
			if ( !trail.active )
				continue;

			if ( trail.arrowObject == arrowObject )
				return &trail;
		}

		return nullptr;
	}

	static ArrowTrailEntry* AcquireFreeArrowTrailEntry(
		std::vector<ArrowTrailEntry>& trails)
	{
		for ( ArrowTrailEntry& trail : trails )
		{
			if ( !trail.active )
				return &trail;
		}

		// 전부 사용 중이면 이미 화살 연결이 끊겨 fade-out만 남은 엔트리를 재사용한다.
		for ( ArrowTrailEntry& trail : trails )
		{
			if ( trail.arrowObject == nullptr )
			{
				trail.samples.clear();
				trail.active = false;
				return &trail;
			}
		}

		return nullptr;
	}

	static void ResetArrowTrailEntry(ArrowTrailEntry& trail)
	{
		trail.active = false;
		trail.arrowObject = nullptr;
		trail.samples.clear();
	}

	static void AppendArrowTrailSample(
		ArrowTrailEntry& trail,
		const XMFLOAT3& position,
		UINT maxSamples)
	{
		constexpr float kMinArrowTrailSampleDistanceSq = 0.010f; // 약 10cm

		if ( !trail.samples.empty() )
		{
			const ArrowTrailSample& last = trail.samples.back();

			const float dx = position.x - last.position.x;
			const float dy = position.y - last.position.y;
			const float dz = position.z - last.position.z;

			const float movedSq = dx * dx + dy * dy + dz * dz;

			if ( movedSq < kMinArrowTrailSampleDistanceSq )
				return;
		}

		ArrowTrailSample sample{};
		sample.position = position;
		sample.age = 0.0f;

		trail.samples.push_back(sample);

		while ( trail.samples.size() > maxSamples )
			trail.samples.pop_front();
	}
}

void CGameScene::BuildBossPoisonProjectileBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_bossPoisonProjectileEffect.shader = std::make_shared<CBossPoisonProjectileBillboardShader>();

	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_bossPoisonProjectileEffect.shader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		1,
		&rtvFormat,
		dsvFormat
	);

	if ( m_bossPoisonProjectileEffect.entries.size() != kBossPoisonProjectileMaxCount )
		m_bossPoisonProjectileEffect.entries.resize(kBossPoisonProjectileMaxCount);

	m_bossPoisonProjectileEffect.instanceBuffer.Create(dev, cmd, kBossPoisonProjectileMaxCount, [ dev, cmd ] (UINT bufferBytes)
	{
		return ::CreateBufferResource(dev, cmd, nullptr, bufferBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
	});
}

void CGameScene::BuildSwordTrailBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_swordTrailEffect.shader = std::make_shared<CSwordTrailShader>();

	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_swordTrailEffect.shader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		1,
		&rtvFormat,
		dsvFormat
	);

	m_swordTrailEffect.entries.clear();
	m_swordTrailEffect.entries.resize(kSwordTrailMaxCount);

	m_swordTrailEffect.vertexBuffer.Create(dev, cmd, kSwordTrailMaxVertices, [ dev, cmd ] (UINT bufferBytes)
	{
		return ::CreateBufferResource(dev, cmd, nullptr, bufferBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
	});
}

void CGameScene::BuildMonsterSwordTrailBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_monsterSwordTrailEffect.shader = std::make_shared<CSwordTrailShader>();

	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_monsterSwordTrailEffect.shader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		1,
		&rtvFormat,
		dsvFormat
	);

	m_monsterSwordTrailEffect.entries.clear();
	m_monsterSwordTrailEffect.entries.resize(kMonsterSwordTrailMaxCount);

	m_monsterSwordTrailEffect.vertexBuffer.Create(dev, cmd, kMonsterSwordTrailMaxVertices, [ dev, cmd ] (UINT bufferBytes)
	{
		return ::CreateBufferResource(dev, cmd, nullptr, bufferBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
	});
}

void CGameScene::BuildArrowTrailBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_arrowTrailEffect.shader = std::make_shared<CSwordTrailShader>();

	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_arrowTrailEffect.shader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		1,
		&rtvFormat,
		dsvFormat
	);

	m_arrowTrailEffect.entries.clear();
	m_arrowTrailEffect.entries.resize(kArrowTrailMaxCount);

	m_arrowTrailEffect.vertexBuffer.Create(dev, cmd, kArrowTrailMaxVertices, [ dev, cmd ] (UINT bufferBytes)
	{
		return ::CreateBufferResource(dev, cmd, nullptr, bufferBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
	});
}

void CGameScene::BuildBossCallSummonWwwBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_bossCallSummonWwwEffect.shader = std::make_shared<CSwordTrailShader>();

	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_bossCallSummonWwwEffect.shader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		1,
		&rtvFormat,
		dsvFormat
	);

	m_bossCallSummonWwwEffect.entries.clear();
	m_bossCallSummonWwwEffect.entries.resize(kBossCallSummonWwwMaxCount);

	m_bossCallSummonWwwEffect.vertexBuffer.Create(dev, cmd, kBossCallSummonWwwMaxVertices, [ dev, cmd ] (UINT bufferBytes)
	{
		return ::CreateBufferResource(dev, cmd, nullptr, bufferBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
	});
}

void CGameScene::ReleaseBossPoisonProjectileGpuResources()
{
	m_bossPoisonProjectileEffect.instanceBuffer.Release();
}

void CGameScene::ReleaseSwordTrailGpuResources()
{
	m_swordTrailEffect.vertexBuffer.Release();
}

void CGameScene::ReleaseMonsterSwordTrailGpuResources()
{
	m_monsterSwordTrailEffect.vertexBuffer.Release();
}

void CGameScene::ReleaseArrowTrailGpuResources()
{
	m_arrowTrailEffect.vertexBuffer.Release();
}

void CGameScene::ReleaseBossCallSummonWwwGpuResources()
{
	m_bossCallSummonWwwEffect.vertexBuffer.Release();
}

void CGameScene::ReleaseAllGameSceneEffectGpuResources()
{
	ReleaseItemBillboardGpuResources();
	ReleaseMuzzleFlashGpuResources();
	ReleaseBossPoisonProjectileGpuResources();
	ReleaseSwordTrailGpuResources();
	ReleaseMonsterSwordTrailGpuResources();
	ReleaseArrowTrailGpuResources();
	ReleaseBossCallSummonWwwGpuResources();
}

void CGameScene::SpawnBossCallSummonWwwEffect(
	const XMFLOAT3& center,
	EEnemySpawnerEnemyKind kind)
{
#ifndef USING_NETWORK
	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> zeroOneDist(0.0f, 1.0f);
	static std::uniform_real_distribution<float> lifeDist(0.95f, 1.30f);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);

	BossCallSummonWwwEntry* entry = nullptr;

	for ( BossCallSummonWwwEntry& candidate : m_bossCallSummonWwwEffect.entries )
	{
		if ( candidate.active )
			continue;

		entry = &candidate;
		break;
	}

	if ( !entry )
		return;

	const float circleSize = GetBossCallSummonCircleSize(kind);
	const float radius = std::max(0.10f, circleSize * 0.5f);

	XMFLOAT3 fixedCenter = center;
	fixedCenter.y = 0.0f;

	*entry = BossCallSummonWwwEntry{};

	entry->active = true;
	entry->center = fixedCenter;

	entry->radius = radius;
	entry->maxHeight = radius * 2.0f;

	entry->age = 0.0f;
	entry->lifetime = lifeDist(rng);

	entry->retargetTimer = 0.0f;
	entry->seed = seedDist(rng);

	// 잔광 색상과 동일 계열.
	entry->color = XMFLOAT4(0.10f, 0.90f, 0.18f, 0.78f);

	for ( UINT i = 0; i < kBossCallSummonWwwPeakCount; ++i )
	{
		const float h = zeroOneDist(rng) * entry->maxHeight;

		entry->peakHeights[i] = h;
		entry->targetPeakHeights[i] = zeroOneDist(rng) * entry->maxHeight;

		entry->peakMoveSpeeds[i] =
			entry->maxHeight * ( 2.8f + zeroOneDist(rng) * 4.2f );
	}
#else
	UNREFERENCED_PARAMETER(center);
	UNREFERENCED_PARAMETER(kind);
#endif
}

void CGameScene::UpdateBossCallSummonWwwEffects(float dt)
{
#ifndef USING_NETWORK
	if ( dt <= 0.0f )
		return;

	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> zeroOneDist(0.0f, 1.0f);

	for ( BossCallSummonWwwEntry& entry : m_bossCallSummonWwwEffect.entries )
	{
		if ( !entry.active )
			continue;

		entry.age += dt;

		if ( entry.age >= entry.lifetime )
		{
			entry.active = false;
			continue;
		}

		entry.retargetTimer -= dt;

		if ( entry.retargetTimer <= 0.0f )
		{
			// 모든 꼭짓점을 매번 동시에 바꾸면 규칙적으로 보이므로,
			// 일부 꼭짓점만 랜덤하게 새 목표 높이를 받는다.
			for ( UINT i = 0; i < kBossCallSummonWwwPeakCount; ++i )
			{
				if ( zeroOneDist(rng) < 0.68f )
				{
					entry.targetPeakHeights[i] =
						zeroOneDist(rng) * entry.maxHeight;

					entry.peakMoveSpeeds[i] =
						entry.maxHeight *
						( 2.4f + zeroOneDist(rng) * 5.0f );
				}
			}

			entry.retargetTimer =
				0.035f + zeroOneDist(rng) * 0.085f;
		}

		for ( UINT i = 0; i < kBossCallSummonWwwPeakCount; ++i )
		{
			const float target = entry.targetPeakHeights[i];
			const float current = entry.peakHeights[i];

			const float maxStep =
				entry.peakMoveSpeeds[i] * dt;

			const float delta =
				std::clamp(
					target - current,
					-maxStep,
					maxStep
				);

			entry.peakHeights[i] =
				std::clamp(
					current + delta,
					0.0f,
					entry.maxHeight
				);
		}
	}
#else
	UNREFERENCED_PARAMETER(dt);
#endif
}

void CGameScene::BeginSwordTrail(CGameObject* owner)
{
	if ( !owner )
		return;

	auto* equip = owner->GetComponent<CPlayerEquipmentComponent>();
	if ( !equip )
		return;

	CGameObject* swordObject = equip->GetWeaponObject(EWeaponType::Sword);
	if ( !swordObject )
		return;

	SwordTrailEntry* trail = AcquireFreeSwordTrailEntry(m_swordTrailEffect.entries);
	if ( !trail )
		return;

	trail->active = true;
	trail->kind = EWeaponTrailKind::Sword;

	trail->owner = owner;
	trail->weaponObject = swordObject;

	trail->age = 0.0f;
	ResetPlayerMeleeWeaponHitbox(swordObject);

	trail->startDelay = 0.340f;
	trail->sampleDuration = 0.240f;
	trail->fadeDuration = 0.120f;

	trail->rootLocal = XMFLOAT3(0.0f, 0.0f, 0.10f);
	trail->tipLocal = XMFLOAT3(0.0f, 0.0f, 1.45f);

	trail->widthScale = 1.0f;

	// 검 궤적 색상: 푸른빛.
	trail->color = XMFLOAT4(0.55f, 0.80f, 1.0f, 1.0f);

	trail->samples.clear();
	trail->samples.reserve(kSwordTrailMaxSamples);
}

void CGameScene::BeginAxeTrail(CGameObject* owner)
{
	if ( !owner )
		return;

	auto* equip = owner->GetComponent<CPlayerEquipmentComponent>();
	if ( !equip )
		return;

	CGameObject* axeObject = equip->GetWeaponObject(EWeaponType::Axe);
	if ( !axeObject )
		return;

	SwordTrailEntry* trail = AcquireFreeSwordTrailEntry(m_swordTrailEffect.entries);
	if ( !trail )
		return;

	trail->active = true;
	trail->kind = EWeaponTrailKind::Axe;

	trail->owner = owner;
	trail->weaponObject = axeObject;

	trail->age = 0.0f;
	ResetPlayerMeleeWeaponHitbox(axeObject);

	trail->startDelay = 0.530f;
	trail->sampleDuration = 0.160f; // 0.690f - 0.530f
	trail->fadeDuration = 0.120f;

	trail->rootLocal = XMFLOAT3(0.0f, 0.0f, 0.80f);
	trail->tipLocal = XMFLOAT3(0.0f, 0.0f, 1.45f);

	trail->widthScale = 0.80f;

	// 도끼 궤적 색상: 검과 동일한 푸른빛.
	trail->color = XMFLOAT4(0.55f, 0.80f, 1.0f, 1.0f);

	trail->samples.clear();
	trail->samples.reserve(kSwordTrailMaxSamples);
}

void CGameScene::BeginSwordManSwordTrail(CGameObject* swordman)
{
	if ( !swordman )
		return;

	if ( IsMonsterDead(swordman) )
		return;

	const int swordManIndex = GetSwordManIndexFromObject(swordman);
	if ( swordManIndex < 0 )
		return;

	const size_t index = static_cast< size_t >(swordManIndex);

	if ( index >= m_EnemySwordRefs.size() )
		return;

	CGameObject* swordObject = m_EnemySwordRefs[index];
	if ( !swordObject )
		return;

	MonsterSwordTrailEntry* trail =
		AcquireFreeMonsterSwordTrailEntry(m_monsterSwordTrailEffect.entries);

	if ( !trail )
		return;

	trail->active = true;

	trail->owner = swordman;
	trail->weaponObject = swordObject;

	trail->age = 0.0f;

	trail->startDelay = 0.340f;
	trail->sampleDuration = 0.240f;
	trail->fadeDuration = 0.120f;

	trail->rootLocal = XMFLOAT3(0.0f, 0.0f, 0.10f * 1.5f);
	trail->tipLocal = XMFLOAT3(0.0f, 0.0f, 1.45f * 1.5f);

	trail->widthScale = 1.0f;

	trail->color = XMFLOAT4(0.55f, 0.80f, 1.0f, 1.0f);

	trail->samples.clear();
	trail->samples.reserve(kMonsterSwordTrailMaxSamples);
}

void CGameScene::UpdateSwordTrails(float dt)
{
	if ( dt <= 0.0f )
		return;

	for ( SwordTrailEntry& trail : m_swordTrailEffect.entries )
	{
		if ( !trail.active )
			continue;

		if ( !trail.weaponObject )
		{
			FinishSwordTrailEntry(trail);
			continue;
		}

		trail.age += dt;

		const float totalLife =
			trail.startDelay +
			trail.sampleDuration +
			trail.fadeDuration;

		if ( trail.age >= totalLife )
		{
			FinishSwordTrailEntry(trail);
			continue;
		}

		if ( trail.age < trail.startDelay )
		{
			SetPlayerMeleeWeaponHitboxActive(trail.weaponObject, false);
			continue;
		}

		const float sampleAge = trail.age - trail.startDelay;

		const bool hitWindowActive = ( sampleAge <= trail.sampleDuration );

		SetPlayerMeleeWeaponHitboxActive(trail.weaponObject, hitWindowActive);

		if ( hitWindowActive )
		{
			AppendSwordTrailSample(trail, kSwordTrailMaxSamples);
		}
	}
}

void CGameScene::UpdateMonsterSwordTrails(float dt)
{
	if ( dt <= 0.0f )
		return;

	for ( MonsterSwordTrailEntry& trail : m_monsterSwordTrailEffect.entries )
	{
		if ( !trail.active )
			continue;

		if ( !trail.owner || IsMonsterDead(trail.owner) )
		{
			FinishMonsterSwordTrailEntry(trail);
			continue;
		}

		if ( !trail.weaponObject )
		{
			FinishMonsterSwordTrailEntry(trail);
			continue;
		}

		trail.age += dt;

		const float totalLife =
			trail.startDelay +
			trail.sampleDuration +
			trail.fadeDuration;

		if ( trail.age >= totalLife )
		{
			FinishMonsterSwordTrailEntry(trail);
			continue;
		}

		if ( trail.age < trail.startDelay )
			continue;

		const float sampleAge = trail.age - trail.startDelay;

		const bool sampleWindowActive =
			( sampleAge <= trail.sampleDuration );

		if ( sampleWindowActive )
		{
			AppendMonsterSwordTrailSample(
				trail,
				kMonsterSwordTrailMaxSamples
			);
		}
	}
}

void CGameScene::UpdateArrowTrails(float dt)
{
	if ( dt <= 0.0f )
		return;

	constexpr float kArrowTrailSampleLifetimeSec = 0.260f;

	// 기존 trail sample age 증가 및 만료 처리.
	for ( ArrowTrailEntry& trail : m_arrowTrailEffect.entries )
	{
		if ( !trail.active )
			continue;

		for ( ArrowTrailSample& sample : trail.samples )
			sample.age += dt;

		while ( !trail.samples.empty() &&
				trail.samples.front().age >= kArrowTrailSampleLifetimeSec )
		{
			trail.samples.pop_front();
		}

		if ( trail.arrowObject )
		{
			CArrowComponent* arrow =
				trail.arrowObject->GetComponent<CArrowComponent>();

			const bool launched =
				arrow &&
				arrow->IsActive() &&
				!arrow->IsPrepared();

			if ( !launched )
				trail.arrowObject = nullptr;
		}

		if ( !trail.arrowObject && trail.samples.empty() )
			ResetArrowTrailEntry(trail);
	}

	// 현재 발사 중인 화살 위치를 trail에 샘플링한다.
	for ( CGameObject* arrowObj : m_arrowRefs )
	{
		if ( !arrowObj )
			continue;

		CArrowComponent* arrow =
			arrowObj->GetComponent<CArrowComponent>();

		if ( !arrow )
			continue;

		// 준비 중인 화살은 활에 붙어 있으므로 잔상 생성 금지.
		if ( !arrow->IsActive() || arrow->IsPrepared() )
			continue;

		ArrowTrailEntry* trail =
			FindArrowTrailEntry(m_arrowTrailEffect.entries, arrowObj);

		if ( !trail )
		{
			trail = AcquireFreeArrowTrailEntry(m_arrowTrailEffect.entries);
			if ( !trail )
				continue;

			*trail = ArrowTrailEntry{};
			trail->active = true;
			trail->arrowObject = arrowObj;
		}

		AppendArrowTrailSample(
			*trail,
			arrowObj->GetPosition(),
			kArrowTrailMaxSamples
		);
	}
}

void CGameScene::SpawnBossSummonCircle(const XMFLOAT3& center, float alpha)
{
	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( item.kind != EItemBillboardKind::BossSummonCircle )
			continue;

		XMFLOAT3 fixedCenter = center;
		fixedCenter.y = 0.0f;

		item.active = true;
		item.distanceCulled = false;
		item.transparent = true;

		item.position = fixedCenter;

		item.width = 100.0f;
		item.height = 100.0f;
		item.yOffset = 0.05f;

		item.cullDistance = 1000000.0f;

		item.pickupRadius = 0.0f;
		item.pickupHeightTolerance = 0.0f;

		item.materialId = kBossSummonCircleMaterialId;

		SetBossSummonCircleAlpha(alpha);

		return;
	}

	OutputDebugStringA("[BossSummonCircle] spawn failed: BossSummonCircle entry not found.\n");
}

void CGameScene::SpawnBossSummonGlow(const XMFLOAT3& center, float alpha)
{
	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( item.kind != EItemBillboardKind::BossSummonGlow )
			continue;

		XMFLOAT3 fixedCenter = center;
		fixedCenter.y = 0.0f;

		item.active = true;
		item.distanceCulled = false;
		item.transparent = true;

		item.position = fixedCenter;

		item.width = 110.0f;
		item.height = 110.0f;
		item.yOffset = 0.01f;

		item.cullDistance = 1000000.0f;

		item.pickupRadius = 0.0f;
		item.pickupHeightTolerance = 0.0f;

		item.materialId = kBossSummonGlowMaterialId;

		SetBossSummonGlowAlpha(alpha);

		return;
	}

	OutputDebugStringA("[BossSummonGlow] spawn failed: BossSummonGlow entry not found.\n");
}

void CGameScene::SpawnBossSummonVisuals(const XMFLOAT3& center, float alpha)
{
	SpawnBossSummonGlow(center, alpha);
	SpawnBossSummonCircle(center, alpha);

	SetBossSummonVisualAlpha(alpha);
}

void CGameScene::SetBossCallSummonCircleAlpha(float alpha)
{
	if ( !m_pMaterials )
		return;

	alpha = std::clamp(alpha, 0.0f, 1.0f);

	MATERIAL& mat =
		m_pMaterials->m_pReflections[kBossCallSummonCircleMaterialId];

	mat.m_xmf4Diffuse.w = alpha;

	m_itemBillboardState.bossCallSummonCircleVisual.alpha = alpha;
}

float CGameScene::GetBossCallSummonCircleSize(
	EEnemySpawnerEnemyKind kind) const
{
	switch ( kind )
	{
	case EEnemySpawnerEnemyKind::Ghoul:
		return 2.0f;

	case EEnemySpawnerEnemyKind::SwordMan:
	case EEnemySpawnerEnemyKind::BowMan:
		return 4.0f;

	case EEnemySpawnerEnemyKind::Mutant:
		return 4.0f;

	default:
		return 1.0f;
	}
}

void CGameScene::ClearBossCallSummonCircleVisuals()
{
	m_itemBillboardState.activeBossCallSummonCircleItemIndices.clear();

	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( item.kind != EItemBillboardKind::BossCallSummonCircle )
			continue;

		item.active = false;
		item.distanceCulled = true;
		item.width = 0.0f;
		item.height = 0.0f;
	}

	m_itemBillboardState.bossCallSummonCircleVisual = BossCallSummonCircleVisualState{};
	m_itemBillboardState.bossCallSummonGlowParticleEmitAccumulatorSec = 0.0f;

	SetBossCallSummonCircleAlpha(0.0f);
}

void CGameScene::AddBossCallSummonCircle(
	const XMFLOAT3& center,
	EEnemySpawnerEnemyKind kind)
{
	const float size = GetBossCallSummonCircleSize(kind);

	for ( size_t i = 0; i < m_itemBillboardState.entries.size(); ++i )
	{
		ItemBillboardEntry& item = m_itemBillboardState.entries[i];

		if ( item.kind != EItemBillboardKind::BossCallSummonCircle )
			continue;

		if ( item.active )
			continue;

		XMFLOAT3 fixedCenter = center;
		fixedCenter.y = 0.0f;

		item.active = true;
		item.distanceCulled = false;
		item.transparent = true;

		item.position = fixedCenter;

		item.width = size;
		item.height = size;
		item.yOffset = 0.05f;

		item.cullDistance = 1000000.0f;

		item.pickupRadius = 0.0f;
		item.pickupHeightTolerance = 0.0f;

		item.materialId = kBossCallSummonCircleMaterialId;

		m_itemBillboardState.activeBossCallSummonCircleItemIndices.push_back(i);
		return;
	}

	OutputDebugStringA(
		"[BossCallSummonCircle] add failed: no free BossCallSummonCircle entry.\n"
	);
}

void CGameScene::BeginBossCallMonsterSummonVisuals(
	int callIndex,
	float fadeInDurationSec)
{
#ifndef USING_NETWORK
	ClearBossCallSummonCircleVisuals();

	m_bossCallSummonPlanCallIndex = -1;
	m_bossCallSummonPlanEntries.clear();

	if ( !m_enemySpawner )
		return;

	if ( callIndex < 1 || callIndex > 3 )
		return;

	m_bossCallSummonPlanCallIndex = callIndex;

	constexpr int megaGridNumber = 5;

	auto AddPreviewKind =
		[ & ](
			EEnemySpawnerEnemyKind kind,
			int count
		)
		{
			if ( count <= 0 )
				return;

			std::vector<EnemySpawnerPreviewEntry> previews;
			previews.reserve(static_cast< size_t >( count ));

			const int found =
				m_enemySpawner->PeekSpawnEntries(
					megaGridNumber,
					kind,
					count,
					previews
				);

			for ( const EnemySpawnerPreviewEntry& preview : previews )
			{
				m_bossCallSummonPlanEntries.push_back(preview);

				AddBossCallSummonCircle(
					preview.spawnPosition,
					preview.kind
				);
			}
		};

	switch ( callIndex )
	{
	case 1:
		AddPreviewKind(EEnemySpawnerEnemyKind::Ghoul, 30);
		break;

	case 2:
		AddPreviewKind(EEnemySpawnerEnemyKind::Ghoul, 20);
		AddPreviewKind(EEnemySpawnerEnemyKind::BowMan, 5);
		AddPreviewKind(EEnemySpawnerEnemyKind::SwordMan, 5);
		break;

	case 3:
		AddPreviewKind(EEnemySpawnerEnemyKind::Ghoul, 20);
		AddPreviewKind(EEnemySpawnerEnemyKind::BowMan, 5);
		AddPreviewKind(EEnemySpawnerEnemyKind::SwordMan, 5);
		AddPreviewKind(EEnemySpawnerEnemyKind::Mutant, 5);
		break;

	default:
		break;
	}

	if ( m_itemBillboardState.activeBossCallSummonCircleItemIndices.empty() )
	{
#ifndef USING_NETWORK
		char buf[256];
		sprintf_s(
			buf,
			"[BossCallSummonCircle][BeginFailed] call=%d plan=%zu activeCircle=0\n",
			callIndex,
			m_bossCallSummonPlanEntries.size()
		);
		OutputDebugStringA(buf);
#endif
		m_bossCallSummonPlanCallIndex = -1;
		m_bossCallSummonPlanEntries.clear();
		return;
	}

	m_itemBillboardState.bossCallSummonCircleVisual = BossCallSummonCircleVisualState{};
	m_itemBillboardState.bossCallSummonCircleVisual.active = true;
	m_itemBillboardState.bossCallSummonCircleVisual.fadingIn = true;
	m_itemBillboardState.bossCallSummonCircleVisual.fadingOut = false;
	m_itemBillboardState.bossCallSummonCircleVisual.ageSec = 0.0f;
	m_itemBillboardState.bossCallSummonCircleVisual.durationSec =
		( fadeInDurationSec > 1.0e-6f ) ? fadeInDurationSec : 0.001f;
	m_itemBillboardState.bossCallSummonCircleVisual.alpha = 0.0f;

	SetBossCallSummonCircleAlpha(0.0f);

	{
		XMFLOAT3 sfxPos = XMFLOAT3(400.0f, 0.0f, 400.0f);

		if ( !m_bossCallSummonPlanEntries.empty() )
		{
			XMFLOAT3 sum = XMFLOAT3(0.0f, 0.0f, 0.0f);

			for ( const EnemySpawnerPreviewEntry& entry :
				  m_bossCallSummonPlanEntries )
			{
				sum.x += entry.spawnPosition.x;
				sum.y += entry.spawnPosition.y;
				sum.z += entry.spawnPosition.z;
			}

			const float invCount =
				1.0f /
				static_cast< float >( m_bossCallSummonPlanEntries.size() );

			sfxPos.x = sum.x * invCount;
			sfxPos.y = sum.y * invCount;
			sfxPos.z = sum.z * invCount;
		}

		sfxPos.y = 0.0f;

		PlayBossCallSummonCircleSfxAt(sfxPos);

	}

#else
	UNREFERENCED_PARAMETER(callIndex);
	UNREFERENCED_PARAMETER(fadeInDurationSec);
#endif
}

void CGameScene::StartBossCallSummonCircleFadeOut()
{
#ifndef USING_NETWORK
	if ( !m_itemBillboardState.bossCallSummonCircleVisual.active )
		return;

	if ( m_itemBillboardState.activeBossCallSummonCircleItemIndices.empty() )
		return;

	m_itemBillboardState.bossCallSummonCircleVisual.fadingIn = false;
	m_itemBillboardState.bossCallSummonCircleVisual.fadingOut = true;
	m_itemBillboardState.bossCallSummonCircleVisual.ageSec = 0.0f;
	m_itemBillboardState.bossCallSummonCircleVisual.durationSec =
		( kBossCallSummonCircleFadeOutDurationSec > 1.0e-6f )
		? kBossCallSummonCircleFadeOutDurationSec
		: 0.001f;
#endif
}

void CGameScene::EmitBossCallSummonCircleGlowParticles(
	float dt,
	float alpha)
{
#ifndef USING_NETWORK
	if ( dt <= 0.0f )
		return;

	if ( alpha <= 0.001f )
		return;

	if ( m_itemBillboardState.activeBossCallSummonCircleItemIndices.empty() )
		return;

	const float emitIntervalSec =
		( kBossCallSummonGlowParticleEmitIntervalSec > 1.0e-6f )
		? kBossCallSummonGlowParticleEmitIntervalSec
		: 0.001f;

	m_itemBillboardState.bossCallSummonGlowParticleEmitAccumulatorSec += dt;

	if ( m_itemBillboardState.bossCallSummonGlowParticleEmitAccumulatorSec < emitIntervalSec )
		return;

	int emitCount =
		static_cast< int >(
			m_itemBillboardState.bossCallSummonGlowParticleEmitAccumulatorSec /
			emitIntervalSec
		);

	m_itemBillboardState.bossCallSummonGlowParticleEmitAccumulatorSec =
		std::fmod(
			m_itemBillboardState.bossCallSummonGlowParticleEmitAccumulatorSec,
			emitIntervalSec
		);

	if ( emitCount > 3 )
		emitCount = 3;

	for ( size_t itemIndex : m_itemBillboardState.activeBossCallSummonCircleItemIndices )
	{
		if ( itemIndex >= m_itemBillboardState.entries.size() )
			continue;

		const ItemBillboardEntry& item = m_itemBillboardState.entries[itemIndex];

		if ( !item.active )
			continue;

		if ( item.kind != EItemBillboardKind::BossCallSummonCircle )
			continue;

		XMFLOAT3 center = item.position;
		center.y += item.yOffset;

		const float circleSize =
			std::max(item.width, item.height);

		for ( int n = 0; n < emitCount; ++n )
		{
			for ( int i = 0; i < kBossCallSummonGlowParticlesPerEmit; ++i )
			{
				SpawnMagicCircleGlowParticle(
					center,
					circleSize,
					alpha,
					kBossCallSummonGlowParticleIntensityScale,
					kBossCallSummonGlowParticleSizeScale,
					kBossCallSummonAfterimageParticleSizeScale,
					kBossCallSummonGlowParticleLifetimeScale
				);
			}
		}
	}
#else
	UNREFERENCED_PARAMETER(dt);
	UNREFERENCED_PARAMETER(alpha);
#endif
}

void CGameScene::UpdateBossCallSummonCircles(float dt)
{
#ifndef USING_NETWORK
	if ( !m_itemBillboardState.bossCallSummonCircleVisual.active )
		return;

	if ( dt < 0.0f )
		dt = 0.0f;

	BossCallSummonCircleVisualState& state =
		m_itemBillboardState.bossCallSummonCircleVisual;

	state.ageSec += dt;

	const float duration =
		( state.durationSec > 1.0e-6f )
		? state.durationSec
		: 0.001f;

	const float t =
		std::clamp(
			state.ageSec / duration,
			0.0f,
			1.0f
		);

	if ( state.fadingIn )
	{
		const float alpha = t;

		SetBossCallSummonCircleAlpha(alpha);
		EmitBossCallSummonCircleGlowParticles(dt, alpha);

		if ( t >= 1.0f )
		{
			state.fadingIn = false;
			state.ageSec = 0.0f;
			state.durationSec = 0.0f;
			SetBossCallSummonCircleAlpha(1.0f);
		}

		return;
	}

	if ( state.fadingOut )
	{
		const float alpha = 1.0f - t;

		SetBossCallSummonCircleAlpha(alpha);
		EmitBossCallSummonCircleGlowParticles(dt, alpha);

		if ( t >= 1.0f )
		{
			ClearBossCallSummonCircleVisuals();
		}

		return;
	}

	// fade-in 완료 후 실제 몬스터 생성 전까지도
	// 마법진 잔광을 계속 발생시킨다.
	if ( state.active )
	{
		SetBossCallSummonCircleAlpha(1.0f);
		EmitBossCallSummonCircleGlowParticles(dt, 1.0f);
		return;
	}
#else
	UNREFERENCED_PARAMETER(dt);
#endif
}

void CGameScene::SpawnBossShockwave(const XMFLOAT3& center)
{
	XMFLOAT3 fixedCenter = center;
	fixedCenter.y = 0.0f;

	m_bossShockwaveCenter = fixedCenter;
	m_bBossShockwaveActive = true;
	m_bossShockwaveAgeSec = 0.0f;

	m_bossShockwavePrevRadius = kBossShockwaveStartRadius;
	m_bBossShockwavePushLocalPlayer = false;
	m_bossShockwavePlayerInitialDistance = 0.0f;
	m_bossShockwavePlayerPushDir = XMFLOAT3(0.0f, 0.0f, 1.0f);

	m_bossShockwaveWindSfxDirection = XMFLOAT3(0.0f, 0.0f, 1.0f);

	CGameObject* localPlayer = GetPlayer();

	if ( localPlayer && !m_bLocalPlayerDead )
	{
		const XMFLOAT3 playerPos = localPlayer->GetPosition();

		const float dx = playerPos.x - fixedCenter.x;
		const float dz = playerPos.z - fixedCenter.z;

		const float distSq = dx * dx + dz * dz;
		float dist = sqrtf(distSq);

		XMFLOAT3 radialDir = XMFLOAT3(0.0f, 0.0f, 1.0f);

		if ( dist > kBossShockwavePlayerMinDirectionDistance )
		{
			const float invDist = 1.0f / dist;

			radialDir.x = dx * invDist;
			radialDir.y = 0.0f;
			radialDir.z = dz * invDist;
		}
		else
		{
			dist = 0.0f;
		}

		// 바람 사운드는 이 방향으로 퍼지는 충격파 전면을 따라간다.
		m_bossShockwaveWindSfxDirection = radialDir;

		const float maxAffectRadius =
			kBossShockwaveMaxRadius + kBossShockwavePlayerRangePadding;

		const float maxAffectRadiusSq =
			maxAffectRadius * maxAffectRadius;

		if ( distSq <= maxAffectRadiusSq )
		{
			m_bBossShockwavePushLocalPlayer = true;
			m_bossShockwavePlayerInitialDistance = dist;
			m_bossShockwavePlayerPushDir = radialDir;
		}
	}

	// 바람 생성 순간에는 보스/충격파 중심에서 1회 재생한다.
	// 이후 UpdateBossShockwaveWindSfx()가 충격파 반지름에 맞춰 위치를 이동시킨다.
	PlayBossShockwaveWindSfxAt(fixedCenter);

	SetBossShockwaveAlpha(1.0f);
	SetBossShockwaveWallAlpha(0.72f);

	const float correctedRadius =
		kBossShockwaveStartRadius / kBossShockwaveShaderRingCenter;

	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( item.kind == EItemBillboardKind::BossShockwave )
		{
			item.active = true;
			item.distanceCulled = false;
			item.transparent = true;

			item.position = fixedCenter;
			item.width = correctedRadius * 2.0f;
			item.height = correctedRadius * 2.0f;
			item.yOffset = 0.075f;
			item.cullDistance = 1000000.0f;
			item.pickupRadius = 0.0f;
			item.pickupHeightTolerance = 0.0f;
			item.materialId = kBossShockwaveMaterialId;
		}
		else if ( item.kind == EItemBillboardKind::BossShockwaveWall )
		{
			item.active = true;
			item.distanceCulled = false;
			item.transparent = true;

			item.position = fixedCenter;
			item.width = kBossShockwaveWallMinWidth;
			item.height = 0.25f;
			item.yOffset = 0.15f;

			item.cullDistance = 1000000.0f;
			item.pickupRadius = 0.0f;
			item.pickupHeightTolerance = 0.0f;
			item.materialId = kBossShockwaveWallMaterialId;
		}
	}
}

void CGameScene::UpdateBossShockwave(float dt)
{
#ifndef USING_NETWORK
	if ( !m_bBossShockwaveActive )
		return;

	if ( dt < 0.0f )
		dt = 0.0f;

	m_bossShockwaveAgeSec += dt;

	const float totalDuration =
		kBossShockwaveExpandDurationSec + kBossShockwaveFadeDurationSec;

	if ( m_bossShockwaveAgeSec >= totalDuration )
	{
		m_bBossShockwaveActive = false;
		m_bossShockwaveAgeSec = 0.0f;

		m_bBossShockwavePushLocalPlayer = false;
		m_bossShockwavePrevRadius = 0.0f;
		m_bossShockwavePlayerInitialDistance = 0.0f;
		m_bossShockwavePlayerPushDir = XMFLOAT3(0.0f, 0.0f, 1.0f);

		ResetBossShockwaveWindSfxTracking();

		for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
		{
			if ( item.kind != EItemBillboardKind::BossShockwave &&
				 item.kind != EItemBillboardKind::BossShockwaveWall )
			{
				continue;
			}

			item.active = false;
			item.distanceCulled = true;
			item.width = 0.0f;
			item.height = 0.0f;
		}

		SetBossShockwaveAlpha(0.0f);
		SetBossShockwaveWallAlpha(0.0f);
		return;
	}

	float radius = kBossShockwaveMaxRadius;
	float floorAlpha = 1.0f;
	float wallAlpha = 0.72f;

	if ( m_bossShockwaveAgeSec < kBossShockwaveExpandDurationSec )
	{
		const float t =
			( kBossShockwaveExpandDurationSec > 1.0e-6f )
			? std::clamp(
				m_bossShockwaveAgeSec / kBossShockwaveExpandDurationSec,
				0.0f,
				1.0f
			)
			: 1.0f;

		const float easeOut = 1.0f - ( 1.0f - t ) * ( 1.0f - t );

		radius =
			kBossShockwaveStartRadius +
			( kBossShockwaveMaxRadius - kBossShockwaveStartRadius ) * easeOut;

		floorAlpha = 1.0f;
		wallAlpha = 0.72f;
	}
	else
	{
		const float fadeAge =
			m_bossShockwaveAgeSec - kBossShockwaveExpandDurationSec;

		const float fadeT =
			( kBossShockwaveFadeDurationSec > 1.0e-6f )
			? std::clamp(
				fadeAge / kBossShockwaveFadeDurationSec,
				0.0f,
				1.0f
			)
			: 1.0f;

		radius = kBossShockwaveMaxRadius;
		floorAlpha = 1.0f - fadeT;
		wallAlpha = ( 1.0f - fadeT ) * 0.72f;
	}

	ApplyBossShockwavePushToLocalPlayer(m_bossShockwavePrevRadius, radius);
	UpdateBossShockwaveWindSfx(radius);

	m_bossShockwavePrevRadius = radius;
	const float correctedRadius =
		radius / kBossShockwaveShaderRingCenter;

	// 바닥 충격파 갱신
	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( item.kind != EItemBillboardKind::BossShockwave )
			continue;

		item.active = true;
		item.distanceCulled = false;
		item.transparent = true;

		item.position = m_bossShockwaveCenter;
		item.width = correctedRadius * 2.0f;
		item.height = correctedRadius * 2.0f;
		item.yOffset = 0.075f;
		item.materialId = kBossShockwaveMaterialId;
	}

	// 세로 먼지 벽 갱신
	UINT wallIndex = 0;
	const float timeRatio =
		std::clamp(m_bossShockwaveAgeSec / totalDuration, 0.0f, 1.0f);

	const float wallHeight =
		kBossShockwaveWallMaxHeight * sinf(timeRatio * XM_PI);

	const float arcLength =
		std::max(
			kBossShockwaveWallMinWidth,
			( radius * XM_2PI / static_cast< float >( kBossShockwaveWallSegmentCount ) ) *
			kBossShockwaveWallWidthScale
		);

	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( item.kind != EItemBillboardKind::BossShockwaveWall )
			continue;

		const float angle =
			( static_cast< float >( wallIndex ) / static_cast< float >( kBossShockwaveWallSegmentCount ) ) *
			XM_2PI;

		XMFLOAT3 pos = m_bossShockwaveCenter;
		pos.x += cosf(angle) * radius;
		pos.z += sinf(angle) * radius;
		pos.y = 0.0f;

		item.active = true;
		item.distanceCulled = false;
		item.transparent = true;

		item.position = pos;
		item.width = arcLength;
		item.height = std::max(0.25f, wallHeight);
		item.yOffset = item.height * 0.5f + 0.05f;
		item.materialId = kBossShockwaveWallMaterialId;

		++wallIndex;
	}

	SetBossShockwaveAlpha(floorAlpha);
	SetBossShockwaveWallAlpha(wallAlpha);
#else
	UNREFERENCED_PARAMETER(dt);
#endif
}

void CGameScene::ApplyBossShockwavePushToLocalPlayer(
	float previousRadius,
	float currentRadius)
{
#ifndef USING_NETWORK
	if ( !m_bBossShockwavePushLocalPlayer )
		return;

	if ( currentRadius <= previousRadius )
		return;

	CGameObject* localPlayer = GetPlayer();
	if ( !localPlayer )
		return;

	if ( m_bLocalPlayerDead )
		return;

	float playerStartDistance = m_bossShockwavePlayerInitialDistance;

	if ( playerStartDistance < kBossShockwaveStartRadius )
		playerStartDistance = kBossShockwaveStartRadius;

	float prevCarryRadius = previousRadius;

	if ( prevCarryRadius < playerStartDistance )
		prevCarryRadius = playerStartDistance;

	float currCarryRadius = currentRadius;

	if ( currCarryRadius > kBossShockwaveMaxRadius )
		currCarryRadius = kBossShockwaveMaxRadius;

	const float pushDistance =
		currCarryRadius - prevCarryRadius;

	if ( pushDistance <= 0.0f )
		return;

	const XMFLOAT3 previousPos = localPlayer->GetPosition();

	XMFLOAT3 pushedPos = previousPos;
	pushedPos.x += m_bossShockwavePlayerPushDir.x * pushDistance;
	pushedPos.z += m_bossShockwavePlayerPushDir.z * pushDistance;

	localPlayer->SetPosition(pushedPos);

	auto* collider = localPlayer->GetComponent<CColliderComponent>();

	if ( collider )
		collider->UpdateWorldBounds();

	if ( collider && m_Collision )
	{
		if ( m_Collision->HasCollisionWithWorldStatic(collider) )
		{
			localPlayer->SetPosition(previousPos);
			collider->UpdateWorldBounds();
			return;
		}
	}

	if ( auto* controller = localPlayer->GetComponent<CPlayerControllerComponent>() )
	{
		controller->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
	}

	if ( auto* camera = GetMainCamera() )
	{
		XMFLOAT3 cameraTarget = localPlayer->GetPosition();
		cameraTarget.y += 1.7f;

		camera->Update(cameraTarget, 0.0f);
		camera->SetLookAt(cameraTarget);
		camera->RegenerateViewMatrix();
	}

	UpdateDynamicGridState();
#else
	UNREFERENCED_PARAMETER(previousRadius);
	UNREFERENCED_PARAMETER(currentRadius);
#endif
}

void CGameScene::ResetBossPoisonProjectileState()
{
	m_bossPoisonProjectileEffect.entries.clear();
	m_bossPoisonProjectileEffect.entries.resize(kBossPoisonProjectileMaxCount);

	m_bossPoisonProjectileEffect.spellCastStates.clear();
	m_bossMeleeSlashCastStates.clear();
}

BossPoisonProjectileEntry* CGameScene::AcquireFreeBossPoisonProjectileEntry()
{
	for ( BossPoisonProjectileEntry& entry : m_bossPoisonProjectileEffect.entries )
	{
		if ( !entry.active )
			return &entry;
	}

	return nullptr;
}

bool CGameScene::IsBossPoisonProjectilePlayerRollInvincible(
	const CGameObject* player) const
{
	if ( !player )
		return false;

	if ( auto* animComp = player->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->GetController() )
			return ctrl->IsRollPhase();
	}

	if ( auto* ctrl = player->GetAnimController() )
		return ctrl->IsRollPhase();

	return false;
}

bool CGameScene::DoesBossPoisonProjectileOverlapPlayer(
	const BossPoisonProjectileEntry& entry,
	const CGameObject* player) const
{
	if ( !entry.active )
		return false;

	if ( !player )
		return false;

	const XMFLOAT3 playerBasePos = player->GetPosition();

	XMFLOAT3 playerHitCenter = playerBasePos;
	playerHitCenter.y += kBossPoisonProjectilePlayerHitCenterYOffset;

	const float gasRadius = entry.gasDiameter * 0.5f;

	const float hitRadius =
		gasRadius + kBossPoisonProjectilePlayerCollisionRadius;

	const float dx = entry.position.x - playerHitCenter.x;
	const float dz = entry.position.z - playerHitCenter.z;

	const float distSqXZ = dx * dx + dz * dz;

	if ( distSqXZ > hitRadius * hitRadius )
		return false;

	const float verticalTolerance =
		gasRadius + kBossPoisonProjectilePlayerHalfHeight;

	const float dy = fabsf(entry.position.y - playerHitCenter.y);

	if ( dy > verticalTolerance )
		return false;

	return true;
}

void CGameScene::ApplyBossPoisonProjectileHitToPlayer(
	BossPoisonProjectileEntry& entry,
	int playerSlot,
	CGameObject* player)
{
#ifndef USING_NETWORK
	if ( playerSlot < 0 || playerSlot >= 4 )
		return;

	if ( !player )
		return;

	const size_t slotIndex = static_cast< size_t >(playerSlot);

	if ( entry.hitPlayerSlots[slotIndex] )
		return;

	if ( playerSlot == m_localPlayerSlot && m_bLocalPlayerDead )
		return;

	if ( IsBossPoisonProjectilePlayerRollInvincible(player) )
		return;

	auto* hp = player->GetComponent<CHealthComponent>();

	if ( !hp )
		return;

	if ( hp->IsDead() )
		return;

	const bool damaged =
		hp->TakeDamage(kBossPoisonProjectileDamage);

	if ( !damaged )
		return;

	entry.hitPlayerSlots[slotIndex] = true;

	XMFLOAT3 hitDir = entry.direction;

	if ( hitDir.x * hitDir.x + hitDir.z * hitDir.z <= 1.0e-8f )
		hitDir = XMFLOAT3(0.0f, 0.0f, 1.0f);

	SpawnBloodSplash(player, nullptr, &hitDir);

	const bool deadAfterHit = hp->IsDead();

	if ( deadAfterHit )
	{
		if ( playerSlot == m_localPlayerSlot )
		{
			BeginLocalPlayerDeath(player);
		}
		else
		{
			if ( auto* animComp = player->GetComponent<CAnimatorComponent>() )
			{
				if ( auto* ctrl = animComp->EnsureController() )
				{
					ctrl->RequestDeath();
				}
			}
			else if ( auto* ctrl = player->GetAnimController() )
			{
				ctrl->RequestDeath();
			}
		}
	}
	else
	{
		if ( auto* animComp = player->GetComponent<CAnimatorComponent>() )
		{
			if ( auto* ctrl = animComp->EnsureController() )
			{
				ctrl->RequestHit();
			}
		}
		else if ( auto* ctrl = player->GetAnimController() )
		{
			ctrl->RequestHit();
		}
	}

#else
	UNREFERENCED_PARAMETER(entry);
	UNREFERENCED_PARAMETER(playerSlot);
	UNREFERENCED_PARAMETER(player);
#endif
}

void CGameScene::ApplyBossPoisonProjectilePlayerHits(
	BossPoisonProjectileEntry& entry)
{
#ifndef USING_NETWORK
	if ( !entry.active )
		return;

	for ( int slot = 0; slot < 4; ++slot )
	{
		const size_t slotIndex = static_cast< size_t >(slot);

		if ( entry.hitPlayerSlots[slotIndex] )
			continue;

		CGameObject* player = GetPlayerBySlot(slot);

		if ( !player )
			continue;

		if ( !DoesBossPoisonProjectileOverlapPlayer(entry, player) )
			continue;

		ApplyBossPoisonProjectileHitToPlayer(entry, slot, player);
	}
#else
	UNREFERENCED_PARAMETER(entry);
#endif
}

void CGameScene::UpdateBossMeleeSlashCasts(float dt)
{
#ifndef USING_NETWORK
	if ( dt < 0.0f )
		dt = 0.0f;

	for ( CGameObject* boss : m_bossRefs )
	{
		if ( !boss )
			continue;

		if ( !boss->GetActive() || IsMonsterDead(boss) )
		{
			m_bossMeleeSlashCastStates.erase(boss);
			continue;
		}

		CAnimatorComponent* animComp =
			boss->GetComponent<CAnimatorComponent>();

		if ( !animComp )
		{
			m_bossMeleeSlashCastStates.erase(boss);
			continue;
		}

		CMonsterAnimController* ctrl =
			animComp->EnsureMonsterController();

		if ( !ctrl )
		{
			m_bossMeleeSlashCastStates.erase(boss);
			continue;
		}

		const bool isMeleePhase =
			ctrl->IsAttackPrimaryPhase() ||
			ctrl->IsAttackChainPhase();

		BossMeleeSlashCastState& state =
			m_bossMeleeSlashCastStates[boss];

		if ( !isMeleePhase )
		{
			state = BossMeleeSlashCastState{};
			continue;
		}

		if ( !state.wasMeleePhase )
		{
			state.wasMeleePhase = true;
			state.pendingSpawn = true;
			state.spawned = false;
			state.meleeAgeSec = 0.0f;

			RequestBossAttackSfx(boss);
		}
		else
		{
			state.meleeAgeSec += dt;
		}

		if ( state.pendingSpawn &&
			!state.spawned &&
			 state.meleeAgeSec >= kBossMeleeSlashLaunchDelaySec )
		{
			SpawnBossMeleeSlashEffect(boss);

			state.spawned = true;
			state.pendingSpawn = false;
		}
	}
#else
	UNREFERENCED_PARAMETER(dt);
#endif
}

void CGameScene::UpdateBossPoisonProjectileSpellCasts(float dt)
{
#ifndef USING_NETWORK
	if ( dt < 0.0f )
		dt = 0.0f;

	for ( CGameObject* boss : m_bossRefs )
	{
		if ( !boss )
			continue;

		if ( !boss->GetActive() || IsMonsterDead(boss) )
		{
			m_bossPoisonProjectileEffect.spellCastStates.erase(boss);
			continue;
		}

		CAnimatorComponent* animComp =
			boss->GetComponent<CAnimatorComponent>();

		if ( !animComp )
		{
			m_bossPoisonProjectileEffect.spellCastStates.erase(boss);
			continue;
		}

		CMonsterAnimController* ctrl =
			animComp->EnsureMonsterController();

		if ( !ctrl )
		{
			m_bossPoisonProjectileEffect.spellCastStates.erase(boss);
			continue;
		}

		const bool isSpellPhase = ctrl->IsSpellPhase();

		BossPoisonSpellCastState& state =
			m_bossPoisonProjectileEffect.spellCastStates[boss];

		if ( !isSpellPhase )
		{
			state = BossPoisonSpellCastState{};
			continue;
		}

		if ( !state.wasSpellPhase )
		{
			state.wasSpellPhase = true;
			state.pendingFire = true;
			state.fired = false;
			state.spellAgeSec = 0.0f;
		}
		else
		{
			state.spellAgeSec += dt;
		}

		if ( state.pendingFire &&
			!state.fired &&
			state.spellAgeSec >= kBossPoisonProjectileLaunchDelaySec )
		{
			SpawnBossPoisonProjectile(boss);

			state.fired = true;
			state.pendingFire = false;
		}
	}
#else
	UNREFERENCED_PARAMETER(dt);
#endif
}

void CGameScene::SpawnBossPoisonProjectileDust(BossPoisonProjectileEntry& entry)
{
#ifndef USING_NETWORK
	if ( !entry.active )
		return;

	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> unitDist(-1.0f, 1.0f);
	static std::uniform_real_distribution<float> speedDist(
		kBossPoisonDustMinScatterSpeed,
		kBossPoisonDustMaxScatterSpeed
	);
	static std::uniform_real_distribution<float> lifeDist(
		kBossPoisonDustMinLifetimeSec,
		kBossPoisonDustMaxLifetimeSec
	);
	static std::uniform_real_distribution<float> sizeDist(
		kBossPoisonDustMinSize,
		kBossPoisonDustMaxSize
	);
	static std::uniform_real_distribution<float> alphaDist(0.86f, 1.00f);
	static std::uniform_real_distribution<float> rotDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);

	XMVECTOR forward = XMLoadFloat3(&entry.direction);
	forward = XMVectorSetY(forward, 0.0f);

	if ( XMVectorGetX(XMVector3LengthSq(forward)) <= 1.0e-8f )
		forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	else
		forward = XMVector3Normalize(forward);

	const XMVECTOR projectileVelocity = XMLoadFloat3(&entry.velocity);
	const XMVECTOR inheritedVelocity =
		XMVectorScale(
			projectileVelocity,
			kBossPoisonDustProjectileVelocityInherit
		);

	for ( UINT i = 0; i < kBossPoisonDustParticlesPerEmit; ++i )
	{
		MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashEffect.entries);

		if ( !e )
			return;

		XMVECTOR scatterDir = XMVectorZero();

		for ( int attempt = 0; attempt < 8; ++attempt )
		{
			scatterDir = XMVectorSet(
				unitDist(rng),
				unitDist(rng),
				unitDist(rng),
				0.0f
			);

			if ( XMVectorGetX(XMVector3LengthSq(scatterDir)) > 1.0e-6f )
				break;
		}

		if ( XMVectorGetX(XMVector3LengthSq(scatterDir)) <= 1.0e-6f )
		{
			scatterDir = XMVectorNegate(forward);
		}
		else
		{
			scatterDir = XMVector3Normalize(scatterDir);

			const float dotForward =
				XMVectorGetX(XMVector3Dot(scatterDir, forward));

			if ( dotForward > 0.0f )
				scatterDir = XMVectorNegate(scatterDir);
		}

		const float scatterSpeed = speedDist(rng);

		XMVECTOR vel =
			XMVectorAdd(
				inheritedVelocity,
				XMVectorScale(scatterDir, scatterSpeed)
			);

		XMFLOAT3 vel3{};
		XMStoreFloat3(&vel3, vel);

		XMVECTOR pos =
			XMLoadFloat3(&entry.position) +
			XMVectorScale(scatterDir, kBossPoisonDustSpawnOffsetRadius);

		XMFLOAT3 pos3{};
		XMStoreFloat3(&pos3, pos);

		const float size = sizeDist(rng);
		const float alpha = alphaDist(rng);
		const float life = lifeDist(rng);

		e->active = true;
		e->kind = EMuzzleFlashKind::PoisonDust;

		e->position = pos3;
		e->velocity = vel3;

		e->age = 0.0f;
		e->lifetime = life;

		const float startAspectX = 0.85f + unitDist(rng) * 0.18f;
		const float startAspectY = 0.80f + unitDist(rng) * 0.20f;

		e->startWidth = size * startAspectX;
		e->startHeight = size * startAspectY;

		const float endScaleX = 2.10f + unitDist(rng) * 0.35f;
		const float endScaleY = 1.95f + unitDist(rng) * 0.35f;

		e->endWidth = size * endScaleX;
		e->endHeight = size * endScaleY;

		e->rotationRad = rotDist(rng);

		e->intensity = 0.72f + unitDist(rng) * 0.12f;

		e->drag = kBossPoisonDustDrag;
		e->gravity = kBossPoisonDustGravity;
		e->seed = seedDist(rng);

		e->color = XMFLOAT4(0.015f, 0.24f, 0.020f, alpha);
	}
#else
	UNREFERENCED_PARAMETER(entry);
#endif
}

void CGameScene::SpawnBossPoisonProjectile(CGameObject* boss)
{
#ifndef USING_NETWORK
	if ( !boss )
		return;

	BossPoisonProjectileEntry* entry =
		AcquireFreeBossPoisonProjectileEntry();

	if ( !entry )
	{
		return;
	}

	const XMFLOAT3 bossPos = boss->GetPosition();
	const XMFLOAT4X4& world = boss->GetWorldMatrix();

	XMFLOAT3 dir = XMFLOAT3(world._31, 0.0f, world._33);

	float lenSq = dir.x * dir.x + dir.z * dir.z;

	if ( lenSq <= 1.0e-8f )
	{
		dir = XMFLOAT3(0.0f, 0.0f, 1.0f);
		lenSq = 1.0f;
	}

	const float invLen = 1.0f / sqrtf(lenSq);

	dir.x *= invLen;
	dir.y = 0.0f;
	dir.z *= invLen;

	XMFLOAT3 spawnPos = bossPos;
	spawnPos.y += kBossPoisonProjectileLaunchHeight;
	spawnPos.x += dir.x * kBossPoisonProjectileForwardOffset;
	spawnPos.z += dir.z * kBossPoisonProjectileForwardOffset;

	entry->active = true;
	entry->owner = boss;

	entry->position = spawnPos;
	entry->direction = dir;

	entry->speed = kBossPoisonProjectileSpeed;
	entry->velocity = XMFLOAT3(
		dir.x * entry->speed,
		0.0f,
		dir.z * entry->speed
	);

	entry->coreDiameter = kBossPoisonProjectileCoreDiameter;
	entry->coreRadius = kBossPoisonProjectileCoreRadius;
	entry->gasDiameter = kBossPoisonProjectileGasDiameter;

	static UINT s_bossPoisonProjectileVisualSeed = 1;
	entry->visualSeed = static_cast< float >( s_bossPoisonProjectileVisualSeed++ );

	entry->dustEmitAccumulatorSec = 0.0f;

	entry->hitPlayerSlots.fill(false);

	PlayBossSpellSfxAt(spawnPos);
#else
	UNREFERENCED_PARAMETER(boss);
#endif
}

void CGameScene::UpdateBossPoisonProjectiles(float dt)
{
#ifndef USING_NETWORK
	if ( dt <= 0.0f )
		return;

	const int bossMegaGridNumber = 5;
	const int zeroBased = bossMegaGridNumber - 1;

	const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
	const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

	const float centerX =
		static_cast< float >(
			CSceneGrid::kGridMinX +
			megaX * CSceneGrid::kMegaGridCellWidth +
			CSceneGrid::kMegaGridCellWidth / 2
		);

	const float centerZ =
		static_cast< float >(
			CSceneGrid::kGridMinZ +
			megaZ * CSceneGrid::kMegaGridCellHeight +
			CSceneGrid::kMegaGridCellHeight / 2
		);

	const float minX =
		centerX - kBossPoisonProjectileStageHalfExtent;

	const float maxX =
		centerX + kBossPoisonProjectileStageHalfExtent;

	const float minZ =
		centerZ - kBossPoisonProjectileStageHalfExtent;

	const float maxZ =
		centerZ + kBossPoisonProjectileStageHalfExtent;

	for ( BossPoisonProjectileEntry& entry : m_bossPoisonProjectileEffect.entries )
	{
		if ( !entry.active )
			continue;

		entry.position.x += entry.velocity.x * dt;
		entry.position.y += entry.velocity.y * dt;
		entry.position.z += entry.velocity.z * dt;

		ApplyBossPoisonProjectilePlayerHits(entry);

		entry.dustEmitAccumulatorSec += dt;

		int dustEmitLoopGuard = 0;

		while ( entry.dustEmitAccumulatorSec >= kBossPoisonDustEmitIntervalSec &&
				dustEmitLoopGuard < 3 )
		{
			entry.dustEmitAccumulatorSec -= kBossPoisonDustEmitIntervalSec;
			SpawnBossPoisonProjectileDust(entry);

			++dustEmitLoopGuard;
		}

		if ( entry.position.x <= minX + entry.coreRadius ||
			 entry.position.x >= maxX - entry.coreRadius ||
			 entry.position.z <= minZ + entry.coreRadius ||
			 entry.position.z >= maxZ - entry.coreRadius )
		{
			entry = BossPoisonProjectileEntry{};
		}
	}
#else
	UNREFERENCED_PARAMETER(dt);
#endif
}

void CGameScene::RenderBossPoisonProjectiles(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_bossPoisonProjectileEffect.shader ) return;
	if ( !m_itemBillboardState.quadMesh ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kSceneBatchFrameResourceCount;

	ID3D12Resource* instanceBuffer = m_bossPoisonProjectileEffect.instanceBuffer.Resource(frameIndex);
	MuzzleFlashInstanceVertex* mappedInstanceBuffer = m_bossPoisonProjectileEffect.instanceBuffer.Mapped(frameIndex);
	const UINT bossPoisonProjectileInstanceBufferCapacity = m_bossPoisonProjectileEffect.instanceBuffer.Capacity();

	if ( !instanceBuffer ) return;
	if ( !mappedInstanceBuffer ) return;
	if ( bossPoisonProjectileInstanceBufferCapacity == 0 ) return;
	if ( m_itemBillboardState.quadMesh->m_SubMeshes.empty() ) return;

	const SubMesh& sm = m_itemBillboardState.quadMesh->m_SubMeshes[0];

	if ( sm.indices.empty() )
		return;

	const XMFLOAT3 cameraPos = camera->GetPosition();

	std::vector<const BossPoisonProjectileEntry*> visibleProjectiles;
	visibleProjectiles.reserve(m_bossPoisonProjectileEffect.entries.size());

	for ( const BossPoisonProjectileEntry& entry : m_bossPoisonProjectileEffect.entries )
	{
		if ( entry.active )
			visibleProjectiles.push_back(&entry);
	}

	if ( visibleProjectiles.empty() )
		return;

	std::sort(
		visibleProjectiles.begin(),
		visibleProjectiles.end(),
		[ &cameraPos ] (const BossPoisonProjectileEntry* a, const BossPoisonProjectileEntry* b)
		{
			const float adx = a->position.x - cameraPos.x;
			const float ady = a->position.y - cameraPos.y;
			const float adz = a->position.z - cameraPos.z;

			const float bdx = b->position.x - cameraPos.x;
			const float bdy = b->position.y - cameraPos.y;
			const float bdz = b->position.z - cameraPos.z;

			const float aDistSq = adx * adx + ady * ady + adz * adz;
			const float bDistSq = bdx * bdx + bdy * bdy + bdz * bdz;

			return aDistSq > bDistSq;
		}
	);

	UINT visibleInstanceCount = 0;

	for ( const BossPoisonProjectileEntry* entry : visibleProjectiles )
	{
		if ( !entry )
			continue;

		if ( visibleInstanceCount >= bossPoisonProjectileInstanceBufferCapacity )
			break;

		MuzzleFlashInstanceVertex& dst =
			mappedInstanceBuffer[visibleInstanceCount];

		StoreMuzzleFlashWorldRows(
			dst,
			entry->position,
			entry->gasDiameter,
			entry->gasDiameter,
			cameraPos
		);

		dst.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		dst.params0 = XMFLOAT4(
			0.0f,
			entry->coreDiameter,
			entry->gasDiameter,
			entry->visualSeed
		);

		dst.params1 = XMFLOAT4(
			entry->coreRadius,
			entry->speed,
			0.0f,
			0.0f
		);

		++visibleInstanceCount;
	}

	if ( visibleInstanceCount == 0 )
		return;

	m_bossPoisonProjectileEffect.shader->Render(cmd, camera, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
	vbViews[0] = sm.vbView;

	vbViews[1].BufferLocation =
		instanceBuffer->GetGPUVirtualAddress();

	vbViews[1].SizeInBytes =
		sizeof(MuzzleFlashInstanceVertex) * visibleInstanceCount;

	vbViews[1].StrideInBytes =
		sizeof(MuzzleFlashInstanceVertex);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 2, vbViews);
	cmd->IASetIndexBuffer(&sm.ibView);

	cmd->DrawIndexedInstanced(
		static_cast< UINT >( sm.indices.size() ),
		visibleInstanceCount,
		0,
		0,
		0
	);
}

void CGameScene::RenderSwordTrails( ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_swordTrailEffect.shader ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kSceneBatchFrameResourceCount;

	ID3D12Resource* swordTrailVertexBuffer = m_swordTrailEffect.vertexBuffer.Resource(frameIndex);
	SwordTrailVertex* mappedSwordTrailVertexBuffer = m_swordTrailEffect.vertexBuffer.Mapped(frameIndex);
	const UINT swordTrailVertexBufferCapacity = m_swordTrailEffect.vertexBuffer.Capacity();

	if ( !swordTrailVertexBuffer ) return;
	if ( !mappedSwordTrailVertexBuffer ) return;
	if ( swordTrailVertexBufferCapacity == 0 ) return;

	struct DrawRange
	{
		UINT startVertex = 0;
		UINT vertexCount = 0;
	};

	std::vector<DrawRange> drawRanges;
	drawRanges.reserve(m_swordTrailEffect.entries.size());

	UINT vertexCursor = 0;

	for ( const SwordTrailEntry& trail : m_swordTrailEffect.entries )
	{
		if ( !trail.active )
			continue;

		const size_t sampleCount = trail.samples.size();

		if ( sampleCount < 2 )
			continue;

		const UINT neededVertices =
			static_cast< UINT >(sampleCount * 2);

		if ( vertexCursor + neededVertices > swordTrailVertexBufferCapacity )
			break;

		float trailFade = 1.0f;

		const float sampleAge = trail.age - trail.startDelay;

		if ( sampleAge < 0.0f )
			continue;

		if ( sampleAge > trail.sampleDuration )
		{
			const float fadeAge = sampleAge - trail.sampleDuration;
			const float denom = ( trail.fadeDuration > 1.0e-6f ) ? trail.fadeDuration : 1.0e-6f;

			trailFade = 1.0f - std::clamp( fadeAge / denom, 0.0f, 1.0f);
		}

		const UINT startVertex = vertexCursor;

		for ( size_t i = 0; i < sampleCount; ++i )
		{
			const float u = ( sampleCount > 1 ) ? static_cast< float >( i ) / static_cast< float >( sampleCount - 1 ) : 1.0f;

			const float ageAlpha = std::clamp(u, 0.0f, 1.0f);
			const float alpha = trailFade * ageAlpha * 0.75f;

			const XMFLOAT4 color =
				XMFLOAT4(
					trail.color.x,
					trail.color.y,
					trail.color.z,
					alpha * trail.color.w
				);

			const SwordTrailSample& sample = trail.samples[i];

			SwordTrailVertex& v0 =
				mappedSwordTrailVertexBuffer[vertexCursor++];

			v0.position = sample.root;
			v0.uv = XMFLOAT2(u, 0.0f);
			v0.color = color;

			SwordTrailVertex& v1 =
				mappedSwordTrailVertexBuffer[vertexCursor++];

			v1.position = sample.tip;
			v1.uv = XMFLOAT2(u, 1.0f);
			v1.color = color;
		}

		DrawRange range{};
		range.startVertex = startVertex;
		range.vertexCount = neededVertices;
		drawRanges.push_back(range);
	}

	if ( drawRanges.empty() ) return;

	m_swordTrailEffect.shader->Render(cmd, camera, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView{};
	vbView.BufferLocation =
		swordTrailVertexBuffer->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeof(SwordTrailVertex) * vertexCursor;
	vbView.StrideInBytes = sizeof(SwordTrailVertex);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cmd->IASetVertexBuffers(0, 1, &vbView);
	cmd->IASetIndexBuffer(nullptr);

	for ( const DrawRange& range : drawRanges )
	{
		if ( range.vertexCount < 4 ) continue;

		cmd->DrawInstanced(range.vertexCount, 1, range.startVertex, 0);
	}
}

void CGameScene::RenderBossCallSummonWwwEffects(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
#ifndef USING_NETWORK
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_bossCallSummonWwwEffect.shader ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kSceneBatchFrameResourceCount;

	ID3D12Resource* vertexBuffer = m_bossCallSummonWwwEffect.vertexBuffer.Resource(frameIndex);
	SwordTrailVertex* mappedVertexBuffer = m_bossCallSummonWwwEffect.vertexBuffer.Mapped(frameIndex);
	const UINT bossCallSummonWwwVertexBufferCapacity = m_bossCallSummonWwwEffect.vertexBuffer.Capacity();

	if ( !vertexBuffer ) return;
	if ( !mappedVertexBuffer ) return;
	if ( bossCallSummonWwwVertexBufferCapacity == 0 ) return;

	struct DrawRange
	{
		UINT startVertex = 0;
		UINT vertexCount = 0;
	};

	std::vector<DrawRange> fillRanges;
	std::vector<DrawRange> outlineRanges;

	fillRanges.reserve(m_bossCallSummonWwwEffect.entries.size());
	outlineRanges.reserve(m_bossCallSummonWwwEffect.entries.size());

	UINT vertexCursor = 0;

	auto Smooth01 =
		[ ] (float x) -> float
		{
			x = std::clamp(x, 0.0f, 1.0f);
			return x * x * ( 3.0f - 2.0f * x );
		};

	auto ComputePoint =
		[ ](
			const BossCallSummonWwwEntry& entry,
			UINT wrappedPoint
		) -> XMFLOAT3
		{
			const UINT basePointCount =
				kBossCallSummonWwwPeakCount * 2;

			wrappedPoint %= basePointCount;

			const bool isPeak =
				( wrappedPoint & 1u ) != 0u;

			const UINT peakIndex =
				( wrappedPoint / 2u ) %
				kBossCallSummonWwwPeakCount;

			const float angle =
				XM_2PI *
				static_cast< float >( wrappedPoint ) /
				static_cast< float >( basePointCount );

			const float cx = std::cos(angle);
			const float sz = std::sin(angle);

			const float y =
				isPeak
				? entry.peakHeights[peakIndex]
				: 0.0f;

				XMFLOAT3 p{};
				p.x = entry.center.x + cx * entry.radius;
				p.y = entry.center.y + y;
				p.z = entry.center.z + sz * entry.radius;

				return p;
		};

	for ( const BossCallSummonWwwEntry& entry : m_bossCallSummonWwwEffect.entries )
	{
		if ( !entry.active )
			continue;

		const UINT neededFillVertices =
			kBossCallSummonWwwFillVertexCount;

		const UINT neededOutlineVertices =
			kBossCallSummonWwwOutlineVertexCount;

		const UINT neededVertices =
			neededFillVertices + neededOutlineVertices;

		if ( vertexCursor + neededVertices > bossCallSummonWwwVertexBufferCapacity )
		{
			break;
		}

		const float ageRatio =
			( entry.lifetime > 1.0e-6f )
			? std::clamp(entry.age / entry.lifetime, 0.0f, 1.0f)
			: 1.0f;

		const float birth =
			Smooth01(ageRatio / 0.12f);

		const float death =
			1.0f -
			Smooth01(( ageRatio - 0.62f ) / 0.38f);

		const float alpha =
			std::clamp(
				entry.color.w * birth * death,
				0.0f,
				1.0f
			);

		if ( alpha <= 0.002f )
			continue;

		const UINT basePointCount =
			kBossCallSummonWwwPeakCount * 2;

		// -----------------------------------------------------------------
		// 1) WWW 내부 채움.
		// 각 peak마다 bottom_i -> peak_i -> bottom_i+1 삼각형을 만든다.
		// RGB는 잔광과 동일하게 쓰고, additive 과노출 방지를 위해 alpha만 낮춘다.
		// -----------------------------------------------------------------
		{
			const UINT fillStartVertex = vertexCursor;

			const float fillAlpha =
				std::clamp(alpha * 0.38f, 0.0f, 1.0f);

			const XMFLOAT4 fillColor(
				entry.color.x,
				entry.color.y,
				entry.color.z,
				fillAlpha
			);

			for ( UINT peakIndex = 0;
				  peakIndex < kBossCallSummonWwwPeakCount;
				  ++peakIndex )
			{
				const UINT bottomAIndex =
					peakIndex * 2u;

				const UINT peakPointIndex =
					peakIndex * 2u + 1u;

				const UINT bottomBIndex =
					( peakIndex * 2u + 2u ) % basePointCount;

				const XMFLOAT3 bottomA =
					ComputePoint(entry, bottomAIndex);

				const XMFLOAT3 peak =
					ComputePoint(entry, peakPointIndex);

				const XMFLOAT3 bottomB =
					ComputePoint(entry, bottomBIndex);

				SwordTrailVertex& v0 =
					mappedVertexBuffer[vertexCursor++];

				v0.position = bottomA;
				v0.uv = XMFLOAT2(1.0f, 0.5f);
				v0.color = fillColor;

				SwordTrailVertex& v1 =
					mappedVertexBuffer[vertexCursor++];

				v1.position = peak;
				v1.uv = XMFLOAT2(1.0f, 0.5f);
				v1.color = fillColor;

				SwordTrailVertex& v2 =
					mappedVertexBuffer[vertexCursor++];

				v2.position = bottomB;
				v2.uv = XMFLOAT2(1.0f, 0.5f);
				v2.color = fillColor;
			}

			DrawRange fillRange{};
			fillRange.startVertex = fillStartVertex;
			fillRange.vertexCount = vertexCursor - fillStartVertex;

			if ( fillRange.vertexCount >= 3 )
				fillRanges.push_back(fillRange);
		}

		// -----------------------------------------------------------------
		// 2) WWW 외곽선.
		// 기존처럼 원 둘레를 따라 두께 있는 TRIANGLESTRIP을 만든다.
		// -----------------------------------------------------------------
		{
			const float halfThickness =
				std::max(0.035f, entry.radius * 0.030f);

			const UINT outlineStartVertex = vertexCursor;

			const XMFLOAT4 outlineColor(
				entry.color.x,
				entry.color.y,
				entry.color.z,
				alpha
			);

			for ( UINT pointIndex = 0;
				  pointIndex < kBossCallSummonWwwPathPointCount;
				  ++pointIndex )
			{
				const UINT wrappedPoint =
					pointIndex % basePointCount;

				const float angle =
					XM_2PI *
					static_cast< float >(wrappedPoint) /
					static_cast< float >(basePointCount);

				const float cx = std::cos(angle);
				const float sz = std::sin(angle);

				const XMFLOAT3 radial(cx, 0.0f, sz);
				const XMFLOAT3 p =
					ComputePoint(entry, wrappedPoint);

				SwordTrailVertex& v0 =
					mappedVertexBuffer[vertexCursor++];

				v0.position = XMFLOAT3(
					p.x - radial.x * halfThickness,
					p.y,
					p.z - radial.z * halfThickness
				);

				v0.uv = XMFLOAT2(1.0f, 0.0f);
				v0.color = outlineColor;

				SwordTrailVertex& v1 =
					mappedVertexBuffer[vertexCursor++];

				v1.position = XMFLOAT3(
					p.x + radial.x * halfThickness,
					p.y,
					p.z + radial.z * halfThickness
				);

				v1.uv = XMFLOAT2(1.0f, 1.0f);
				v1.color = outlineColor;
			}

			DrawRange outlineRange{};
			outlineRange.startVertex = outlineStartVertex;
			outlineRange.vertexCount = vertexCursor - outlineStartVertex;

			if ( outlineRange.vertexCount >= 4 )
				outlineRanges.push_back(outlineRange);
		}
	}

	if ( fillRanges.empty() && outlineRanges.empty() )
		return;

	m_bossCallSummonWwwEffect.shader->Render(cmd, camera, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView{};
	vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeof(SwordTrailVertex) * vertexCursor;
	vbView.StrideInBytes = sizeof(SwordTrailVertex);

	cmd->IASetVertexBuffers(0, 1, &vbView);
	cmd->IASetIndexBuffer(nullptr);

	// 내부 면 먼저 그림.
	if ( !fillRanges.empty() )
	{
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for ( const DrawRange& range : fillRanges )
		{
			if ( range.vertexCount < 3 )
				continue;

			cmd->DrawInstanced(range.vertexCount, 1, range.startVertex, 0);
		}
	}

	// 외곽선은 나중에 그려서 WWW 윤곽이 살아나게 한다.
	if ( !outlineRanges.empty() )
	{
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		for ( const DrawRange& range : outlineRanges )
		{
			if ( range.vertexCount < 4 )
				continue;

			cmd->DrawInstanced(range.vertexCount, 1, range.startVertex, 0);
		}
	}
#else
	UNREFERENCED_PARAMETER(cmd);
	UNREFERENCED_PARAMETER(camera);
#endif
}

void CGameScene::RenderMonsterSwordTrails(
	ID3D12GraphicsCommandList* cmd,
	CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_monsterSwordTrailEffect.shader ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kSceneBatchFrameResourceCount;

	ID3D12Resource* vertexBuffer = m_monsterSwordTrailEffect.vertexBuffer.Resource(frameIndex);
	MonsterSwordTrailVertex* mappedVertexBuffer = m_monsterSwordTrailEffect.vertexBuffer.Mapped(frameIndex);
	const UINT monsterSwordTrailVertexBufferCapacity = m_monsterSwordTrailEffect.vertexBuffer.Capacity();

	if ( !vertexBuffer ) return;
	if ( !mappedVertexBuffer ) return;
	if ( monsterSwordTrailVertexBufferCapacity == 0 ) return;

	struct DrawRange
	{
		UINT startVertex = 0;
		UINT vertexCount = 0;
	};

	std::vector<DrawRange> drawRanges;
	drawRanges.reserve(m_monsterSwordTrailEffect.entries.size());

	UINT vertexCursor = 0;

	for ( const MonsterSwordTrailEntry& trail : m_monsterSwordTrailEffect.entries )
	{
		if ( !trail.active )
			continue;

		const size_t sampleCount = trail.samples.size();

		if ( sampleCount < 2 )
			continue;

		const UINT neededVertices =
			static_cast< UINT >(sampleCount * 2);

		if ( vertexCursor + neededVertices > monsterSwordTrailVertexBufferCapacity )
			break;

		float trailFade = 1.0f;

		const float sampleAge = trail.age - trail.startDelay;

		if ( sampleAge < 0.0f )
			continue;

		if ( sampleAge > trail.sampleDuration )
		{
			const float fadeAge = sampleAge - trail.sampleDuration;
			const float denom =
				( trail.fadeDuration > 1.0e-6f )
				? trail.fadeDuration
				: 1.0e-6f;

			trailFade =
				1.0f - std::clamp(fadeAge / denom, 0.0f, 1.0f);
		}

		const UINT startVertex = vertexCursor;

		for ( size_t i = 0; i < sampleCount; ++i )
		{
			const float u =
				( sampleCount > 1 )
				? static_cast< float >( i ) /
				static_cast< float >( sampleCount - 1 )
				: 1.0f;

			const float ageAlpha = std::clamp(u, 0.0f, 1.0f);
			const float alpha = trailFade * ageAlpha * 0.75f;

			const XMFLOAT4 color =
				XMFLOAT4(
					trail.color.x,
					trail.color.y,
					trail.color.z,
					alpha * trail.color.w
				);

			const MonsterSwordTrailSample& sample = trail.samples[i];

			MonsterSwordTrailVertex& v0 =
				mappedVertexBuffer[vertexCursor++];

			v0.position = sample.root;
			v0.uv = XMFLOAT2(u, 0.0f);
			v0.color = color;

			MonsterSwordTrailVertex& v1 =
				mappedVertexBuffer[vertexCursor++];

			v1.position = sample.tip;
			v1.uv = XMFLOAT2(u, 1.0f);
			v1.color = color;
		}

		DrawRange range{};
		range.startVertex = startVertex;
		range.vertexCount = neededVertices;
		drawRanges.push_back(range);
	}

	if ( drawRanges.empty() ) return;

	m_monsterSwordTrailEffect.shader->Render(cmd, camera, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView{};
	vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeof(MonsterSwordTrailVertex) * vertexCursor;
	vbView.StrideInBytes = sizeof(MonsterSwordTrailVertex);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cmd->IASetVertexBuffers(0, 1, &vbView);
	cmd->IASetIndexBuffer(nullptr);

	for ( const DrawRange& range : drawRanges )
	{
		if ( range.vertexCount < 4 ) continue;

		cmd->DrawInstanced(range.vertexCount, 1, range.startVertex, 0);
	}
}

void CGameScene::RenderArrowTrails(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_arrowTrailEffect.shader ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kSceneBatchFrameResourceCount;

	ID3D12Resource* vertexBuffer = m_arrowTrailEffect.vertexBuffer.Resource(frameIndex);
	SwordTrailVertex* mappedVertexBuffer = m_arrowTrailEffect.vertexBuffer.Mapped(frameIndex);
	const UINT arrowTrailVertexBufferCapacity = m_arrowTrailEffect.vertexBuffer.Capacity();

	if ( !vertexBuffer ) return;
	if ( !mappedVertexBuffer ) return;
	if ( arrowTrailVertexBufferCapacity == 0 ) return;

	struct DrawRange
	{
		UINT startVertex = 0;
		UINT vertexCount = 0;
	};

	std::vector<DrawRange> drawRanges;
	drawRanges.reserve(m_arrowTrailEffect.entries.size());

	UINT vertexCursor = 0;

	const XMFLOAT3 cameraPos = camera->GetPosition();

	constexpr float kArrowTrailSampleLifetimeSec = 0.260f;
	constexpr float kArrowTrailHalfWidth = 0.075f;

	for ( const ArrowTrailEntry& trail : m_arrowTrailEffect.entries )
	{
		if ( !trail.active )
			continue;

		const size_t sampleCount = trail.samples.size();

		if ( sampleCount < 2 )
			continue;

		const UINT neededVertices =
			static_cast< UINT >(sampleCount * 2);

		if ( vertexCursor + neededVertices > arrowTrailVertexBufferCapacity )
			break;

		const UINT startVertex = vertexCursor;

		for ( size_t i = 0; i < sampleCount; ++i )
		{
			const float u =
				( sampleCount > 1 )
				? static_cast< float >( i ) /
				static_cast< float >( sampleCount - 1 )
				: 1.0f;

			const ArrowTrailSample& sample = trail.samples[i];

			const XMFLOAT3& prevPos =
				trail.samples[
					( i > 0 ) ? i - 1 : i
				].position;

			const XMFLOAT3& nextPos =
				trail.samples[
					( i + 1 < sampleCount ) ? i + 1 : i
				].position;

			XMVECTOR dir =
				XMLoadFloat3(&nextPos) -
				XMLoadFloat3(&prevPos);

			if ( XMVectorGetX(XMVector3LengthSq(dir)) <= 1.0e-8f )
				dir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			else
				dir = XMVector3Normalize(dir);

			XMVECTOR posV = XMLoadFloat3(&sample.position);
			XMVECTOR viewDir = XMLoadFloat3(&cameraPos) - posV;

			if ( XMVectorGetX(XMVector3LengthSq(viewDir)) <= 1.0e-8f )
				viewDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			else
				viewDir = XMVector3Normalize(viewDir);

			XMVECTOR side = XMVector3Cross(viewDir, dir);

			if ( XMVectorGetX(XMVector3LengthSq(side)) <= 1.0e-8f )
			{
				const XMVECTOR up =
					XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

				side = XMVector3Cross(up, dir);
			}

			if ( XMVectorGetX(XMVector3LengthSq(side)) <= 1.0e-8f )
				side = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
			else
				side = XMVector3Normalize(side);

			const float ageFade =
				1.0f -
				std::clamp(
					sample.age / kArrowTrailSampleLifetimeSec,
					0.0f,
					1.0f
				);

			// 꼬리 쪽은 약하고, 화살 현재 위치에 가까울수록 선명하게.
			const float headFade =
				std::clamp(u, 0.0f, 1.0f);

			const float alpha =
				ageFade *
				( 0.20f + headFade * 0.80f ) *
				0.72f;

			const XMFLOAT4 color =
				XMFLOAT4(
					0.86f,
					0.94f,
					1.0f,
					alpha
				);

			const XMVECTOR offset =
				XMVectorScale(side, kArrowTrailHalfWidth);

			XMFLOAT3 p0{};
			XMFLOAT3 p1{};

			XMStoreFloat3(&p0, posV - offset);
			XMStoreFloat3(&p1, posV + offset);

			SwordTrailVertex& v0 =
				mappedVertexBuffer[vertexCursor++];

			v0.position = p0;
			v0.uv = XMFLOAT2(u, 0.0f);
			v0.color = color;

			SwordTrailVertex& v1 =
				mappedVertexBuffer[vertexCursor++];

			v1.position = p1;
			v1.uv = XMFLOAT2(u, 1.0f);
			v1.color = color;
		}

		DrawRange range{};
		range.startVertex = startVertex;
		range.vertexCount = neededVertices;
		drawRanges.push_back(range);
	}

	if ( drawRanges.empty() )
		return;

	m_arrowTrailEffect.shader->Render(cmd, camera, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView{};
	vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeof(SwordTrailVertex) * vertexCursor;
	vbView.StrideInBytes = sizeof(SwordTrailVertex);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	cmd->IASetVertexBuffers(0, 1, &vbView);
	cmd->IASetIndexBuffer(nullptr);

	for ( const DrawRange& range : drawRanges )
	{
		if ( range.vertexCount < 4 )
			continue;

		cmd->DrawInstanced(range.vertexCount, 1, range.startVertex, 0);
	}
}