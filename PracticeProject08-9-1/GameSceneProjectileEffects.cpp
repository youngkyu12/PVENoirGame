//-----------------------------------------------------------------------------
// File: GameSceneProjectileEffects.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"
#include "GameSceneBillboardCommon.h"

using namespace GameSceneBillboardCommon;

namespace
{
	static MuzzleFlashEntry* AcquireFreeMuzzleFlashEntry(std::vector<MuzzleFlashEntry>& flashes)
	{
		for ( MuzzleFlashEntry& flash : flashes )
		{
			if ( !flash.active )
				return &flash;
		}

		return nullptr;
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

void CGameScene::ReleaseBossPoisonProjectileGpuResources()
{
	m_bossPoisonProjectileEffect.instanceBuffer.Release();
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

	cmd->DrawIndexedInstanced( static_cast< UINT >( sm.indices.size() ), visibleInstanceCount, 0, 0, 0);
}
