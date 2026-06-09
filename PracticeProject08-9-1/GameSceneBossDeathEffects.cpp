//-----------------------------------------------------------------------------
// File: GameSceneBossDeathEffects.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"

namespace
{
	static float BossDeathSaturate(float x)
	{
		return std::clamp(x, 0.0f, 1.0f);
	}

	static float BossDeathSmooth01(float x)
	{
		x = BossDeathSaturate(x);
		return x * x * ( 3.0f - 2.0f * x );
	}

	static float BossDeathRandRange(std::mt19937& rng, float minValue, float maxValue)
	{
		std::uniform_real_distribution<float> dist(minValue, maxValue);
		return dist(rng);
	}

	static MuzzleFlashEntry* AcquireFreeBossDeathParticleEntry(std::vector<MuzzleFlashEntry>& entries)
	{
		for ( MuzzleFlashEntry& entry : entries )
		{
			if ( !entry.active )
				return &entry;
		}

		return nullptr;
	}
}

void CGameScene::SetBossDeathCircleAlpha(float alpha)
{
	if ( !m_pMaterials )
		return;

	alpha = std::clamp(alpha, 0.0f, 1.0f);

	MATERIAL& mat = m_pMaterials->m_pReflections[kBossDeathCircleMaterialId];
	mat.m_xmf4Diffuse.w = alpha * 0.72f;
}

void CGameScene::SetBossDeathRingAlpha(float alpha)
{
	if ( !m_pMaterials )
		return;

	alpha = std::clamp(alpha, 0.0f, 1.0f);

	MATERIAL& mat = m_pMaterials->m_pReflections[kBossDeathRingMaterialId];
	mat.m_xmf4Diffuse.w = alpha;
}

void CGameScene::ClearBossDeathVisualBillboards()
{
	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( item.kind != EItemBillboardKind::BossDeathCircle && item.kind != EItemBillboardKind::BossDeathRing )
			continue;

		item.active = false;
		item.distanceCulled = true;
		item.width = 0.0f;
		item.height = 0.0f;
	}

	SetBossDeathCircleAlpha(0.0f);
	SetBossDeathRingAlpha(0.0f);
}

void CGameScene::SetBossDeathRendererVisible(CGameObject* boss, bool visible)
{
	if ( !boss )
		return;

	if ( auto* renderer = boss->GetComponent<CSkinnedMeshRendererComponent>() )
		renderer->SetEnabled(visible);

	if ( !visible )
	{
		if ( auto* animator = boss->GetComponent<CAnimatorComponent>() )
			animator->SetPoseEvaluationEnabled(false);
	}

	SetBossStageBossRenderAllowed(boss, visible);
}

void CGameScene::BeginBossDeathEffect(CGameObject* boss)
{
	if ( !boss )
		return;

	ClearBossDeathVisualBillboards();

	m_bossDeathEffect = BossDeathEffectState{};
	m_bossDeathEffect.active = true;
	m_bossDeathEffect.boss = boss;
	m_bossDeathEffect.originalPosition = boss->GetPosition();
	m_bossDeathEffect.groundCenter = AlignPositionYToTerrainGround(boss->GetPosition(), 0.0f);

	SetBossDeathCircleAlpha(0.0f);
	SetBossDeathRingAlpha(0.0f);

	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( item.kind == EItemBillboardKind::BossDeathCircle )
		{
			item.active = true;
			item.distanceCulled = false;
			item.transparent = true;
			item.position = m_bossDeathEffect.groundCenter;
			item.width = kBossDeathCircleSize;
			item.height = kBossDeathCircleSize;
			item.yOffset = 0.060f;
			item.cullDistance = 1000000.0f;
			item.pickupRadius = 0.0f;
			item.pickupHeightTolerance = 0.0f;
			item.materialId = kBossDeathCircleMaterialId;
		}
		else if ( item.kind == EItemBillboardKind::BossDeathRing )
		{
			item.active = false;
			item.distanceCulled = true;
			item.transparent = true;
			item.position = m_bossDeathEffect.groundCenter;
			item.width = kBossDeathRingStartSize;
			item.height = kBossDeathRingStartSize;
			item.yOffset = 0.090f;
			item.cullDistance = 1000000.0f;
			item.pickupRadius = 0.0f;
			item.pickupHeightTolerance = 0.0f;
			item.materialId = kBossDeathRingMaterialId;
		}
	}
}

void CGameScene::UpdateBossDeathEffect(float dt)
{
	if ( !m_bossDeathEffect.active )
		return;

	if ( dt < 0.0f )
		dt = 0.0f;

	BossDeathEffectState& state = m_bossDeathEffect;
	state.ageSec += dt;

	CGameObject* boss = state.boss;

	if ( !boss )
	{
		ClearBossDeathVisualBillboards();
		m_bossDeathEffect = BossDeathEffectState{};
		return;
	}

	const float age = state.ageSec;
	const float circleBirth = BossDeathSmooth01(( age - kBossDeathCircleStartSec ) / kBossDeathCircleFadeInSec);
	const float circleDeath = 1.0f - BossDeathSmooth01(( age - kBossDeathCircleFadeOutStartSec ) / kBossDeathCircleFadeOutSec);
	const float circleAlpha = ( age >= kBossDeathCircleStartSec ) ? std::clamp(circleBirth * circleDeath, 0.0f, 1.0f) : 0.0f;
	const float ringT = BossDeathSmooth01(( age - kBossDeathBurstTimeSec ) / kBossDeathRingExpandSec);
	const float ringAlpha = ( age >= kBossDeathBurstTimeSec ) ? 1.0f - BossDeathSmooth01(( age - kBossDeathBurstTimeSec ) / kBossDeathRingFadeSec) : 0.0f;
	const float ringSize = kBossDeathRingStartSize + ( kBossDeathRingEndSize - kBossDeathRingStartSize ) * ringT;

	SetBossDeathCircleAlpha(circleAlpha);
	SetBossDeathRingAlpha(ringAlpha);

	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( item.kind == EItemBillboardKind::BossDeathCircle )
		{
			item.active = circleAlpha > 0.001f;
			item.distanceCulled = !item.active;
			item.position = state.groundCenter;
			item.width = kBossDeathCircleSize;
			item.height = kBossDeathCircleSize;
			item.yOffset = 0.060f;
			item.materialId = kBossDeathCircleMaterialId;
		}
		else if ( item.kind == EItemBillboardKind::BossDeathRing )
		{
			item.active = ringAlpha > 0.001f;
			item.distanceCulled = !item.active;
			item.position = state.groundCenter;
			item.width = ringSize;
			item.height = ringSize;
			item.yOffset = 0.090f;
			item.materialId = kBossDeathRingMaterialId;
		}
	}

	if ( !state.rendererHidden && age >= kBossDeathLiftStartSec )
	{
		const float liftT = BossDeathSmooth01(( age - kBossDeathLiftStartSec ) / kBossDeathLiftDurationSec);
		const float lift = kBossDeathMaxLift * liftT;

		if ( auto* terrainAttach = boss->GetComponent<CTerrainAttachComponent>() )
		{
			terrainAttach->SetHeightOffset(lift);
			terrainAttach->SnapToTerrain();
		}
		else
		{
			XMFLOAT3 liftedPosition = state.originalPosition;
			liftedPosition.y += lift;
			boss->SetPosition(liftedPosition);
		}
	}

	XMFLOAT3 bodyCenter = boss->GetPosition();
	bodyCenter.y += 2.25f;

	if ( age >= kBossDeathCircleStartSec && age <= kBossDeathCircleFadeOutStartSec )
	{
		state.glowEmitAccumulatorSec += dt;

		while ( state.glowEmitAccumulatorSec >= kBossDeathGlowEmitIntervalSec )
		{
			state.glowEmitAccumulatorSec -= kBossDeathGlowEmitIntervalSec;
			SpawnMagicCircleGlowParticle(state.groundCenter, kBossDeathCircleSize, circleAlpha, 0.85f, 0.85f, 0.75f, 0.80f);

			if ( age >= kBossDeathAbsorbStartSec && age <= kBossDeathRendererHideTimeSec + 0.12f )
				SpawnBossDeathAbsorbParticles(bodyCenter, std::max(circleAlpha, 0.55f), kBossDeathAbsorbParticlesPerEmit);
		}
	}

	if ( age >= kBossDeathSmokeStartSec && age <= kBossDeathEffectDurationSec - 0.20f )
	{
		state.smokeEmitAccumulatorSec += dt;

		while ( state.smokeEmitAccumulatorSec >= kBossDeathSmokeEmitIntervalSec )
		{
			state.smokeEmitAccumulatorSec -= kBossDeathSmokeEmitIntervalSec;
			SpawnBossDeathBlackSmoke(bodyCenter, 2, 0.85f, 0.85f);
		}
	}

	if ( !state.burstSpawned && age >= kBossDeathBurstTimeSec )
	{
		SpawnBossDeathGreenBurst(state.groundCenter);
		state.burstSpawned = true;
	}

	if ( !state.rendererHidden && age >= kBossDeathRendererHideTimeSec )
	{
		SetBossDeathRendererVisible(boss, false);
		state.rendererHidden = true;
	}

	if ( age >= kBossDeathEffectDurationSec )
	{
		SetBossDeathRendererVisible(boss, false);

		if ( auto* terrainAttach = boss->GetComponent<CTerrainAttachComponent>() )
		{
			terrainAttach->SetHeightOffset(kBossStageBossHiddenYOffset);
			terrainAttach->SnapToTerrain();
		}
		else
		{
			XMFLOAT3 hiddenPosition = state.originalPosition;
			hiddenPosition.y += kBossStageBossHiddenYOffset;
			boss->SetPosition(hiddenPosition);
		}

		boss->SetActive(false);
		ClearBossDeathVisualBillboards();
		m_bossDeathEffect = BossDeathEffectState{};
	}
}

void CGameScene::SpawnBossDeathAbsorbParticles(const XMFLOAT3& center, float alpha, int count)
{
	if ( alpha <= 0.001f || count <= 0 )
		return;

	static std::mt19937 rng{ std::random_device{}( ) };
	static std::uniform_real_distribution<float> angleDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);

	for ( int i = 0; i < count; ++i )
	{
		MuzzleFlashEntry* e = AcquireFreeBossDeathParticleEntry(m_muzzleFlashEffect.entries);

		if ( !e )
			return;

		const float angle = angleDist(rng);
		const float radius = BossDeathRandRange(rng, 0.65f, 2.80f);
		const float inwardSpeed = BossDeathRandRange(rng, 0.20f, 0.65f);
		const float upwardSpeed = BossDeathRandRange(rng, 2.20f, 4.25f);
		const float size = BossDeathRandRange(rng, 0.18f, 0.42f);

		e->active = true;
		e->kind = EMuzzleFlashKind::MagicCircleAfterimage;
		e->position = XMFLOAT3(center.x + cosf(angle) * radius, center.y + BossDeathRandRange(rng, -1.20f, 1.50f), center.z + sinf(angle) * radius);
		e->velocity = XMFLOAT3(-cosf(angle) * inwardSpeed, upwardSpeed, -sinf(angle) * inwardSpeed);
		e->age = 0.0f;
		e->lifetime = BossDeathRandRange(rng, 0.42f, 0.78f);
		e->startWidth = size;
		e->startHeight = size * BossDeathRandRange(rng, 1.25f, 2.15f);
		e->endWidth = size * 0.35f;
		e->endHeight = size * 0.65f;
		e->rotationRad = angle;
		e->intensity = BossDeathRandRange(rng, 1.10f, 1.75f);
		e->drag = BossDeathRandRange(rng, 0.25f, 0.70f);
		e->gravity = 0.0f;
		e->seed = seedDist(rng);
		e->color = XMFLOAT4(0.10f, 0.95f, 0.20f, std::clamp(alpha * BossDeathRandRange(rng, 0.55f, 0.92f), 0.0f, 1.0f));
	}
}

void CGameScene::SpawnBossDeathBlackSmoke(const XMFLOAT3& center, int count, float sizeScale, float upwardBias)
{
	if ( count <= 0 )
		return;

	static std::mt19937 rng{ std::random_device{}( ) };
	static std::uniform_real_distribution<float> angleDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);

	const float safeSizeScale = std::max(0.05f, sizeScale);

	for ( int i = 0; i < count; ++i )
	{
		MuzzleFlashEntry* e = AcquireFreeBossDeathParticleEntry(m_gunSmokeEffect.entries);

		if ( !e )
			return;

		const float angle = angleDist(rng);
		const float spawnRadius = BossDeathRandRange(rng, 0.10f, 1.25f);
		const float sideSpeed = BossDeathRandRange(rng, 0.15f, 1.05f);
		const float liftSpeed = BossDeathRandRange(rng, 0.40f, 1.35f) + upwardBias;
		const float baseSize = BossDeathRandRange(rng, 1.65f, 3.20f) * safeSizeScale;

		e->active = true;
		e->kind = EMuzzleFlashKind::GunSmoke;
		e->position = XMFLOAT3(center.x + cosf(angle) * spawnRadius, center.y + BossDeathRandRange(rng, -0.80f, 1.00f), center.z + sinf(angle) * spawnRadius);
		e->velocity = XMFLOAT3(cosf(angle) * sideSpeed, liftSpeed, sinf(angle) * sideSpeed);
		e->age = 0.0f;
		e->lifetime = BossDeathRandRange(rng, 0.95f, 1.55f);
		e->startWidth = baseSize * BossDeathRandRange(rng, 0.78f, 1.18f);
		e->startHeight = baseSize * BossDeathRandRange(rng, 0.75f, 1.25f);
		e->endWidth = baseSize * BossDeathRandRange(rng, 2.10f, 3.35f);
		e->endHeight = baseSize * BossDeathRandRange(rng, 1.90f, 3.05f);
		e->rotationRad = angleDist(rng);
		e->intensity = BossDeathRandRange(rng, 0.78f, 1.10f);
		e->drag = BossDeathRandRange(rng, 0.55f, 1.05f);
		e->gravity = BossDeathRandRange(rng, 0.00f, 0.16f);
		e->seed = seedDist(rng);
		e->color = XMFLOAT4(0.006f, 0.007f, 0.006f, BossDeathRandRange(rng, 0.38f, 0.64f));
	}
}

void CGameScene::SpawnBossDeathGreenBurst(const XMFLOAT3& center)
{
	static std::mt19937 rng{ std::random_device{}( ) };
	static std::uniform_real_distribution<float> angleDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);

	XMFLOAT3 bodyCenter = center;
	bodyCenter.y += 2.20f;

	for ( int i = 0; i < 9; ++i )
		SpawnMagicCircleGlowParticle(center, kBossDeathCircleSize, 1.0f, 1.15f, 1.15f, 1.05f, 0.90f);

	for ( int i = 0; i < 46; ++i )
	{
		MuzzleFlashEntry* e = AcquireFreeBossDeathParticleEntry(m_muzzleFlashEffect.entries);

		if ( !e )
			return;

		const float angle = angleDist(rng);
		const float speed = BossDeathRandRange(rng, 4.20f, 9.50f);
		const float lift = BossDeathRandRange(rng, 0.45f, 2.80f);
		const float size = BossDeathRandRange(rng, 0.26f, 0.72f);

		e->active = true;
		e->kind = EMuzzleFlashKind::MagicCircleAfterimage;
		e->position = XMFLOAT3(bodyCenter.x + BossDeathRandRange(rng, -0.65f, 0.65f), bodyCenter.y + BossDeathRandRange(rng, -1.15f, 1.25f), bodyCenter.z + BossDeathRandRange(rng, -0.65f, 0.65f));
		e->velocity = XMFLOAT3(cosf(angle) * speed, lift, sinf(angle) * speed);
		e->age = 0.0f;
		e->lifetime = BossDeathRandRange(rng, 0.32f, 0.74f);
		e->startWidth = size;
		e->startHeight = size * BossDeathRandRange(rng, 1.20f, 2.35f);
		e->endWidth = size * 0.25f;
		e->endHeight = size * 0.45f;
		e->rotationRad = angle;
		e->intensity = BossDeathRandRange(rng, 1.35f, 2.10f);
		e->drag = BossDeathRandRange(rng, 1.10f, 2.15f);
		e->gravity = BossDeathRandRange(rng, 0.00f, 0.35f);
		e->seed = seedDist(rng);
		e->color = XMFLOAT4(0.12f, 1.00f, 0.20f, BossDeathRandRange(rng, 0.72f, 1.00f));
	}

	SpawnBossDeathBlackSmoke(bodyCenter, 30, 1.35f, 1.20f);
}