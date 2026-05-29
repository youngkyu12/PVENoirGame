//-----------------------------------------------------------------------------
// File: GameSceneTrailEffects.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"

namespace
{
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

void CGameScene::BuildMonsterArrowTrailBatch( ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_monsterArrowTrailEffect.shader = std::make_shared<CSwordTrailShader>();

	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_monsterArrowTrailEffect.shader->CreateShader(dev, m_pd3dGraphicsRootSignature.Get(), 1, &rtvFormat, dsvFormat);

	m_monsterArrowTrailEffect.entries.clear();
	m_monsterArrowTrailEffect.entries.resize(kMonsterArrowTrailMaxCount);

	m_monsterArrowTrailEffect.vertexBuffer.Create(dev, cmd, kMonsterArrowTrailMaxVertices, [ dev, cmd ] (UINT bufferBytes)
		{
			return ::CreateBufferResource(dev, cmd, nullptr, bufferBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
		});
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

void CGameScene::ReleaseMonsterArrowTrailGpuResources()
{
	m_monsterArrowTrailEffect.vertexBuffer.Release();
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

	auto UpdateExistingTrailEntries =
		[ & ] (ArrowTrailEffectState& effect)
		{
			for ( ArrowTrailEntry& trail : effect.entries )
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
		};

	UpdateExistingTrailEntries(m_arrowTrailEffect);
	UpdateExistingTrailEntries(m_monsterArrowTrailEffect);

	// 현재 발사 중인 화살 위치를 플레이어용 / 몬스터용 trail state에 나눠 샘플링한다.
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

		const bool isPlayerArrow =
			( m_playerWeaponOwnerByObject.find(arrowObj) !=
			  m_playerWeaponOwnerByObject.end() );

		ArrowTrailEffectState& targetEffect =
			isPlayerArrow
			? m_arrowTrailEffect
			: m_monsterArrowTrailEffect;

		const UINT targetMaxSamples =
			isPlayerArrow
			? kArrowTrailMaxSamples
			: kMonsterArrowTrailMaxSamples;

		ArrowTrailEntry* trail =
			FindArrowTrailEntry(targetEffect.entries, arrowObj);

		if ( !trail )
		{
			trail = AcquireFreeArrowTrailEntry(targetEffect.entries);
			if ( !trail )
				continue;

			*trail = ArrowTrailEntry{};
			trail->active = true;
			trail->arrowObject = arrowObj;
		}

		AppendArrowTrailSample(
			*trail,
			arrowObj->GetPosition(),
			targetMaxSamples
		);
	}
}

void CGameScene::RenderSwordTrails(ID3D12GraphicsCommandList* cmd, CCamera* camera)
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

			trailFade = 1.0f - std::clamp(fadeAge / denom, 0.0f, 1.0f);
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

void CGameScene::RenderMonsterArrowTrails(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_monsterArrowTrailEffect.shader ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kSceneBatchFrameResourceCount;

	ID3D12Resource* vertexBuffer = m_monsterArrowTrailEffect.vertexBuffer.Resource(frameIndex);
	SwordTrailVertex* mappedVertexBuffer = m_monsterArrowTrailEffect.vertexBuffer.Mapped(frameIndex);
	const UINT arrowTrailVertexBufferCapacity = m_monsterArrowTrailEffect.vertexBuffer.Capacity();

	if ( !vertexBuffer ) return;
	if ( !mappedVertexBuffer ) return;
	if ( arrowTrailVertexBufferCapacity == 0 ) return;

	struct DrawRange
	{
		UINT startVertex = 0;
		UINT vertexCount = 0;
	};

	std::vector<DrawRange> drawRanges;
	drawRanges.reserve(m_monsterArrowTrailEffect.entries.size());

	UINT vertexCursor = 0;

	const XMFLOAT3 cameraPos = camera->GetPosition();

	constexpr float kArrowTrailSampleLifetimeSec = 0.260f;
	constexpr float kArrowTrailHalfWidth = 0.075f;

	for ( const ArrowTrailEntry& trail : m_monsterArrowTrailEffect.entries )
	{
		if ( !trail.active )
			continue;

		const size_t sampleCount = trail.samples.size();

		if ( sampleCount < 2 )
			continue;

		const UINT neededVertices = static_cast< UINT >(sampleCount * 2);

		if ( vertexCursor + neededVertices > arrowTrailVertexBufferCapacity )
			break;

		const UINT startVertex = vertexCursor;

		for ( size_t i = 0; i < sampleCount; ++i )
		{
			const float u =
				( sampleCount > 1 )
				? static_cast< float >( i ) / static_cast< float >( sampleCount - 1 )
				: 1.0f;

			const ArrowTrailSample& sample = trail.samples[i];

			const XMFLOAT3& prevPos =
				trail.samples[( i > 0 ) ? i - 1 : i].position;

			const XMFLOAT3& nextPos =
				trail.samples[( i + 1 < sampleCount ) ? i + 1 : i].position;

			XMVECTOR dir = XMLoadFloat3(&nextPos) - XMLoadFloat3(&prevPos);

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
				const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
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

			const float headFade = std::clamp(u, 0.0f, 1.0f);

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

			const XMVECTOR offset = XMVectorScale(side, kArrowTrailHalfWidth);

			XMFLOAT3 p0{};
			XMFLOAT3 p1{};

			XMStoreFloat3(&p0, posV - offset);
			XMStoreFloat3(&p1, posV + offset);

			SwordTrailVertex& v0 = mappedVertexBuffer[vertexCursor++];

			v0.position = p0;
			v0.uv = XMFLOAT2(u, 0.0f);
			v0.color = color;

			SwordTrailVertex& v1 = mappedVertexBuffer[vertexCursor++];

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

	m_monsterArrowTrailEffect.shader->Render(cmd, camera, nullptr);

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