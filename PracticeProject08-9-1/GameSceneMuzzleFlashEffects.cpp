//-----------------------------------------------------------------------------
// File: GameSceneMuzzleFlashEffects.cpp
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

	static XMFLOAT3 GetBloodSplashFallbackPosition(const CGameObject* victim)
	{
		if ( !victim )
			return XMFLOAT3(0.0f, 0.0f, 0.0f);

		XMFLOAT3 p = victim->GetPosition();

		p.y += 1.0f;

		return p;
	}

	static XMVECTOR SafeNormalize3OrDefault(const XMFLOAT3* dir, const XMVECTOR& fallback)
	{
		if ( !dir )
			return fallback;

		XMVECTOR v = XMLoadFloat3(dir);

		if ( XMVectorGetX(XMVector3LengthSq(v)) <= 1.0e-8f )
			return fallback;

		return XMVector3Normalize(v);
	}

	static XMFLOAT3 GetWeaponFireworkOrigin(CGameObject* weaponObject)
	{
		if ( !weaponObject )
			return XMFLOAT3(0.0f, 0.0f, 0.0f);

		const XMFLOAT4X4& W = weaponObject->GetWorldMatrix();

		return XMFLOAT3(
			W._41,
			W._42 + 0.18f,
			W._43
		);
	}
}


void CGameScene::BuildMuzzleFlashBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_muzzleFlashEffect.shader = std::make_shared<CMuzzleFlashBillboardShader>();

	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_muzzleFlashEffect.shader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		1,
		&rtvFormat,
		dsvFormat
	);

	m_muzzleFlashEffect.entries.clear();
	m_muzzleFlashEffect.entries.resize(kMuzzleFlashMaxCount);

	m_muzzleFlashEffect.instanceBuffer.Create(dev, cmd, kMuzzleFlashMaxCount, [ dev, cmd ] (UINT bufferBytes)
	{
		return ::CreateBufferResource(dev, cmd, nullptr, bufferBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
	});
}

void CGameScene::BuildGunSmokeBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_gunSmokeEffect.shader = std::make_shared<CGunSmokeBillboardShader>();

	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_gunSmokeEffect.shader->CreateShader(dev, m_pd3dGraphicsRootSignature.Get(), 1, &rtvFormat, dsvFormat);

	m_gunSmokeEffect.entries.clear();
	m_gunSmokeEffect.entries.resize(kGunSmokeMaxCount);

	m_gunSmokeEffect.instanceBuffer.Create(dev, cmd, kGunSmokeMaxCount, [ dev, cmd ] (UINT bufferBytes)
	{
		return ::CreateBufferResource(dev, cmd, nullptr, bufferBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
	});
}

void CGameScene::ReleaseMuzzleFlashGpuResources()
{
	m_muzzleFlashEffect.instanceBuffer.Release();
}

void CGameScene::ReleaseGunSmokeGpuResources()
{
	m_gunSmokeEffect.instanceBuffer.Release();
}

void CGameScene::SpawnMuzzleFlash(const XMFLOAT3& position, const XMFLOAT3& direction)
{
	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> rotDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);
	static std::uniform_real_distribution<float> unitDist(-1.0f, 1.0f);
	static std::uniform_real_distribution<float> sparkSpeedBaseDist(2.2f, 4.8f);

	const PlayerGunMuzzleFlashVisualDesc& visual = GetPlayerGunMuzzleFlashVisualDesc();

	XMVECTOR dirV = XMLoadFloat3(&direction);
	if ( XMVectorGetX(XMVector3LengthSq(dirV)) <= 1.0e-8f )
		dirV = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	else
		dirV = XMVector3Normalize(dirV);

	const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMVECTOR right = XMVector3Cross(up, dirV);
	if ( XMVectorGetX(XMVector3LengthSq(right)) <= 1.0e-8f )
		right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	else
		right = XMVector3Normalize(right);

	auto spawnCore = [ & ] (const PlayerGunMuzzleFlashCoreVisualDesc& core, float life)
		{
			MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashEffect.entries);
			if ( !e ) return;

			e->active = true;
			e->kind = EMuzzleFlashKind::Core;
			e->position = position;
			e->velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

			e->age = 0.0f;
			e->lifetime = life;

			e->startWidth = core.size;
			e->startHeight = core.size;
			e->endWidth = core.size * core.endSizeScale;
			e->endHeight = core.size * core.endSizeScale;

			e->rotationRad = rotDist(rng);
			e->intensity = core.intensity;
			e->drag = 0.0f;
			e->gravity = 0.0f;
			e->seed = seedDist(rng);

			e->color = core.color;
		};

	auto spawnRing = [ & ] ()
		{
			MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashEffect.entries);
			if ( !e ) return;

			e->active = true;
			e->kind = EMuzzleFlashKind::Ring;
			e->position = position;
			e->velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

			e->age = 0.0f;
			e->lifetime = 0.06f;

			e->startWidth = visual.ring.startSize;
			e->startHeight = visual.ring.startSize;
			e->endWidth = visual.ring.endSize;
			e->endHeight = visual.ring.endSize;

			e->rotationRad = rotDist(rng);
			e->intensity = visual.ring.intensity;
			e->drag = 0.0f;
			e->gravity = 0.0f;
			e->seed = seedDist(rng);

			e->color = visual.ring.color;
		};

	auto spawnSpark = [ & ] (float baseRot)
		{
			MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashEffect.entries);
			if ( !e ) return;

			const float side = unitDist(rng) * visual.spark.sideScale;
			const float lift = unitDist(rng) * visual.spark.liftScale + visual.spark.liftBase;
			const float speed = sparkSpeedBaseDist(rng) * visual.spark.speedScale;

			XMVECTOR vel = XMVectorAdd(XMVectorScale(dirV, 1.0f), XMVectorAdd(XMVectorScale(right, side), XMVectorScale(up, lift)));

			if ( XMVectorGetX(XMVector3LengthSq(vel)) <= 1.0e-8f )
				vel = dirV;
			else
				vel = XMVector3Normalize(vel);

			vel = XMVectorScale(vel, speed);

			XMFLOAT3 vel3{};
			XMStoreFloat3(&vel3, vel);

			e->active = true;
			e->kind = EMuzzleFlashKind::Spark;
			e->position = position;
			e->velocity = vel3;

			e->age = 0.0f;
			e->lifetime = 0.07f;

			e->startWidth = visual.spark.startWidth;
			e->startHeight = visual.spark.startHeight;
			e->endWidth = visual.spark.endWidth;
			e->endHeight = visual.spark.endHeight;

			e->rotationRad = baseRot + unitDist(rng) * visual.spark.rotationJitter;
			e->intensity = visual.spark.intensity;
			e->drag = visual.spark.drag;
			e->gravity = 0.0f;
			e->seed = seedDist(rng);

			e->color = visual.spark.color;
		};

	spawnCore(visual.cores[0], 0.045f);
	spawnCore(visual.cores[1], 0.065f);

	spawnRing();

	for ( int i = 0; i < visual.spark.count; ++i )
	{
		spawnSpark(rotDist(rng));
	}

	SpawnGunSmoke(position, direction);
}

void CGameScene::SpawnGunSmoke(const XMFLOAT3& position, const XMFLOAT3& direction)
{
	static std::mt19937 rng{ std::random_device{}( ) };
	static std::uniform_real_distribution<float> zeroOneDist(0.0f, 1.0f);
	static std::uniform_real_distribution<float> unitDist(-1.0f, 1.0f);
	static std::uniform_real_distribution<float> rotDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);

	const PlayerGunMuzzleFlashVisualDesc& visual = GetPlayerGunMuzzleFlashVisualDesc();
	const PlayerGunSmokeVisualDesc& smoke = visual.smoke;

	if ( smoke.count <= 0 )
		return;

	XMVECTOR dirV = XMLoadFloat3(&direction);
	if ( XMVectorGetX(XMVector3LengthSq(dirV)) <= 1.0e-8f )
		dirV = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	else
		dirV = XMVector3Normalize(dirV);

	const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMVECTOR right = XMVector3Cross(up, dirV);
	if ( XMVectorGetX(XMVector3LengthSq(right)) <= 1.0e-8f )
		right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	else
		right = XMVector3Normalize(right);

	auto RandRange = [ & ] (float minValue, float maxValue)
		{
			return minValue + ( maxValue - minValue ) * zeroOneDist(rng);
		};

	for ( int i = 0; i < smoke.count; ++i )
	{
		MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_gunSmokeEffect.entries);
		if ( !e )
			return;

		const float spawnRightOffset = RandRange(smoke.spawnRightOffsetMin, smoke.spawnRightOffsetMax);
		const float spawnForwardJitter = unitDist(rng) * smoke.spawnForwardJitter;
		const float spawnUpJitter = unitDist(rng) * smoke.spawnUpJitter;

		XMVECTOR posV = XMLoadFloat3(&position) + XMVectorScale(right, spawnRightOffset) + XMVectorScale(dirV, spawnForwardJitter) + XMVectorScale(up, spawnUpJitter);

		XMFLOAT3 pos{};
		XMStoreFloat3(&pos, posV);

		const float rightSpeed = RandRange(smoke.rightSpeedMin, smoke.rightSpeedMax);
		const float forwardSpeed = RandRange(smoke.forwardSpeedMin, smoke.forwardSpeedMax);
		const float liftSpeed = RandRange(smoke.liftSpeedMin, smoke.liftSpeedMax);

		XMVECTOR velV = XMVectorScale(right, rightSpeed) + XMVectorScale(dirV, forwardSpeed) + XMVectorScale(up, liftSpeed);

		XMFLOAT3 vel{};
		XMStoreFloat3(&vel, velV);

		const float startSize = RandRange(smoke.startSizeMin, smoke.startSizeMax);
		const float endSizeScale = RandRange(smoke.endSizeScaleMin, smoke.endSizeScaleMax);
		const float endSize = startSize * endSizeScale;

		e->active = true;
		e->kind = EMuzzleFlashKind::GunSmoke;
		e->position = pos;
		e->velocity = vel;

		e->age = 0.0f;
		e->lifetime = RandRange(smoke.lifetimeMin, smoke.lifetimeMax);

		e->startWidth = startSize;
		e->startHeight = startSize * RandRange(0.78f, 1.12f);
		e->endWidth = endSize;
		e->endHeight = endSize * RandRange(0.86f, 1.20f);

		e->rotationRad = rotDist(rng);
		e->intensity = 1.0f;
		e->drag = smoke.drag;
		e->gravity = smoke.gravity;
		e->seed = seedDist(rng);
		e->color = smoke.color;
	}
}

void CGameScene::SpawnGoldFireworkBurstAtWeapon(CGameObject* weaponObject)
{
	if ( !weaponObject )
		return;

	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> zeroOneDist(0.0f, 1.0f);
	static std::uniform_real_distribution<float> unitDist(-1.0f, 1.0f);
	static std::uniform_real_distribution<float> rotDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);

	const XMFLOAT3 origin = GetWeaponFireworkOrigin(weaponObject);

	constexpr int kGoldFireworkParticleCount = 72;

	for ( int i = 0; i < kGoldFireworkParticleCount; ++i )
	{
		MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashEffect.entries);
		if ( !e )
			return;

		// 구 표면에 가까운 전방위 랜덤 방향.
		const float y = unitDist(rng);
		const float theta = zeroOneDist(rng) * XM_2PI;
		const float horizontalRadius =
			std::sqrt(std::max(0.0f, 1.0f - y * y));

		XMVECTOR dir = XMVectorSet(
			std::cos(theta) * horizontalRadius,
			y,
			std::sin(theta) * horizontalRadius,
			0.0f
		);

		if ( XMVectorGetX(XMVector3LengthSq(dir)) <= 1.0e-8f )
			dir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		else
			dir = XMVector3Normalize(dir);

		// 무기 중심에서 완전히 같은 점에 겹치지 않도록 아주 약간만 퍼뜨린다.
		const float spawnJitter = 0.04f + zeroOneDist(rng) * 0.08f;

		XMVECTOR posV =
			XMLoadFloat3(&origin) +
			XMVectorScale(dir, spawnJitter);

		XMFLOAT3 pos{};
		XMStoreFloat3(&pos, posV);

		// 너무 차이나지는 않게, 그러나 입자별 차이는 나게.
		const float speed = 3.20f + zeroOneDist(rng) * 2.20f;
		const float life = 0.72f + zeroOneDist(rng) * 0.42f;
		const float startSize = 0.16f + zeroOneDist(rng) * 0.12f;
		const float endSize = 0.025f + zeroOneDist(rng) * 0.025f;

		XMVECTOR velV = XMVectorScale(dir, speed);

		XMFLOAT3 vel{};
		XMStoreFloat3(&vel, velV);

		e->active = true;
		e->kind = EMuzzleFlashKind::GoldFirework;

		e->position = pos;
		e->velocity = vel;

		e->age = 0.0f;
		e->lifetime = life;

		// 시간이 지날수록 작아지게 한다.
		e->startWidth = startSize;
		e->startHeight = startSize;
		e->endWidth = endSize;
		e->endHeight = endSize;

		e->rotationRad = rotDist(rng);
		e->intensity = 1.15f + zeroOneDist(rng) * 0.45f;

		// 독가스 중력 수치(kBossPoisonDustGravity = 1.20f)를 기준으로 약간만 랜덤화.
		e->gravity = kBossPoisonDustGravity * ( 0.85f + zeroOneDist(rng) * 0.30f );

		// 공중에서 너무 멀리 날아가지 않도록 약한 감속.
		e->drag = 0.35f + zeroOneDist(rng) * 0.30f;

		e->seed = seedDist(rng);

		// 금빛 계열 안에서만 미세 랜덤.
		const float warm = zeroOneDist(rng);

		e->color = XMFLOAT4(
			1.00f,
			0.66f + warm * 0.16f,
			0.10f + warm * 0.12f,
			0.92f
		);
	}
}

void CGameScene::SpawnInventoryUseBurst(CGameObject* player, int inventorySlot)
{
#ifndef USING_NETWORK
	if ( !player )
		return;

	if ( inventorySlot < 0 || inventorySlot >= CGameSceneHUD::kInventorySlotCount )
		return;

	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> zeroOneDist(0.0f, 1.0f);
	static std::uniform_real_distribution<float> unitDist(-1.0f, 1.0f);
	static std::uniform_real_distribution<float> rotDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);

	const std::array<XMFLOAT4, CGameSceneHUD::kInventorySlotCount> colors =
	{
		XMFLOAT4(1.00f, 0.08f, 0.06f, 0.95f),
		XMFLOAT4(1.00f, 0.48f, 0.05f, 0.95f),
		XMFLOAT4(0.18f, 0.48f, 1.00f, 0.95f),
		XMFLOAT4(0.14f, 1.00f, 0.22f, 0.95f)
	};

	XMFLOAT3 origin = player->GetPosition();
	origin.y += 1.05f;

	const XMFLOAT4 color = colors[static_cast< size_t >( inventorySlot )];

	constexpr int kInventoryUseBurstParticleCount = 56;

	for ( int i = 0; i < kInventoryUseBurstParticleCount; ++i )
	{
		MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashEffect.entries);
		if ( !e )
			return;

		const float y = unitDist(rng);
		const float theta = zeroOneDist(rng) * XM_2PI;
		const float horizontalRadius = std::sqrt(std::max(0.0f, 1.0f - y * y));

		XMVECTOR dir = XMVectorSet(std::cos(theta) * horizontalRadius, y, std::sin(theta) * horizontalRadius, 0.0f);

		if ( XMVectorGetX(XMVector3LengthSq(dir)) <= 1.0e-8f )
			dir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		else
			dir = XMVector3Normalize(dir);

		const float spawnJitter = 0.10f + zeroOneDist(rng) * 0.18f;

		XMVECTOR posV = XMLoadFloat3(&origin) + XMVectorScale(dir, spawnJitter);

		XMFLOAT3 pos{};
		XMStoreFloat3(&pos, posV);

		const float speed = 3.60f + zeroOneDist(rng) * 2.70f;
		const float life = 0.62f + zeroOneDist(rng) * 0.34f;
		const float startSize = 0.14f + zeroOneDist(rng) * 0.11f;
		const float endSize = 0.020f + zeroOneDist(rng) * 0.025f;

		XMVECTOR velV = XMVectorScale(dir, speed);

		XMFLOAT3 vel{};
		XMStoreFloat3(&vel, velV);

		e->active = true;
		e->kind = EMuzzleFlashKind::GoldFirework;

		e->position = pos;
		e->velocity = vel;

		e->age = 0.0f;
		e->lifetime = life;

		e->startWidth = startSize;
		e->startHeight = startSize;
		e->endWidth = endSize;
		e->endHeight = endSize;

		e->rotationRad = rotDist(rng);
		e->intensity = 1.20f + zeroOneDist(rng) * 0.55f;

		e->gravity = 0.35f + zeroOneDist(rng) * 0.35f;
		e->drag = 0.25f + zeroOneDist(rng) * 0.25f;
		e->seed = seedDist(rng);
		e->color = color;
	}
#else
	UNREFERENCED_PARAMETER(player);
	UNREFERENCED_PARAMETER(inventorySlot);
#endif
}

void CGameScene::UpdateInventoryBuffAmbientParticles(float dt)
{
#ifndef USING_NETWORK
	if ( dt <= 0.0f )
		return;

	for ( int playerSlot = 0; playerSlot < 4; ++playerSlot )
	{
		CGameObject* player = GetPlayerBySlot(playerSlot);

		if ( !player || !player->GetActive() )
		{
			for ( int inventorySlot = 1; inventorySlot <= 3; ++inventorySlot )
				m_inventoryBuffParticleEmitAccumulators[static_cast< size_t >(playerSlot)][static_cast< size_t >(inventorySlot)] = 0.0f;

			continue;
		}

		CInventoryComponent* inventory = player->GetComponent<CInventoryComponent>();

		if ( !inventory )
			continue;

		const bool activeBuffs[4] =
		{
			false,
			inventory->IsAttackPowerPotionActive(),
			inventory->IsDefensePotionActive(),
			inventory->IsMoveSpeedPotionActive()
		};

		for ( int inventorySlot = 1; inventorySlot <= 3; ++inventorySlot )
		{
			float& accumulator = m_inventoryBuffParticleEmitAccumulators[static_cast< size_t >( playerSlot )][static_cast< size_t >( inventorySlot )];

			if ( !activeBuffs[inventorySlot] )
			{
				accumulator = 0.0f;
				continue;
			}

			EmitInventoryBuffAmbientParticles(player, inventorySlot, dt, accumulator);
		}
	}
#else
	UNREFERENCED_PARAMETER(dt);
#endif
}

void CGameScene::EmitInventoryBuffAmbientParticles(CGameObject* player, int inventorySlot, float dt, float& accumulatorSec)
{
#ifndef USING_NETWORK
	if ( !player )
		return;

	if ( inventorySlot < 1 || inventorySlot > 3 )
		return;

	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> zeroOneDist(0.0f, 1.0f);
	static std::uniform_real_distribution<float> unitDist(-1.0f, 1.0f);
	static std::uniform_real_distribution<float> rotDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);

	const std::array<XMFLOAT4, CGameSceneHUD::kInventorySlotCount> colors =
	{
		XMFLOAT4(1.00f, 0.08f, 0.06f, 0.82f),
		XMFLOAT4(1.00f, 0.48f, 0.05f, 0.72f),
		XMFLOAT4(0.18f, 0.48f, 1.00f, 0.72f),
		XMFLOAT4(0.14f, 1.00f, 0.22f, 0.72f)
	};

	static constexpr float kEmitIntervalSec = 0.085f;
	static constexpr int kParticlesPerEmit = 1;
	static constexpr int kMaxEmitsPerFrame = 3;

	accumulatorSec += dt;

	int emitCount = 0;

	while ( accumulatorSec >= kEmitIntervalSec && emitCount < kMaxEmitsPerFrame )
	{
		accumulatorSec -= kEmitIntervalSec;
		++emitCount;

		for ( int i = 0; i < kParticlesPerEmit; ++i )
		{
			MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashEffect.entries);
			if ( !e )
				return;

			const XMFLOAT3 playerPos = player->GetPosition();

			const float theta = zeroOneDist(rng) * XM_2PI;

			// 기존 0.45~1.15보다 플레이어 몸 주변에 더 붙여서 생성.
			const float radius = 0.24f + zeroOneDist(rng) * 0.62f;

			// 발밑보다는 몸통~상체 주변에 떠다니도록 범위 유지.
			const float yOffset = 0.55f + zeroOneDist(rng) * 1.10f;

			XMFLOAT3 pos{};
			pos.x = playerPos.x + std::cos(theta) * radius;
			pos.y = playerPos.y + yOffset;
			pos.z = playerPos.z + std::sin(theta) * radius;

			XMVECTOR dir = XMVectorSet(unitDist(rng), unitDist(rng) * 0.45f, unitDist(rng), 0.0f);

			if ( XMVectorGetX(XMVector3LengthSq(dir)) <= 1.0e-8f )
				dir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			else
				dir = XMVector3Normalize(dir);

			// 기존 0.16~0.38보다 훨씬 느리게.
			const float speed = 0.035f + zeroOneDist(rng) * 0.080f;

			XMFLOAT3 vel{};
			XMStoreFloat3(&vel, XMVectorScale(dir, speed));

			// 기존 0.075~0.12보다 크게.
			const float startSize = 0.145f + zeroOneDist(rng) * 0.085f;
			const float endSize = startSize * ( 1.18f + zeroOneDist(rng) * 0.42f );

			e->active = true;
			e->kind = EMuzzleFlashKind::GoldFirework;

			e->position = pos;
			e->velocity = vel;

			e->age = 0.0f;
			e->lifetime = 1.35f + zeroOneDist(rng) * 0.85f;

			e->startWidth = startSize;
			e->startHeight = startSize;
			e->endWidth = endSize;
			e->endHeight = endSize;

			e->rotationRad = rotDist(rng);
			e->intensity = 0.78f + zeroOneDist(rng) * 0.42f;

			// 버프 지속 파티클은 떠있는 느낌이므로 등속직선운동에 가깝게 유지한다.
			e->gravity = 0.0f;
			e->drag = 0.0f;

			e->seed = seedDist(rng);
			e->color = colors[static_cast< size_t >( inventorySlot )];
		}
	}

	if ( emitCount >= kMaxEmitsPerFrame )
		accumulatorSec = 0.0f;
#else
	UNREFERENCED_PARAMETER(player);
	UNREFERENCED_PARAMETER(inventorySlot);
	UNREFERENCED_PARAMETER(dt);
	UNREFERENCED_PARAMETER(accumulatorSec);
#endif
}

void CGameScene::SpawnWeaponLevelUpFireworks()
{
	for ( int slot = 0; slot < 4; ++slot )
	{
		CGameObject* player = GetPlayerBySlot(slot);
		if ( !player )
			continue;

		if ( !player->GetActive() )
			continue;

		CGameObject* weaponObject = nullptr;

		if ( auto* equip = player->GetComponent<CPlayerEquipmentComponent>() )
		{
			const EWeaponType equippedWeapon = equip->GetEquippedWeapon();
			weaponObject = equip->GetWeaponObject(equippedWeapon);
		}

		// fallback: 장비 컴포넌트에서 못 얻은 경우 기존 slot 순서 ref로 복구.
		if ( !weaponObject )
		{
			const size_t index = static_cast< size_t >( slot );

			auto PickActiveWeaponRef =
				[ index ] (const std::vector<CGameObject*>& refs) -> CGameObject*
				{
					if ( index >= refs.size() )
						return nullptr;

					CGameObject* obj = refs[index];
					if ( !obj )
						return nullptr;

					if ( !obj->GetActive() )
						return nullptr;

					return obj;
				};

			if ( !weaponObject )
				weaponObject = PickActiveWeaponRef(m_PlayerSwordRefs);

			if ( !weaponObject )
				weaponObject = PickActiveWeaponRef(m_PlayerAxeRefs);

			if ( !weaponObject )
				weaponObject = PickActiveWeaponRef(m_PlayerBowRefs);

			if ( !weaponObject )
				weaponObject = PickActiveWeaponRef(m_PlayerGunRefs);
		}

		if ( !weaponObject )
			continue;

		if ( !weaponObject->GetActive() )
			continue;

		SpawnGoldFireworkBurstAtWeapon(weaponObject);
	}
}

void CGameScene::SpawnMagicCircleGlowParticle(
	const XMFLOAT3& center,
	float circleSize,
	float alpha,
	float intensityScale,
	float glowSizeScale,
	float afterimageSizeScale,
	float lifetimeScale)
{
	if ( alpha <= 0.001f )
		return;

	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> zeroOneDist(0.0f, 1.0f);
	static std::uniform_real_distribution<float> unitDist(-1.0f, 1.0f);
	static std::uniform_real_distribution<float> rotDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);

	const float brightness = std::clamp(alpha, 0.0f, 1.0f);
	const float safeLifetimeScale =
		std::max(0.01f, lifetimeScale);

	const float circleRadius =
		std::max(0.25f, circleSize * 0.5f);

	const float angle =
		zeroOneDist(rng) * XM_2PI;

	// 마법진 내부/외곽에 고르게 분포.
	const float r =
		std::sqrt(zeroOneDist(rng)) * circleRadius;

	XMFLOAT3 pos = center;
	pos.x += std::cos(angle) * r;
	pos.z += std::sin(angle) * r;
	pos.y = center.y + kMagicCircleGlowParticleYOffset + zeroOneDist(rng) * 0.08f;

	const float outwardSpeed =
		0.03f + zeroOneDist(rng) * 0.12f;

	const float upwardSpeed =
		0.02f + zeroOneDist(rng) * 0.08f;

	XMFLOAT3 velocity{};
	velocity.x = std::cos(angle) * outwardSpeed;
	velocity.y = upwardSpeed;
	velocity.z = std::sin(angle) * outwardSpeed;

	// ---------------------------------------------------------------------
	// 1) 큰 초록 빛번짐.
	// 총구화염 Core/Ring 사용 금지. MagicCircleGlow 전용 kind 사용.
	// ---------------------------------------------------------------------
	{
		MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashEffect.entries);
		if ( !e )
			return;

		const float safeGlowSizeScale =
			std::max(0.01f, glowSizeScale);

		const float baseSize =
			std::max(
				0.35f,
				circleSize * ( 0.10f + zeroOneDist(rng) * 0.08f )
			) * safeGlowSizeScale;

		e->active = true;
		e->kind = EMuzzleFlashKind::MagicCircleGlow;

		e->position = pos;
		e->velocity = velocity;

		e->age = 0.0f;
		e->lifetime =
			( 0.38f + zeroOneDist(rng) * 0.24f ) *
			safeLifetimeScale;

		e->startWidth = baseSize;
		e->startHeight = baseSize;
		e->endWidth = baseSize * 1.45f;
		e->endHeight = baseSize * 1.45f;

		e->intensity = intensityScale * ( 0.85f + brightness * 0.65f );

		e->color = XMFLOAT4(
			0.16f,
			0.95f,
			0.24f,
			std::clamp(0.18f + brightness * 0.32f, 0.0f, 0.55f)
		);
	}

	// ---------------------------------------------------------------------
	// 2) 잔광/부유 입자.
	// 작은 초록 흔적. 이것도 총구화염 Spark 사용 금지.
	// ---------------------------------------------------------------------
	{
		MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashEffect.entries);
		if ( !e )
			return;

		const float safeAfterimageSizeScale =
			std::max(0.01f, afterimageSizeScale);

		const float moteSize =
			std::max(
				0.18f,
				circleSize * ( 0.045f + zeroOneDist(rng) * 0.045f )
			) * safeAfterimageSizeScale;

		e->active = true;
		e->kind = EMuzzleFlashKind::MagicCircleAfterimage;

		e->position = pos;
		e->velocity = XMFLOAT3(
			velocity.x * ( 0.45f + zeroOneDist(rng) * 0.30f ),
			0.08f + zeroOneDist(rng) * 0.20f,
			velocity.z * ( 0.45f + zeroOneDist(rng) * 0.30f )
		);

		e->lifetime =
			( 0.45f + zeroOneDist(rng) * 0.35f ) *
			safeLifetimeScale;

		e->endWidth = moteSize * 1.35f;
		e->endHeight = moteSize * 1.35f;

		e->intensity = intensityScale * ( 0.75f + brightness * 0.55f );
		e->gravity = 0.0f;

		e->color = XMFLOAT4(
			0.10f,
			0.90f,
			0.18f,
			std::clamp(0.16f + brightness * 0.28f, 0.0f, 0.45f)
		);
	}
}

void CGameScene::EmitMagicCircleGlowParticles(
	const XMFLOAT3& center,
	float circleSize,
	float alpha,
	float dt,
	float& accumulator,
	float emitIntervalSec,
	int particlesPerEmit,
	float intensityScale)
{
	if ( dt <= 0.0f )
		return;

	if ( alpha <= 0.001f )
		return;

	if ( circleSize <= 0.0f )
		return;

	emitIntervalSec =
		( emitIntervalSec > 1.0e-6f )
		? emitIntervalSec
		: 0.001f;

	accumulator += dt;

	if ( accumulator < emitIntervalSec )
		return;

	int emitCount =
		static_cast< int >(accumulator / emitIntervalSec);

	accumulator =
		std::fmod(accumulator, emitIntervalSec);

	// 프레임 드랍 시 한 번에 폭발적으로 생성되는 것 방지.
	if ( emitCount > 3 )
		emitCount = 3;

	for ( int n = 0; n < emitCount; ++n )
	{
		for ( int i = 0; i < particlesPerEmit; ++i )
		{
			SpawnMagicCircleGlowParticle(
				center,
				circleSize,
				alpha,
				intensityScale
			);
		}
	}
}

void CGameScene::SpawnBloodSplash(CGameObject* victim, const XMFLOAT3* hitPosition, const XMFLOAT3* hitDirection)
{
	if ( !victim )
		return;

	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> rotDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);
	static std::uniform_real_distribution<float> unitDist(-1.0f, 1.0f);
	static std::uniform_real_distribution<float> lifeDist(0.30f, 0.62f);
	static std::uniform_real_distribution<float> speedDist(4.0f, 9.4f);
	static std::uniform_real_distribution<float> sideDist(-2.80f, 2.80f);
	static std::uniform_real_distribution<float> liftDist(0.75f, 2.65f);
	static std::uniform_real_distribution<float> sizeDist(0.16f, 0.34f);
	static std::uniform_real_distribution<float> alphaDist(0.46f, 0.68f);
	constexpr float kBloodParticleVisualScale = 2.0f;

	const XMFLOAT3 basePos =
		hitPosition ? *hitPosition : GetBloodSplashFallbackPosition(victim);

	const XMVECTOR fallbackDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	XMVECTOR baseDir = SafeNormalize3OrDefault(hitDirection, fallbackDir);

	baseDir = XMVectorSetY(baseDir, 0.0f);

	if ( XMVectorGetX(XMVector3LengthSq(baseDir)) <= 1.0e-8f )
		baseDir = fallbackDir;
	else
		baseDir = XMVector3Normalize(baseDir);

	const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMVECTOR right = XMVector3Cross(up, baseDir);
	if ( XMVectorGetX(XMVector3LengthSq(right)) <= 1.0e-8f )
		right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	else
		right = XMVector3Normalize(right);

	constexpr int kBloodParticleCount = 60;

	for ( int i = 0; i < kBloodParticleCount; ++i )
	{
		MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashEffect.entries);
		if ( !e )
			return;

		const float side = sideDist(rng);
		const float lift = liftDist(rng);
		const float speed = speedDist(rng);

		XMVECTOR vel =
			XMVectorAdd(
				XMVectorScale(baseDir, 1.15f + unitDist(rng) * 0.55f),
				XMVectorAdd(
					XMVectorScale(right, side),
					XMVectorScale(up, lift)
				)
			);

		if ( XMVectorGetX(XMVector3LengthSq(vel)) <= 1.0e-8f )
			vel = up;
		else
			vel = XMVector3Normalize(vel);

		vel = XMVectorScale(vel, speed);

		XMFLOAT3 vel3{};
		XMStoreFloat3(&vel3, vel);

		const float jitterX = unitDist(rng) * 0.22f;
		const float jitterY = unitDist(rng) * 0.18f;
		const float jitterZ = unitDist(rng) * 0.22f;

		XMFLOAT3 pos = basePos;
		pos.x += jitterX;
		pos.y += jitterY;
		pos.z += jitterZ;

		const float size = sizeDist(rng) * kBloodParticleVisualScale;
		const float alpha = alphaDist(rng);

		e->active = true;
		e->kind = EMuzzleFlashKind::Blood;

		e->position = pos;
		e->velocity = vel3;

		e->age = 0.0f;
		e->lifetime = lifeDist(rng);

		e->startWidth = size;
		e->startHeight = size * ( 0.85f + unitDist(rng) * 0.25f );

		e->endWidth = size * 0.75f;
		e->endHeight = size * 0.65f;

		e->rotationRad = rotDist(rng);

		e->intensity = 1.0f + unitDist(rng) * 0.15f;

		e->drag = 1.35f;
		e->gravity = 4.2f;
		e->seed = seedDist(rng);

		e->color = XMFLOAT4(0.55f, 0.015f, 0.01f, alpha);
	}
}

void CGameScene::SpawnBossMeleeSlashEffect(CGameObject* boss)
{
	if ( !boss )
		return;

	if ( IsMonsterDead(boss) )
		return;

	static std::mt19937 rng{ std::random_device{}( ) };
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);

	const XMFLOAT3 bossPos = boss->GetPosition();

	const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	const XMFLOAT4X4& bossWorldF = boss->GetWorldMatrix();
	const XMMATRIX bossWorld = XMLoadFloat4x4(&bossWorldF);

	XMVECTOR forward =
		XMVector3TransformNormal(
			XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
			bossWorld
		);

	forward = XMVectorSetY(forward, 0.0f);

	if ( XMVectorGetX(XMVector3LengthSq(forward)) <= 1.0e-8f )
	{
		CGameObject* player = GetPlayer();

		if ( !player )
			player = GetPlayerBySlot(0);

		if ( player )
		{
			const XMFLOAT3 playerPos = player->GetPosition();

			forward =
				XMLoadFloat3(&playerPos) -
				XMLoadFloat3(&bossPos);

			forward = XMVectorSetY(forward, 0.0f);
		}
	}

	if ( XMVectorGetX(XMVector3LengthSq(forward)) <= 1.0e-8f )
		forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	else
		forward = XMVector3Normalize(forward);

	XMVECTOR right = XMVector3Cross(worldUp, forward);

	if ( XMVectorGetX(XMVector3LengthSq(right)) <= 1.0e-8f )
		right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	else
		right = XMVector3Normalize(right);

	XMFLOAT3 forwardF{};
	XMFLOAT3 rightF{};
	XMFLOAT3 upF{};

	XMStoreFloat3(&forwardF, forward);
	XMStoreFloat3(&rightF, right);
	XMStoreFloat3(&upF, worldUp);

	auto SpawnSlashLayer =
		[&](
			float width,
			float height,
			float life,
			float startScale,
			float endScale,
			float intensity,
			float alpha,
			float forwardOffset,
			float sideOffset,
			float verticalOffset,
			const XMFLOAT3& rgb,
			float seedBias)
		{
			MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashEffect.entries);

			if ( !e )
				return;

			*e = MuzzleFlashEntry{};

			constexpr float kBossMeleeSlashRootLocalX = -0.46f;
			constexpr float kBossMeleeSlashRootLocalY = -0.46f;

			constexpr float kBossMeleeSlashRootLeftExtraOffset = -1.85f;

			constexpr float kBossMeleeSlashRootSinkY = -1.20f;

			XMVECTOR root =
				XMLoadFloat3(&bossPos) +
				XMVectorScale(forward, forwardOffset) +
				XMVectorScale(right, sideOffset + kBossMeleeSlashRootLeftExtraOffset);

			XMVECTOR center =
				root -
				XMVectorScale(right, width * kBossMeleeSlashRootLocalX) -
				XMVectorScale(worldUp, height * kBossMeleeSlashRootLocalY);

			XMFLOAT3 centerF{};
			XMStoreFloat3(&centerF, center);

			centerF.y =
				bossPos.y +
				kBossMeleeSlashRootSinkY +
				verticalOffset -
				height * kBossMeleeSlashRootLocalY;

			e->active = true;
			e->kind = EMuzzleFlashKind::BossMeleeSlash;

			e->position = centerF;
			e->velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

			e->axisRight = rightF;
			e->axisUp = upF;
			e->axisForward = forwardF;

			e->age = 0.0f;
			e->lifetime = life;

			e->startWidth = width * startScale;
			e->startHeight = height * startScale;
			e->endWidth = width * endScale;
			e->endHeight = height * endScale;

			e->rotationRad = 0.0f;
			e->intensity = intensity;
			e->drag = 0.0f;
			e->gravity = 0.0f;
			e->seed = seedDist(rng) + seedBias;

			e->color = XMFLOAT4(rgb.x, rgb.y, rgb.z, alpha);
		};

	// 1) 외곽 glow: 크게, 오래, 옅게.
	SpawnSlashLayer(
		12.8f,
		13.4f,
		0.56f,
		0.96f,
		1.08f,
		0.78f,
		0.38f,
		1.35f,
		-1.15f,
		-0.15f,
		XMFLOAT3(0.26f, 0.95f, 0.04f),
		77.3f
	);

	// 2) 메인 칼날: 밝은 연두색.
	SpawnSlashLayer(
		11.8f,
		12.5f,
		0.46f,
		0.94f,
		1.03f,
		1.10f,
		0.92f,
		1.50f,
		-0.95f,
		0.00f,
		XMFLOAT3(0.58f, 1.00f, 0.08f),
		0.0f
	);

	// 3) 내부 하이라이트: 흰빛 섞인 연두색.
	SpawnSlashLayer(
		8.8f,
		10.8f,
		0.32f,
		0.92f,
		0.98f,
		1.35f,
		0.68f,
		1.65f,
		-0.75f,
		0.25f,
		XMFLOAT3(0.88f, 1.00f, 0.55f),
		31.7f
	);
}


void CGameScene::UpdateMuzzleFlashes(float dt)
{
	if ( dt <= 0.0f )
		return;

	for ( MuzzleFlashEntry& flash : m_muzzleFlashEffect.entries )
	{
		if ( !flash.active )
			continue;

		flash.age += dt;

		if ( flash.age >= flash.lifetime )
		{
			flash.active = false;
			flash.age = 0.0f;
			continue;
		}

		flash.position.x += flash.velocity.x * dt;
		flash.position.y += flash.velocity.y * dt;
		flash.position.z += flash.velocity.z * dt;

		if ( flash.gravity != 0.0f )
			flash.velocity.y -= flash.gravity * dt;

		float df = 1.0f - flash.drag * dt;
		const float dragFactor = ( df > 0.0f ) ? df : 0.0f;

		flash.velocity.x *= dragFactor;
		flash.velocity.y *= dragFactor;
		flash.velocity.z *= dragFactor;
	}
}

void CGameScene::UpdateGunSmokes(float dt)
{
	if ( dt <= 0.0f )
		return;

	for ( MuzzleFlashEntry& smoke : m_gunSmokeEffect.entries )
	{
		if ( !smoke.active )
			continue;

		smoke.age += dt;

		if ( smoke.age >= smoke.lifetime )
		{
			smoke.active = false;
			smoke.age = 0.0f;
			continue;
		}

		smoke.position.x += smoke.velocity.x * dt;
		smoke.position.y += smoke.velocity.y * dt;
		smoke.position.z += smoke.velocity.z * dt;

		if ( smoke.gravity != 0.0f )
			smoke.velocity.y -= smoke.gravity * dt;

		float df = 1.0f - smoke.drag * dt;
		const float dragFactor = ( df > 0.0f ) ? df : 0.0f;

		smoke.velocity.x *= dragFactor;
		smoke.velocity.y *= dragFactor;
		smoke.velocity.z *= dragFactor;
	}
}

void CGameScene::RenderMuzzleFlashes(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_muzzleFlashEffect.shader ) return;
	if ( !m_itemBillboardState.quadMesh ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kSceneBatchFrameResourceCount;

	ID3D12Resource* muzzleFlashInstanceBuffer = m_muzzleFlashEffect.instanceBuffer.Resource(frameIndex);
	MuzzleFlashInstanceVertex* mappedMuzzleFlashInstanceBuffer = m_muzzleFlashEffect.instanceBuffer.Mapped(frameIndex);
	const UINT muzzleFlashInstanceBufferCapacity = m_muzzleFlashEffect.instanceBuffer.Capacity();

	if ( !muzzleFlashInstanceBuffer ) return;
	if ( !mappedMuzzleFlashInstanceBuffer ) return;
	if ( muzzleFlashInstanceBufferCapacity == 0 ) return;
	if ( m_itemBillboardState.quadMesh->m_SubMeshes.empty() ) return;

	const SubMesh& sm = m_itemBillboardState.quadMesh->m_SubMeshes[0];

	if ( sm.indices.empty() )
		return;

	const XMFLOAT3 cameraPos = camera->GetPosition();

	UINT visibleInstanceCount = 0;

	for ( const MuzzleFlashEntry& flash : m_muzzleFlashEffect.entries )
	{
		if ( !flash.active )
			continue;

		if ( visibleInstanceCount >= muzzleFlashInstanceBufferCapacity )
			break;

		const float ageRatio =
			( flash.lifetime > 1.0e-6f )
			? std::clamp(flash.age / flash.lifetime, 0.0f, 1.0f)
			: 1.0f;

		const float width =
			flash.startWidth +
			( flash.endWidth - flash.startWidth ) * ageRatio;

		const float height =
			flash.startHeight +
			( flash.endHeight - flash.startHeight ) * ageRatio;

		MuzzleFlashInstanceVertex& dst =
			mappedMuzzleFlashInstanceBuffer[visibleInstanceCount];

		if ( flash.kind == EMuzzleFlashKind::BossMeleeSlash )
		{
			StoreOrientedMuzzleFlashWorldRows(
				dst,
				flash.position,
				flash.axisRight,
				flash.axisUp,
				flash.axisForward,
				width,
				height
			);
		}
		else
		{
			StoreMuzzleFlashWorldRows(
				dst,
				flash.position,
				width,
				height,
				cameraPos
			);
		}

		dst.color = flash.color;
		dst.params0 = XMFLOAT4(ageRatio, flash.intensity, flash.rotationRad, flash.seed);
		dst.params1 = XMFLOAT4(static_cast< float >( static_cast< UINT >( flash.kind ) ), 0.0f, 0.0f, 0.0f);

		++visibleInstanceCount;
	}

	if ( visibleInstanceCount == 0 )
		return;

	m_muzzleFlashEffect.shader->Render(cmd, camera, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
	vbViews[0] = sm.vbView;

	vbViews[1].BufferLocation =
		muzzleFlashInstanceBuffer->GetGPUVirtualAddress();

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

void CGameScene::RenderGunSmokes(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_gunSmokeEffect.shader ) return;
	if ( !m_itemBillboardState.quadMesh ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kSceneBatchFrameResourceCount;

	ID3D12Resource* instanceBuffer = m_gunSmokeEffect.instanceBuffer.Resource(frameIndex);
	MuzzleFlashInstanceVertex* mappedInstanceBuffer = m_gunSmokeEffect.instanceBuffer.Mapped(frameIndex);
	const UINT instanceBufferCapacity = m_gunSmokeEffect.instanceBuffer.Capacity();

	if ( !instanceBuffer ) return;
	if ( !mappedInstanceBuffer ) return;
	if ( instanceBufferCapacity == 0 ) return;
	if ( m_itemBillboardState.quadMesh->m_SubMeshes.empty() ) return;

	const SubMesh& sm = m_itemBillboardState.quadMesh->m_SubMeshes[0];

	if ( sm.indices.empty() )
		return;

	const XMFLOAT3 cameraPos = camera->GetPosition();

	UINT visibleInstanceCount = 0;

	for ( const MuzzleFlashEntry& smoke : m_gunSmokeEffect.entries )
	{
		if ( !smoke.active )
			continue;

		if ( visibleInstanceCount >= instanceBufferCapacity )
			break;

		const float ageRatio = ( smoke.lifetime > 1.0e-6f ) ? std::clamp(smoke.age / smoke.lifetime, 0.0f, 1.0f) : 1.0f;

		const float width = smoke.startWidth + ( smoke.endWidth - smoke.startWidth ) * ageRatio;
		const float height = smoke.startHeight + ( smoke.endHeight - smoke.startHeight ) * ageRatio;

		MuzzleFlashInstanceVertex& dst = mappedInstanceBuffer[visibleInstanceCount];

		StoreMuzzleFlashWorldRows(dst, smoke.position, width, height, cameraPos);

		dst.color = smoke.color;
		dst.params0 = XMFLOAT4(ageRatio, smoke.intensity, smoke.rotationRad, smoke.seed);
		dst.params1 = XMFLOAT4(static_cast< float >( static_cast< UINT >( smoke.kind ) ), 0.0f, 0.0f, 0.0f);

		++visibleInstanceCount;
	}

	if ( visibleInstanceCount == 0 )
		return;

	m_gunSmokeEffect.shader->Render(cmd, camera, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
	vbViews[0] = sm.vbView;

	vbViews[1].BufferLocation = instanceBuffer->GetGPUVirtualAddress();
	vbViews[1].SizeInBytes = sizeof(MuzzleFlashInstanceVertex) * visibleInstanceCount;
	vbViews[1].StrideInBytes = sizeof(MuzzleFlashInstanceVertex);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 2, vbViews);
	cmd->IASetIndexBuffer(&sm.ibView);

	cmd->DrawIndexedInstanced(static_cast< UINT >( sm.indices.size() ), visibleInstanceCount, 0, 0, 0);
}