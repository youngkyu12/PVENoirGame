//-----------------------------------------------------------------------------
// File: GameSceneBossShockwaveEffects.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"

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
}

void CGameScene::ApplyBossShockwavePushToLocalPlayer(float previousRadius, float currentRadius)
{
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
}