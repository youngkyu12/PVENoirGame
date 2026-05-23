//-----------------------------------------------------------------------------
// File: GameSceneBillboard.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"

namespace
{
	static float DistanceSqXZ(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;

		return dx * dx + dz * dz;
	}

	static float DistanceSq3(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		const float dx = a.x - b.x;
		const float dy = a.y - b.y;
		const float dz = a.z - b.z;

		return dx * dx + dy * dy + dz * dz;
	}

	static void StoreCylindricalBillboardWorldRows(
		ItemBillboardInstanceVertex& dst,
		const XMFLOAT3& basePosition,
		float yOffset,
		float width,
		float height,
		const XMFLOAT3& targetPosition,
		UINT materialId)
	{
		const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		XMVECTOR center = XMLoadFloat3(&basePosition);
		center = XMVectorAdd(center, XMVectorSet(0.0f, yOffset, 0.0f, 0.0f));

		XMVECTOR target = XMLoadFloat3(&targetPosition);

		XMVECTOR forward = XMVectorSubtract(target, center);
		forward = XMVectorSetY(forward, 0.0f);

		if ( XMVectorGetX(XMVector3LengthSq(forward)) <= 1.0e-6f )
			forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		else
			forward = XMVector3Normalize(forward);

		XMVECTOR right = XMVector3Cross(up, forward);

		if ( XMVectorGetX(XMVector3LengthSq(right)) <= 1.0e-6f )
			right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		else
			right = XMVector3Normalize(right);

		const XMVECTOR scaledRight = XMVectorScale(right, width);
		const XMVECTOR scaledUp = XMVectorScale(up, height);

		XMFLOAT3 r{};
		XMFLOAT3 u{};
		XMFLOAT3 f{};
		XMFLOAT3 c{};

		XMStoreFloat3(&r, scaledRight);
		XMStoreFloat3(&u, scaledUp);
		XMStoreFloat3(&f, forward);
		XMStoreFloat3(&c, center);

		dst.world0 = XMFLOAT4(r.x, r.y, r.z, 0.0f);
		dst.world1 = XMFLOAT4(u.x, u.y, u.z, 0.0f);
		dst.world2 = XMFLOAT4(f.x, f.y, f.z, 0.0f);
		dst.world3 = XMFLOAT4(c.x, c.y, c.z, 1.0f);

		dst.materialId = materialId;
		dst.pad[0] = 0;
		dst.pad[1] = 0;
		dst.pad[2] = 0;
	}

	static void StoreMuzzleFlashWorldRows(
		MuzzleFlashInstanceVertex& dst,
		const XMFLOAT3& basePosition,
		float width,
		float height,
		const XMFLOAT3& targetPosition)
	{
		const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		XMVECTOR center = XMLoadFloat3(&basePosition);
		XMVECTOR target = XMLoadFloat3(&targetPosition);

		XMVECTOR forward = XMVectorSubtract(target, center);
		forward = XMVectorSetY(forward, 0.0f);

		if ( XMVectorGetX(XMVector3LengthSq(forward)) <= 1.0e-6f )
			forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		else
			forward = XMVector3Normalize(forward);

		XMVECTOR right = XMVector3Cross(up, forward);

		if ( XMVectorGetX(XMVector3LengthSq(right)) <= 1.0e-6f )
			right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		else
			right = XMVector3Normalize(right);

		const XMVECTOR scaledRight = XMVectorScale(right, width);
		const XMVECTOR scaledUp = XMVectorScale(up, height);

		XMFLOAT3 r{};
		XMFLOAT3 u{};
		XMFLOAT3 f{};
		XMFLOAT3 c{};

		XMStoreFloat3(&r, scaledRight);
		XMStoreFloat3(&u, scaledUp);
		XMStoreFloat3(&f, forward);
		XMStoreFloat3(&c, center);

		dst.world0 = XMFLOAT4(r.x, r.y, r.z, 0.0f);
		dst.world1 = XMFLOAT4(u.x, u.y, u.z, 0.0f);
		dst.world2 = XMFLOAT4(f.x, f.y, f.z, 0.0f);
		dst.world3 = XMFLOAT4(c.x, c.y, c.z, 1.0f);
	}

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

	static XMFLOAT3 GetBloodSplashFallbackPosition(const CGameObject* victim)
	{
		if ( !victim )
			return XMFLOAT3(0.0f, 0.0f, 0.0f);

		XMFLOAT3 p = victim->GetPosition();

		// 현재 오브젝트 원점이 발 쪽이므로 피격점 정보가 없으면 약 1m 위.
		p.y += 1.0f;

		return p;
	}

	static XMVECTOR SafeNormalize3OrDefault(
		const XMFLOAT3* dir,
		const XMVECTOR& fallback)
	{
		if ( !dir )
			return fallback;

		XMVECTOR v = XMLoadFloat3(dir);

		if ( XMVectorGetX(XMVector3LengthSq(v)) <= 1.0e-8f )
			return fallback;

		return XMVector3Normalize(v);
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

		// root-tip 사이 폭을 center 기준으로 조절한다.
		// widthScale < 1.0f : 더 얇은/짧은 trail
		// widthScale > 1.0f : 더 넓은 trail
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

		// 같은 위치가 너무 많이 쌓이는 것을 약간 방지.
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

		// 검/도끼에 hitbox component가 빠져 있으면 다중 히트 방지 없이 데미지가 들어갈 수 있으므로
		// 안전하게 collider를 꺼 둔다.
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
}

std::shared_ptr<CMesh> CGameScene::CreateItemBillboardQuadMesh(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd)
{
	if ( !dev || !cmd )
		return nullptr;

	struct ITEM_BILLBOARD_VERTEX
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT2 uv;
		XMFLOAT4 tangent;
	};

	const ITEM_BILLBOARD_VERTEX vertices[4] =
	{
		// position                  normal              uv                  tangent
		{ XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-0.5f, +0.5f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(+0.5f, +0.5f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(+0.5f, -0.5f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
	};

	const UINT indices[6] =
	{
		0, 1, 2,
		0, 2, 3
	};

	auto mesh = std::make_shared<CMesh>(dev, cmd);

	mesh->m_SubMeshes.resize(1);
	SubMesh& sm = mesh->m_SubMeshes[0];

	sm.positions =
	{
		vertices[0].position,
		vertices[1].position,
		vertices[2].position,
		vertices[3].position
	};

	sm.normals =
	{
		vertices[0].normal,
		vertices[1].normal,
		vertices[2].normal,
		vertices[3].normal
	};

	sm.uvs =
	{
		vertices[0].uv,
		vertices[1].uv,
		vertices[2].uv,
		vertices[3].uv
	};

	sm.tangents =
	{
		vertices[0].tangent,
		vertices[1].tangent,
		vertices[2].tangent,
		vertices[3].tangent
	};

	sm.indices.assign(std::begin(indices), std::end(indices));

	sm.subMeshMin = XMFLOAT3(-0.5f, -0.5f, 0.0f);
	sm.subMeshMax = XMFLOAT3(+0.5f, +0.5f, 0.0f);

	sm.materialId = kItemBillboardKeyMaterialId;

	const UINT vertexBufferSize = sizeof(vertices);
	const UINT indexBufferSize = sizeof(indices);

	sm.vb = ::CreateBufferResource(
		dev,
		cmd,
		( void* ) vertices,
		vertexBufferSize,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		&sm.vbUpload
	);

	sm.ib = ::CreateBufferResource(
		dev,
		cmd,
		( void* ) indices,
		indexBufferSize,
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_STATE_INDEX_BUFFER,
		&sm.ibUpload
	);

	sm.vbView.BufferLocation = sm.vb->GetGPUVirtualAddress();
	sm.vbView.StrideInBytes = sizeof(ITEM_BILLBOARD_VERTEX);
	sm.vbView.SizeInBytes = vertexBufferSize;

	sm.ibView.BufferLocation = sm.ib->GetGPUVirtualAddress();
	sm.ibView.Format = DXGI_FORMAT_R32_UINT;
	sm.ibView.SizeInBytes = indexBufferSize;

	return mesh;
}


void CGameScene::BuildItemBillboardBatch(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	UINT rtCount,
	DXGI_FORMAT* rtvFormats,
	DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_itemBillboards.clear();

	m_itemBillboardShader = std::make_shared<CItemBillboardShader>();
	m_itemBillboardShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		rtCount,
		rtvFormats,
		dsvFormat
	);

	m_transparentItemBillboardShader =
		std::make_shared<CTransparentItemBillboardShader>();

	DXGI_FORMAT transparentRtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_transparentItemBillboardShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		1,
		&transparentRtvFormat,
		dsvFormat
	);

	{
		m_keyItemTexture = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 1);

		m_keyItemTexture->LoadTextureFromFile(
			dev,
			cmd,
			L"Assets/UI/Key.dds",
			RESOURCE_TEXTURE2D,
			0
		);

		CScene::m_pDescriptorHeap->CreateShaderResourceViews(
			dev,
			m_keyItemTexture.get(),
			ROOT_PARAMETER_GLOBAL_SRV
		);

		SetKeyItemDiffuseSrvIndex(m_keyItemTexture->GetBaseSrvIndex());
		SetTransparentItemDiffuseSrvIndex(m_keyItemTexture->GetBaseSrvIndex());
	}

	{
		m_bossSummonCircleTexture =
			std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 1);

		m_bossSummonCircleTexture->LoadTextureFromFile(
			dev,
			cmd,
			L"Assets/Particle/mhj.dds",
			RESOURCE_TEXTURE2D,
			0
		);

		CScene::m_pDescriptorHeap->CreateShaderResourceViews(
			dev,
			m_bossSummonCircleTexture.get(),
			ROOT_PARAMETER_GLOBAL_SRV
		);

		SetBossSummonCircleDiffuseSrvIndex(
	m_bossSummonCircleTexture->GetBaseSrvIndex()
		);
	}

	m_itemBillboardQuadMesh = CreateItemBillboardQuadMesh(dev, cmd);

	if ( !m_itemBillboardQuadMesh )
		return;

	const std::array<XMFLOAT3, kKeyItemBillboardCount> keyPositions =
	{
		XMFLOAT3(380.0f, 100.5f, -24.0f),
		XMFLOAT3(400.0f, 0.0f, 400.0f),
		XMFLOAT3(400.0f, 0.0f, 800.0f),
		XMFLOAT3(0.0f, 0.0f, 800.0f),
		XMFLOAT3(-430.0f, 100.5f, 774.0f),
		XMFLOAT3(-400.0f, 0.0f, 400.0f),
		XMFLOAT3(-400.0f, 0.0f, 0.0f)
	};

	m_itemBillboards.clear();
	m_itemBillboards.reserve(
		kKeyItemBillboardCount + 3 + kBossShockwaveWallSegmentCount
	);

	for ( UINT i = 0; i < kKeyItemBillboardCount; ++i )
	{
		ItemBillboardEntry key{};
		key.active = true;
		key.distanceCulled = false;
		key.kind = EItemBillboardKind::Key;

		key.position = keyPositions[i];
		key.megaGridNumber =
			m_sceneGrid.MegaGridNumberFromWorldPosition(
				key.position.x,
				key.position.z
			);

		// 6, 8번 메가그리드의 열쇠는 해당 메가그리드 첫 Mutant 사망 전까지 숨기고 획득 불가.
		if ( key.megaGridNumber == 6 || key.megaGridNumber == 8 )
		{
			key.active = false;
			key.distanceCulled = true;
		}

		key.width = 2.0f;
		key.height = 2.0f;
		key.yOffset = 2.0f;

		key.cullDistance = 300.0f;

		key.pickupRadius = 1.25f;
		key.pickupHeightTolerance = 2.0f;

		key.transparent = true;
		key.materialId = kTransparentItemBillboardMaterialId;

		m_itemBillboards.push_back(key);
	}

	{
		ItemBillboardEntry summonGlow{};

		summonGlow.active = false;
		summonGlow.distanceCulled = true;

		summonGlow.transparent = true;
		summonGlow.kind = EItemBillboardKind::BossSummonGlow;

		summonGlow.megaGridNumber = 5;

		summonGlow.position = XMFLOAT3(400.0f, 0.0f, 0.0f);

		summonGlow.width = 110.0f;
		summonGlow.height = 110.0f;
		summonGlow.yOffset = 0.01f;

		summonGlow.cullDistance = 1000000.0f;

		summonGlow.pickupRadius = 0.0f;
		summonGlow.pickupHeightTolerance = 0.0f;

		summonGlow.materialId = kBossSummonGlowMaterialId;

		m_itemBillboards.push_back(summonGlow);
	}

	{
		ItemBillboardEntry summonCircle{};

		// 처음에는 그리지 않는다.
		// 보스가 실제 활성화될 때 SpawnBossSummonCircle()에서 active=true로 바꾼다.
		summonCircle.active = false;
		summonCircle.distanceCulled = true;

		summonCircle.transparent = true;
		summonCircle.kind = EItemBillboardKind::BossSummonCircle;

		summonCircle.megaGridNumber = 5;

		// fallback 위치. 실제 보스 활성화 시점에 보스 원래 위치로 다시 세팅한다.
		summonCircle.position = XMFLOAT3(400.0f, 0.0f, 0.0f);

		// x 100, z 100 크기.
		summonCircle.width = 100.0f;
		summonCircle.height = 100.0f;

		// 지면과 z-fighting 방지용. 좌표상 중심은 y=0으로 유지하고 렌더만 살짝 띄운다.
		summonCircle.yOffset = 0.05f;

		summonCircle.cullDistance = 1000000.0f;

		// pickup 대상이 아니므로 의미 없는 값.
		summonCircle.pickupRadius = 0.0f;
		summonCircle.pickupHeightTolerance = 0.0f;

		summonCircle.materialId = kBossSummonCircleMaterialId;

		m_itemBillboards.push_back(summonCircle);
	}

	{
		ItemBillboardEntry shockwave{};

		shockwave.active = false;
		shockwave.distanceCulled = true;

		shockwave.transparent = true;
		shockwave.kind = EItemBillboardKind::BossShockwave;
		shockwave.megaGridNumber = -1;

		shockwave.position = XMFLOAT3(0.0f, 0.0f, 0.0f);

		shockwave.width = 0.0f;
		shockwave.height = 0.0f;

		// BossSummonCircle보다 살짝 위.
		shockwave.yOffset = 0.075f;

		shockwave.cullDistance = 1000000.0f;

		shockwave.pickupRadius = 0.0f;
		shockwave.pickupHeightTolerance = 0.0f;

		shockwave.materialId = kBossShockwaveMaterialId;

		m_itemBillboards.push_back(shockwave);
	}

	for ( UINT i = 0; i < kBossShockwaveWallSegmentCount; ++i )
	{
		ItemBillboardEntry shockwaveWall{};

		shockwaveWall.active = false;
		shockwaveWall.distanceCulled = true;

		shockwaveWall.transparent = true;
		shockwaveWall.kind = EItemBillboardKind::BossShockwaveWall;
		shockwaveWall.megaGridNumber = -1;

		shockwaveWall.position = XMFLOAT3(0.0f, 0.0f, 0.0f);

		shockwaveWall.width = 0.0f;
		shockwaveWall.height = 0.0f;
		shockwaveWall.yOffset = 0.0f;

		shockwaveWall.cullDistance = 1000000.0f;

		shockwaveWall.pickupRadius = 0.0f;
		shockwaveWall.pickupHeightTolerance = 0.0f;

		shockwaveWall.materialId = kBossShockwaveWallMaterialId;

		m_itemBillboards.push_back(shockwaveWall);
	}

	m_itemBillboardInstanceBufferCapacity =
		static_cast< UINT >( m_itemBillboards.size() );

	if ( m_itemBillboardInstanceBufferCapacity == 0 )
		return;

	const UINT instanceBufferBytes =
		sizeof(ItemBillboardInstanceVertex) *
		m_itemBillboardInstanceBufferCapacity;

	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		m_pd3dItemBillboardInstanceBuffer[frameIndex] =
			::CreateBufferResource(
				dev,
				cmd,
				nullptr,
				instanceBufferBytes,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr
			);

		if ( m_pd3dItemBillboardInstanceBuffer[frameIndex] )
		{
			m_pd3dItemBillboardInstanceBuffer[frameIndex]->Map(
				0,
				nullptr,
				reinterpret_cast< void** >(
					&m_pMappedItemBillboardInstanceBuffer[frameIndex]
					)
			);
		}
	}

	m_transparentItemBillboardInstanceBufferCapacity =
		static_cast< UINT >( m_itemBillboards.size() );

	if ( m_transparentItemBillboardInstanceBufferCapacity > 0 )
	{
		const UINT transparentInstanceBufferBytes =
			sizeof(ItemBillboardInstanceVertex) *
			m_transparentItemBillboardInstanceBufferCapacity;

		for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
		{
			m_pd3dTransparentItemBillboardInstanceBuffer[frameIndex] =
				::CreateBufferResource(
					dev,
					cmd,
					nullptr,
					transparentInstanceBufferBytes,
					D3D12_HEAP_TYPE_UPLOAD,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr
				);

			if ( m_pd3dTransparentItemBillboardInstanceBuffer[frameIndex] )
			{
				m_pd3dTransparentItemBillboardInstanceBuffer[frameIndex]->Map(
					0,
					nullptr,
					reinterpret_cast< void** >(
						&m_pMappedTransparentItemBillboardInstanceBuffer[frameIndex]
						)
				);
			}
		}

		BuildMuzzleFlashBatch(dev, cmd, dsvFormat);
		BuildBossPoisonProjectileBatch(dev, cmd, dsvFormat);
		BuildSwordTrailBatch(dev, cmd, dsvFormat);
		BuildMonsterSwordTrailBatch(dev, cmd, dsvFormat);
	}
}

void CGameScene::BuildMuzzleFlashBatch(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_muzzleFlashShader = std::make_shared<CMuzzleFlashBillboardShader>();

	// forward pass용. 실제 swapchain format이 다르면 그 format으로 바꿔야 함.
	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_muzzleFlashShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		1,
		&rtvFormat,
		dsvFormat
	);

	m_muzzleFlashes.clear();
	m_muzzleFlashes.resize(kMuzzleFlashMaxCount);

	m_muzzleFlashInstanceBufferCapacity = kMuzzleFlashMaxCount;

	const UINT instanceBufferBytes =
		sizeof(MuzzleFlashInstanceVertex) *
		m_muzzleFlashInstanceBufferCapacity;

	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		m_pd3dMuzzleFlashInstanceBuffer[frameIndex] =
			::CreateBufferResource(
				dev,
				cmd,
				nullptr,
				instanceBufferBytes,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr
			);

		if ( m_pd3dMuzzleFlashInstanceBuffer[frameIndex] )
		{
			m_pd3dMuzzleFlashInstanceBuffer[frameIndex]->Map(
				0,
				nullptr,
				reinterpret_cast< void** >(
					&m_pMappedMuzzleFlashInstanceBuffer[frameIndex]
					)
			);
		}
	}
}

void CGameScene::BuildBossPoisonProjectileBatch(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_bossPoisonProjectileShader =
		std::make_shared<CBossPoisonProjectileBillboardShader>();

	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_bossPoisonProjectileShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		1,
		&rtvFormat,
		dsvFormat
	);

	if ( m_bossPoisonProjectiles.size() != kBossPoisonProjectileMaxCount )
		m_bossPoisonProjectiles.resize(kBossPoisonProjectileMaxCount);

	m_bossPoisonProjectileInstanceBufferCapacity =
		kBossPoisonProjectileMaxCount;

	const UINT instanceBufferBytes =
		sizeof(MuzzleFlashInstanceVertex) *
		m_bossPoisonProjectileInstanceBufferCapacity;

	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		m_pd3dBossPoisonProjectileInstanceBuffer[frameIndex] =
			::CreateBufferResource(
				dev,
				cmd,
				nullptr,
				instanceBufferBytes,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr
			);

		if ( m_pd3dBossPoisonProjectileInstanceBuffer[frameIndex] )
		{
			m_pd3dBossPoisonProjectileInstanceBuffer[frameIndex]->Map(
				0,
				nullptr,
				reinterpret_cast< void** >(
					&m_pMappedBossPoisonProjectileInstanceBuffer[frameIndex]
					)
			);
		}
	}
}

void CGameScene::BuildSwordTrailBatch(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_swordTrailShader = std::make_shared<CSwordTrailShader>();

	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_swordTrailShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		1,
		&rtvFormat,
		dsvFormat
	);

	m_swordTrails.clear();
	m_swordTrails.resize(kSwordTrailMaxCount);

	m_swordTrailVertexBufferCapacity = kSwordTrailMaxVertices;

	const UINT bufferBytes =
		sizeof(SwordTrailVertex) * m_swordTrailVertexBufferCapacity;

	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		m_pd3dSwordTrailVertexBuffer[frameIndex] =
			::CreateBufferResource(
				dev,
				cmd,
				nullptr,
				bufferBytes,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr
			);

		if ( m_pd3dSwordTrailVertexBuffer[frameIndex] )
		{
			m_pd3dSwordTrailVertexBuffer[frameIndex]->Map(
				0,
				nullptr,
				reinterpret_cast< void** >(
					&m_pMappedSwordTrailVertexBuffer[frameIndex]
					)
			);
		}
	}
}

void CGameScene::BuildMonsterSwordTrailBatch(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_monsterSwordTrailShader = std::make_shared<CSwordTrailShader>();

	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_monsterSwordTrailShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		1,
		&rtvFormat,
		dsvFormat
	);

	m_monsterSwordTrails.clear();
	m_monsterSwordTrails.resize(kMonsterSwordTrailMaxCount);

	m_monsterSwordTrailVertexBufferCapacity = kMonsterSwordTrailMaxVertices;

	const UINT bufferBytes =
		sizeof(MonsterSwordTrailVertex) *
		m_monsterSwordTrailVertexBufferCapacity;

	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		m_pd3dMonsterSwordTrailVertexBuffer[frameIndex] =
			::CreateBufferResource(
				dev,
				cmd,
				nullptr,
				bufferBytes,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr
			);

		if ( m_pd3dMonsterSwordTrailVertexBuffer[frameIndex] )
		{
			m_pd3dMonsterSwordTrailVertexBuffer[frameIndex]->Map(
				0,
				nullptr,
				reinterpret_cast< void** >(
					&m_pMappedMonsterSwordTrailVertexBuffer[frameIndex]
					)
			);
		}
	}
}

void CGameScene::ReleaseMuzzleFlashGpuResources()
{
	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		if ( m_pd3dMuzzleFlashInstanceBuffer[frameIndex] )
		{
			if ( m_pMappedMuzzleFlashInstanceBuffer[frameIndex] )
			{
				m_pd3dMuzzleFlashInstanceBuffer[frameIndex]->Unmap(0, nullptr);
				m_pMappedMuzzleFlashInstanceBuffer[frameIndex] = nullptr;
			}

			m_pd3dMuzzleFlashInstanceBuffer[frameIndex].Reset();
		}

		m_pMappedMuzzleFlashInstanceBuffer[frameIndex] = nullptr;
	}

	m_muzzleFlashInstanceBufferCapacity = 0;
}

void CGameScene::ReleaseBossPoisonProjectileGpuResources()
{
	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		if ( m_pd3dBossPoisonProjectileInstanceBuffer[frameIndex] )
		{
			if ( m_pMappedBossPoisonProjectileInstanceBuffer[frameIndex] )
			{
				m_pd3dBossPoisonProjectileInstanceBuffer[frameIndex]->Unmap(0, nullptr);
				m_pMappedBossPoisonProjectileInstanceBuffer[frameIndex] = nullptr;
			}

			m_pd3dBossPoisonProjectileInstanceBuffer[frameIndex].Reset();
		}

		m_pMappedBossPoisonProjectileInstanceBuffer[frameIndex] = nullptr;
	}

	m_bossPoisonProjectileInstanceBufferCapacity = 0;
}

void CGameScene::ReleaseSwordTrailGpuResources()
{
	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		if ( m_pd3dSwordTrailVertexBuffer[frameIndex] )
		{
			if ( m_pMappedSwordTrailVertexBuffer[frameIndex] )
			{
				m_pd3dSwordTrailVertexBuffer[frameIndex]->Unmap(0, nullptr);
				m_pMappedSwordTrailVertexBuffer[frameIndex] = nullptr;
			}

			m_pd3dSwordTrailVertexBuffer[frameIndex].Reset();
		}

		m_pMappedSwordTrailVertexBuffer[frameIndex] = nullptr;
	}

	m_swordTrailVertexBufferCapacity = 0;
}

void CGameScene::ReleaseMonsterSwordTrailGpuResources()
{
	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		if ( m_pd3dMonsterSwordTrailVertexBuffer[frameIndex] )
		{
			if ( m_pMappedMonsterSwordTrailVertexBuffer[frameIndex] )
			{
				m_pd3dMonsterSwordTrailVertexBuffer[frameIndex]->Unmap(0, nullptr);
				m_pMappedMonsterSwordTrailVertexBuffer[frameIndex] = nullptr;
			}

			m_pd3dMonsterSwordTrailVertexBuffer[frameIndex].Reset();
		}

		m_pMappedMonsterSwordTrailVertexBuffer[frameIndex] = nullptr;
	}

	m_monsterSwordTrailVertexBufferCapacity = 0;
}

void CGameScene::SpawnMuzzleFlash(
	const XMFLOAT3& position,
	const XMFLOAT3& direction)
{
	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> rotDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);
	static std::uniform_real_distribution<float> unitDist(-1.0f, 1.0f);
	static std::uniform_real_distribution<float> sparkSpeedBaseDist(2.2f, 4.8f);

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

	auto spawnCore = [ & ] (float size, float life, float intensity, float alpha)
		{
			MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashes);
			if ( !e ) return;

			e->active = true;
			e->kind = EMuzzleFlashKind::Core;
			e->position = position;
			e->velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

			e->age = 0.0f;
			e->lifetime = life;

			e->startWidth = size;
			e->startHeight = size;
			e->endWidth = size * 1.7f;
			e->endHeight = size * 1.7f;

			e->rotationRad = rotDist(rng);
			e->intensity = intensity;
			e->drag = 0.0f;
			e->gravity = 0.0f;
			e->seed = seedDist(rng);

			// 불꽃 중심부: 강한 주황/적황색
			e->color = XMFLOAT4(1.0f, 0.32f, 0.04f, alpha);
		};

	auto spawnRing = [ & ] ()
		{
			MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashes);
			if ( !e ) return;

			e->active = true;
			e->kind = EMuzzleFlashKind::Ring;
			e->position = position;
			e->velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);

			e->age = 0.0f;
			e->lifetime = 0.06f;

			e->startWidth = 0.20f * 1.10f;
			e->startHeight = 0.20f * 1.10f;
			e->endWidth = 1.15f * 1.10f;
			e->endHeight = 1.15f * 1.10f;

			e->rotationRad = rotDist(rng);
			e->intensity = 1.2f;
			e->drag = 0.0f;
			e->gravity = 0.0f;
			e->seed = seedDist(rng);

			// 충격 링: 붉은 외곽 불꽃 느낌
			e->color = XMFLOAT4(1.0f, 0.28f, 0.03f, 0.75f);
		};

	auto spawnSpark = [ & ] (float baseRot)
		{
			MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashes);
			if ( !e ) return;

			const float side = unitDist(rng) * 0.35f;
			const float lift = unitDist(rng) * 0.18f + 0.12f;
			const float speed = sparkSpeedBaseDist(rng) * 1.10f;

			XMVECTOR vel =
				XMVectorAdd(
					XMVectorScale(dirV, 1.0f),
					XMVectorAdd(
						XMVectorScale(right, side),
						XMVectorScale(up, lift)
					)
				);

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

			e->startWidth = 0.10f;
			e->startHeight = 0.42f;
			e->endWidth = 0.05f;
			e->endHeight = 0.28f;

			e->rotationRad = baseRot + unitDist(rng) * 0.35f;
			e->intensity = 1.4f;
			e->drag = 5.5f;
			e->gravity = 0.0f;
			e->seed = seedDist(rng);

			// 스파크: 노란 심지 + 주황 불티
			e->color = XMFLOAT4(1.0f, 0.52f, 0.08f, 1.0f);
		};

	// 코어 flash를 2장 겹친다
	spawnCore(0.55f * 1.10f, 0.045f, 2.2f, 1.0f);
	spawnCore( 0.80f * 1.10f, 0.065f, 1.5f, 0.75f);

	// 충격 링
	spawnRing();

	// spark
	constexpr int kMuzzleFlashSparkCount = 14;

	for ( int i = 0; i < kMuzzleFlashSparkCount; ++i )
	{
		spawnSpark(rotDist(rng));
	}
}

void CGameScene::SpawnBloodSplash(
	CGameObject* victim,
	const XMFLOAT3* hitPosition,
	const XMFLOAT3* hitDirection)
{
	if ( !victim )
		return;

	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> rotDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);
	static std::uniform_real_distribution<float> unitDist(-1.0f, 1.0f);
	static std::uniform_real_distribution<float> lifeDist(0.24f, 0.48f);
	static std::uniform_real_distribution<float> speedDist(2.4f, 6.2f);
	static std::uniform_real_distribution<float> sideDist(-1.35f, 1.35f);
	static std::uniform_real_distribution<float> liftDist(0.55f, 1.85f);
	static std::uniform_real_distribution<float> sizeDist(0.18f, 0.36f);
	static std::uniform_real_distribution<float> alphaDist(0.70f, 1.00f);

	const XMFLOAT3 basePos =
		hitPosition ? *hitPosition : GetBloodSplashFallbackPosition(victim);

	const XMVECTOR fallbackDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	XMVECTOR baseDir = SafeNormalize3OrDefault(hitDirection, fallbackDir);

	// 수평 기준으로 피가 앞/옆/위로 튀도록 한다.
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

	constexpr int kBloodParticleCount = 30;

	for ( int i = 0; i < kBloodParticleCount; ++i )
	{
		MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashes);
		if ( !e )
			return;

		const float side = sideDist(rng);
		const float lift = liftDist(rng);
		const float speed = speedDist(rng);

		XMVECTOR vel =
			XMVectorAdd(
				XMVectorScale(baseDir, 1.0f + unitDist(rng) * 0.35f),
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

		const float jitterX = unitDist(rng) * 0.08f;
		const float jitterY = unitDist(rng) * 0.10f;
		const float jitterZ = unitDist(rng) * 0.08f;

		XMFLOAT3 pos = basePos;
		pos.x += jitterX;
		pos.y += jitterY;
		pos.z += jitterZ;

		const float size = sizeDist(rng);
		const float alpha = alphaDist(rng);

		e->active = true;
		e->kind = EMuzzleFlashKind::Blood;

		e->position = pos;
		e->velocity = vel3;

		e->age = 0.0f;
		e->lifetime = lifeDist(rng);

		// 시작은 살짝 크고, 시간이 지나며 작아지게 한다.
		e->startWidth = size;
		e->startHeight = size * ( 0.85f + unitDist(rng) * 0.25f );

		// 시간이 지나면서 너무 작아지지 않게 유지.
		// 기존 0.35배는 너무 빨리 사라져 보인다.
		e->endWidth = size * 0.75f;
		e->endHeight = size * 0.65f;

		e->rotationRad = rotDist(rng);

		// Blood branch에서는 intensity를 밝기/농도 계수로 사용.
		e->intensity = 1.0f + unitDist(rng) * 0.15f;

		e->drag = 2.2f;
		e->gravity = 4.2f;
		e->seed = seedDist(rng);

		// 어두운 붉은색. additive 포화 방지를 위해 너무 밝게 두지 않는다.
		e->color = XMFLOAT4(0.55f, 0.015f, 0.01f, alpha);
	}
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

	SwordTrailEntry* trail = AcquireFreeSwordTrailEntry(m_swordTrails);
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

	// 검 모델의 길이 방향이 local Z가 아니면 여기만 바꿔라.
	trail->rootLocal = XMFLOAT3(0.0f, 0.0f, 0.10f);
	trail->tipLocal = XMFLOAT3(0.0f, 0.0f, 1.45f);

	// 검은 전체 날 길이를 쓰므로 1.0.
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

	SwordTrailEntry* trail = AcquireFreeSwordTrailEntry(m_swordTrails);
	if ( !trail )
		return;

	trail->active = true;
	trail->kind = EWeaponTrailKind::Axe;

	trail->owner = owner;
	trail->weaponObject = axeObject;

	trail->age = 0.0f;
	ResetPlayerMeleeWeaponHitbox(axeObject);

	// 최종 튜닝값 고정.
	// 공격 accepted 이후 0.530초부터 0.690초까지 도끼 궤적을 샘플링한다.
	trail->startDelay = 0.530f;
	trail->sampleDuration = 0.160f; // 0.690f - 0.530f
	trail->fadeDuration = 0.120f;

	// 도끼는 날이 끝자락에만 있으므로 root를 tip에 가깝게 둔다.
	trail->rootLocal = XMFLOAT3(0.0f, 0.0f, 0.80f);
	trail->tipLocal = XMFLOAT3(0.0f, 0.0f, 1.45f);

	// 도끼 궤적 폭 조절 핵심값.
	// 작게 할수록 날 끝부분에만 짧게 붙는다.
	// 0.45~0.80 사이에서 튜닝 추천.
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
		AcquireFreeMonsterSwordTrailEntry(m_monsterSwordTrails);

	if ( !trail )
		return;

	trail->active = true;

	trail->owner = swordman;
	trail->weaponObject = swordObject;

	trail->age = 0.0f;

	// SwordMan은 플레이어 sword attack과 같은 애니메이션 타이밍.
	trail->startDelay = 0.340f;
	trail->sampleDuration = 0.240f;
	trail->fadeDuration = 0.120f;

	// SwordMan 에셋/메시가 플레이어 대비 1.5배.
	trail->rootLocal = XMFLOAT3(0.0f, 0.0f, 0.10f * 1.5f);
	trail->tipLocal = XMFLOAT3(0.0f, 0.0f, 1.45f * 1.5f);

	trail->widthScale = 1.0f;

	// 일단 플레이어 검과 같은 색.
	trail->color = XMFLOAT4(0.55f, 0.80f, 1.0f, 1.0f);

	trail->samples.clear();
	trail->samples.reserve(kMonsterSwordTrailMaxSamples);
}

void CGameScene::UpdateMuzzleFlashes(float dt)
{
	if ( dt <= 0.0f )
		return;

	for ( MuzzleFlashEntry& flash : m_muzzleFlashes )
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

void CGameScene::UpdateSwordTrails(float dt)
{
	if ( dt <= 0.0f )
		return;

	for ( SwordTrailEntry& trail : m_swordTrails )
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

		// 준비 동작 구간: trail도 샘플링하지 않고, 공격 판정도 끈다.
		if ( trail.age < trail.startDelay )
		{
			SetPlayerMeleeWeaponHitboxActive(trail.weaponObject, false);
			continue;
		}

		const float sampleAge = trail.age - trail.startDelay;

		// startDelay 이후 sampleDuration 동안만 trail을 샘플링하고 공격 판정도 켠다.
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

	for ( MonsterSwordTrailEntry& trail : m_monsterSwordTrails )
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

void CGameScene::ReleaseItemBillboardGpuResources()
{
	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		if ( m_pd3dItemBillboardInstanceBuffer[frameIndex] )
		{
			if ( m_pMappedItemBillboardInstanceBuffer[frameIndex] )
			{
				m_pd3dItemBillboardInstanceBuffer[frameIndex]->Unmap(0, nullptr);
				m_pMappedItemBillboardInstanceBuffer[frameIndex] = nullptr;
			}

			m_pd3dItemBillboardInstanceBuffer[frameIndex].Reset();
		}

		m_pMappedItemBillboardInstanceBuffer[frameIndex] = nullptr;
	}

	m_itemBillboardInstanceBufferCapacity = 0;

	for ( UINT frameIndex = 0; frameIndex < kFrameResourceCount; ++frameIndex )
	{
		if ( m_pd3dTransparentItemBillboardInstanceBuffer[frameIndex] )
		{
			if ( m_pMappedTransparentItemBillboardInstanceBuffer[frameIndex] )
			{
				m_pd3dTransparentItemBillboardInstanceBuffer[frameIndex]->Unmap(0, nullptr);
				m_pMappedTransparentItemBillboardInstanceBuffer[frameIndex] = nullptr;
			}

			m_pd3dTransparentItemBillboardInstanceBuffer[frameIndex].Reset();
		}

		m_pMappedTransparentItemBillboardInstanceBuffer[frameIndex] = nullptr;
	}

	m_transparentItemBillboardInstanceBufferCapacity = 0;

	ReleaseMuzzleFlashGpuResources();
	ReleaseSwordTrailGpuResources();
	ReleaseMonsterSwordTrailGpuResources();
}

void CGameScene::UpdateItemBillboardDistanceCullSelection(CCamera* camera)
{
	if ( !camera )
		return;

	const XMFLOAT3 cameraPos = camera->GetPosition();

	for ( ItemBillboardEntry& item : m_itemBillboards )
	{
		if ( !item.active )
		{
			item.distanceCulled = true;
			continue;
		}

		const float cullDist = item.cullDistance;
		const float cullDistSq = cullDist * cullDist;

		item.distanceCulled =
			DistanceSq3(cameraPos, item.position) > cullDistSq;
	}
}

bool CGameScene::DoesPlayerOverlapItemBillboard(
	const CGameObject* player,
	const ItemBillboardEntry& item) const
{
	if ( !player )
		return false;

	if ( !item.active )
		return false;

	const XMFLOAT3 playerPos = player->GetPosition();

	// 죽었거나 비활성 처리된 오브젝트가 지하/멀리 내려가는 패턴을 피하기 위한 방어.
	if ( playerPos.y < -100.0f )
		return false;

	const float radiusSq = item.pickupRadius * item.pickupRadius;

	if ( DistanceSqXZ(playerPos, item.position) > radiusSq )
		return false;

	const float dy = fabsf(playerPos.y - item.position.y);

	if ( dy > item.pickupHeightTolerance )
		return false;

	return true;
}

void CGameScene::UpdateItemBillboardPickupCollision()
{
	if ( !m_bSimulateLocalItemPickup )
		return;

	for ( ItemBillboardEntry& item : m_itemBillboards )
	{
		if ( !item.active )
			continue;

		if ( item.kind != EItemBillboardKind::Key )
			continue;

		for ( int slot = 0; slot < 4; ++slot )
		{
			CGameObject* player = GetPlayerBySlot(slot);

			if ( !player )
				continue;

			if ( DoesPlayerOverlapItemBillboard(player, item) )
			{
				item.active = false;
				item.distanceCulled = true;

				if ( item.kind == EItemBillboardKind::Key )
					MarkMegaGridClearedByNumber(item.megaGridNumber);

				break;
			}
		}
	}
}

void CGameScene::SpawnBossSummonCircle(const XMFLOAT3& center, float alpha)
{
	for ( ItemBillboardEntry& item : m_itemBillboards )
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
	for ( ItemBillboardEntry& item : m_itemBillboards )
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
	// 먼저 glow, 그 다음 circle.
	// 같은 transparent pass 안에서 push 순서가 유지되면 circle이 glow 위에 올라온다.
	SpawnBossSummonGlow(center, alpha);
	SpawnBossSummonCircle(center, alpha);

	SetBossSummonVisualAlpha(alpha);
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

	CGameObject* localPlayer = GetPlayer();

	if ( localPlayer && !m_bLocalPlayerDead )
	{
		const XMFLOAT3 playerPos = localPlayer->GetPosition();

		const float dx = playerPos.x - fixedCenter.x;
		const float dz = playerPos.z - fixedCenter.z;

		const float distSq = dx * dx + dz * dz;

		const float maxAffectRadius =
			kBossShockwaveMaxRadius + kBossShockwavePlayerRangePadding;

		const float maxAffectRadiusSq =
			maxAffectRadius * maxAffectRadius;

		if ( distSq <= maxAffectRadiusSq )
		{
			float dist = sqrtf(distSq);

			XMFLOAT3 pushDir = XMFLOAT3(0.0f, 0.0f, 1.0f);

			if ( dist > kBossShockwavePlayerMinDirectionDistance )
			{
				const float invDist = 1.0f / dist;

				pushDir.x = dx * invDist;
				pushDir.y = 0.0f;
				pushDir.z = dz * invDist;
			}
			else
			{
				// 플레이어가 거의 중심에 있으면 카메라 뒤쪽/전방 같은 기준이 없으므로
				// 임시로 +Z 방향으로 밀어낸다.
				dist = 0.0f;
			}

			m_bBossShockwavePushLocalPlayer = true;
			m_bossShockwavePlayerInitialDistance = dist;
			m_bossShockwavePlayerPushDir = pushDir;
		}
	}

	SetBossShockwaveAlpha(1.0f);
	SetBossShockwaveWallAlpha(0.72f);

	const float correctedRadius =
		kBossShockwaveStartRadius / kBossShockwaveShaderRingCenter;

	for ( ItemBillboardEntry& item : m_itemBillboards )
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

		for ( ItemBillboardEntry& item : m_itemBillboards )
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

	ApplyBossShockwavePushToLocalPlayer(
		m_bossShockwavePrevRadius,
		radius
	);

	m_bossShockwavePrevRadius = radius;
	const float correctedRadius =
		radius / kBossShockwaveShaderRingCenter;

	// 바닥 충격파 갱신
	for ( ItemBillboardEntry& item : m_itemBillboards )
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

	for ( ItemBillboardEntry& item : m_itemBillboards )
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

	// 충격파에 의해 벽 안으로 밀려 들어가는 것은 막는다.
	// RollbackLocalPlayerMoveIfCollidingWorldStatic()는 포탈 처리까지 들어가므로
	// 여기서는 순수 world-static 충돌만 검사해서 원위치시킨다.
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
		// 남아있는 입력/속도 때문에 충격파 밀림이 덜 보이는 것을 막는다.
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
	m_bossPoisonProjectiles.clear();
	m_bossPoisonProjectiles.resize(kBossPoisonProjectileMaxCount);

	m_bossPoisonSpellCastStates.clear();

	m_bossPoisonProjectileLaunchDelaySec =
		kBossPoisonProjectileDefaultLaunchDelaySec;

	m_bossPoisonProjectileLaunchHeight =
		kBossPoisonProjectileDefaultLaunchHeight;

	m_bossPoisonProjectileSpeed =
		kBossPoisonProjectileDefaultSpeed;

	m_bPrevBossPoisonDelayDecKeyDown = false;
	m_bPrevBossPoisonDelayIncKeyDown = false;
	m_bPrevBossPoisonHeightIncKeyDown = false;
	m_bPrevBossPoisonHeightDecKeyDown = false;
}

bool CGameScene::UpdateBossPoisonProjectileDebugInput(UCHAR* pKeysBuffer)
{
#ifndef USING_NETWORK
	if ( !pKeysBuffer )
		return false;

	const bool delayDecDown = ( pKeysBuffer[VK_LEFT] & 0xF0 ) != 0;
	const bool delayIncDown = ( pKeysBuffer[VK_RIGHT] & 0xF0 ) != 0;
	const bool heightIncDown = ( pKeysBuffer[VK_UP] & 0xF0 ) != 0;
	const bool heightDecDown = ( pKeysBuffer[VK_DOWN] & 0xF0 ) != 0;

	bool changed = false;

	if ( delayDecDown && !m_bPrevBossPoisonDelayDecKeyDown )
	{
		m_bossPoisonProjectileLaunchDelaySec -=
			kBossPoisonProjectileDelayStep;

		if ( m_bossPoisonProjectileLaunchDelaySec < 0.0f )
			m_bossPoisonProjectileLaunchDelaySec = 0.0f;

		changed = true;
	}

	if ( delayIncDown && !m_bPrevBossPoisonDelayIncKeyDown )
	{
		m_bossPoisonProjectileLaunchDelaySec +=
			kBossPoisonProjectileDelayStep;

		changed = true;
	}

	if ( heightIncDown && !m_bPrevBossPoisonHeightIncKeyDown )
	{
		m_bossPoisonProjectileLaunchHeight +=
			kBossPoisonProjectileHeightStep;

		changed = true;
	}

	if ( heightDecDown && !m_bPrevBossPoisonHeightDecKeyDown )
	{
		m_bossPoisonProjectileLaunchHeight -=
			kBossPoisonProjectileHeightStep;

		if ( m_bossPoisonProjectileLaunchHeight < 0.0f )
			m_bossPoisonProjectileLaunchHeight = 0.0f;

		changed = true;
	}

	m_bPrevBossPoisonDelayDecKeyDown = delayDecDown;
	m_bPrevBossPoisonDelayIncKeyDown = delayIncDown;
	m_bPrevBossPoisonHeightIncKeyDown = heightIncDown;
	m_bPrevBossPoisonHeightDecKeyDown = heightDecDown;

	if ( changed )
	{
		char buf[256];
		sprintf_s(
			buf,
			"[BossPoison][Tuning] delay=%.4f sec height=%.3f speed=%.3f\n",
			m_bossPoisonProjectileLaunchDelaySec,
			m_bossPoisonProjectileLaunchHeight,
			m_bossPoisonProjectileSpeed
		);
		OutputDebugStringA(buf);
	}

	return changed;
#else
	UNREFERENCED_PARAMETER(pKeysBuffer);
	return false;
#endif
}

BossPoisonProjectileEntry*
CGameScene::AcquireFreeBossPoisonProjectileEntry()
{
	for ( BossPoisonProjectileEntry& entry : m_bossPoisonProjectiles )
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

	char buf[512];
	sprintf_s(
		buf,
		"[BossPoison][HitPlayer] slot=%d damage=%d hp=%d/%d dead=%d projectilePos=(%.3f, %.3f, %.3f)\n",
		playerSlot,
		kBossPoisonProjectileDamage,
		hp->GetCurrentHp(),
		hp->GetMaxHp(),
		deadAfterHit ? 1 : 0,
		entry.position.x,
		entry.position.y,
		entry.position.z
	);
	OutputDebugStringA(buf);
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
			m_bossPoisonSpellCastStates.erase(boss);
			continue;
		}

		CAnimatorComponent* animComp =
			boss->GetComponent<CAnimatorComponent>();

		if ( !animComp )
		{
			m_bossPoisonSpellCastStates.erase(boss);
			continue;
		}

		CMonsterAnimController* ctrl =
			animComp->EnsureMonsterController();

		if ( !ctrl )
		{
			m_bossPoisonSpellCastStates.erase(boss);
			continue;
		}

		const bool isSpellPhase = ctrl->IsSpellPhase();

		BossPoisonSpellCastState& state =
			m_bossPoisonSpellCastStates[boss];

		if ( !isSpellPhase )
		{
			if ( state.wasSpellPhase )
			{
				char buf[256];
				sprintf_s(
					buf,
					"[BossPoison][SpellEnd] boss=%p age=%.4f fired=%d\n",
					static_cast< void* >( boss ),
					state.spellAgeSec,
					state.fired ? 1 : 0
				);
				OutputDebugStringA(buf);
			}

			state = BossPoisonSpellCastState{};
			continue;
		}

		if ( !state.wasSpellPhase )
		{
			state.wasSpellPhase = true;
			state.pendingFire = true;
			state.fired = false;
			state.spellAgeSec = 0.0f;

			char buf[256];
			sprintf_s(
				buf,
				"[BossPoison][SpellBegin] boss=%p delay=%.4f height=%.3f\n",
				static_cast< void* >( boss ),
				m_bossPoisonProjectileLaunchDelaySec,
				m_bossPoisonProjectileLaunchHeight
			);
			OutputDebugStringA(buf);
		}
		else
		{
			state.spellAgeSec += dt;
		}

		if ( state.pendingFire &&
			 !state.fired &&
			 state.spellAgeSec >= m_bossPoisonProjectileLaunchDelaySec )
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

void CGameScene::SpawnBossPoisonProjectile(CGameObject* boss)
{
#ifndef USING_NETWORK
	if ( !boss )
		return;

	BossPoisonProjectileEntry* entry =
		AcquireFreeBossPoisonProjectileEntry();

	if ( !entry )
	{
		OutputDebugStringA(
			"[BossPoison][FireFailed] no free projectile entry.\n"
		);
		return;
	}

	const XMFLOAT3 bossPos = boss->GetPosition();
	const XMFLOAT4X4& world = boss->GetWorldMatrix();

	XMFLOAT3 dir = XMFLOAT3(world._31, 0.0f, world._33);

	float lenSq = dir.x * dir.x + dir.z * dir.z;

	if ( lenSq <= 1.0e-8f )
	{
		// fallback. 정상이라면 보스 world matrix의 forward를 사용한다.
		dir = XMFLOAT3(0.0f, 0.0f, 1.0f);
		lenSq = 1.0f;
	}

	const float invLen = 1.0f / sqrtf(lenSq);

	dir.x *= invLen;
	dir.y = 0.0f;
	dir.z *= invLen;

	XMFLOAT3 spawnPos = bossPos;
	spawnPos.y += m_bossPoisonProjectileLaunchHeight;
	spawnPos.x += dir.x * kBossPoisonProjectileForwardOffset;
	spawnPos.z += dir.z * kBossPoisonProjectileForwardOffset;

	entry->active = true;
	entry->owner = boss;

	entry->position = spawnPos;
	entry->direction = dir;

	entry->speed = m_bossPoisonProjectileSpeed;
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

	entry->hitPlayerSlots.fill(false);

	char buf[512];
	sprintf_s(
		buf,
		"[BossPoison][Fire] boss=%p pos=(%.3f, %.3f, %.3f) dir=(%.3f, %.3f, %.3f) speed=%.3f delay=%.4f height=%.3f\n",
		static_cast< void* >( boss ),
		spawnPos.x,
		spawnPos.y,
		spawnPos.z,
		dir.x,
		dir.y,
		dir.z,
		entry->speed,
		m_bossPoisonProjectileLaunchDelaySec,
		m_bossPoisonProjectileLaunchHeight
	);
	OutputDebugStringA(buf);
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

	for ( BossPoisonProjectileEntry& entry : m_bossPoisonProjectiles )
	{
		if ( !entry.active )
			continue;

		entry.position.x += entry.velocity.x * dt;
		entry.position.y += entry.velocity.y * dt;
		entry.position.z += entry.velocity.z * dt;

		ApplyBossPoisonProjectilePlayerHits(entry);

		// 중앙 구체 반지름 2m를 고려해서 벽에 닿는 시점에 제거.
		if ( entry.position.x <= minX + entry.coreRadius ||
			 entry.position.x >= maxX - entry.coreRadius ||
			 entry.position.z <= minZ + entry.coreRadius ||
			 entry.position.z >= maxZ - entry.coreRadius )
		{
			char buf[512];
			sprintf_s(
				buf,
				"[BossPoison][DespawnWall] pos=(%.3f, %.3f, %.3f) boundsX=(%.3f, %.3f) boundsZ=(%.3f, %.3f)\n",
				entry.position.x,
				entry.position.y,
				entry.position.z,
				minX,
				maxX,
				minZ,
				maxZ
			);
			OutputDebugStringA(buf);

			entry = BossPoisonProjectileEntry{};
		}
	}
#else
	UNREFERENCED_PARAMETER(dt);
#endif
}

void CGameScene::RenderItemBillboards(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_itemBillboardShader ) return;
	if ( !m_itemBillboardQuadMesh ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	ID3D12Resource* itemBillboardInstanceBuffer =
		m_pd3dItemBillboardInstanceBuffer[frameIndex].Get();

	ItemBillboardInstanceVertex* mappedItemBillboardInstanceBuffer =
		m_pMappedItemBillboardInstanceBuffer[frameIndex];

	if ( !itemBillboardInstanceBuffer ) return;
	if ( !mappedItemBillboardInstanceBuffer ) return;
	if ( m_itemBillboardQuadMesh->m_SubMeshes.empty() ) return;

	const SubMesh& sm = m_itemBillboardQuadMesh->m_SubMeshes[0];

	if ( sm.indices.empty() )
		return;

	const XMFLOAT3 targetPos = camera->GetPosition();

	UINT visibleInstanceCount = 0;

	for ( const ItemBillboardEntry& item : m_itemBillboards )
	{
		if ( !item.active )
			continue;

		if ( item.transparent )
			continue;

		if ( item.distanceCulled )
			continue;

		if ( visibleInstanceCount >= m_itemBillboardInstanceBufferCapacity )
			break;

		ItemBillboardInstanceVertex& dst =
			mappedItemBillboardInstanceBuffer[visibleInstanceCount];

		StoreCylindricalBillboardWorldRows(
			dst,
			item.position,
			item.yOffset,
			item.width,
			item.height,
			targetPos,
			item.materialId
		);

		++visibleInstanceCount;
	}

	if ( visibleInstanceCount == 0 )
		return;

	m_itemBillboardShader->Render(cmd, camera, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
	vbViews[0] = sm.vbView;

	vbViews[1].BufferLocation =
		itemBillboardInstanceBuffer->GetGPUVirtualAddress();

	vbViews[1].SizeInBytes =
		sizeof(ItemBillboardInstanceVertex) * visibleInstanceCount;

	vbViews[1].StrideInBytes =
		sizeof(ItemBillboardInstanceVertex);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 2, vbViews);
	cmd->IASetIndexBuffer(&sm.ibView);

	cmd->DrawIndexedInstanced(static_cast< UINT >( sm.indices.size() ), visibleInstanceCount, 0, 0, 0);
}

static void StoreXZPlaneItemBillboardWorldRows(
	ItemBillboardInstanceVertex& dst,
	const XMFLOAT3& center,
	float yOffset,
	float width,
	float depth,
	UINT materialId)
{
	// item billboard quad의 local vertex는:
	// x: -0.5 ~ +0.5
	// y: -0.5 ~ +0.5
	// z: 0
	//
	// 이를 world XZ 평면으로 눕힌다.
	// local x -> world x
	// local y -> world z
	// local z -> world y normal axis
	dst.world0 = XMFLOAT4(width, 0.0f, 0.0f, 0.0f);
	dst.world1 = XMFLOAT4(0.0f, 0.0f, depth, 0.0f);
	dst.world2 = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	dst.world3 = XMFLOAT4(center.x, center.y + yOffset, center.z, 1.0f);

	dst.materialId = materialId;
	dst.pad[0] = 0;
	dst.pad[1] = 0;
	dst.pad[2] = 0;
}

void CGameScene::RenderTransparentItemBillboards(
	ID3D12GraphicsCommandList* cmd,
	CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_transparentItemBillboardShader ) return;
	if ( !m_itemBillboardQuadMesh ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	ID3D12Resource* transparentItemBillboardInstanceBuffer =
		m_pd3dTransparentItemBillboardInstanceBuffer[frameIndex].Get();

	ItemBillboardInstanceVertex* mappedTransparentItemBillboardInstanceBuffer =
		m_pMappedTransparentItemBillboardInstanceBuffer[frameIndex];

	if ( !transparentItemBillboardInstanceBuffer ) return;
	if ( !mappedTransparentItemBillboardInstanceBuffer ) return;
	if ( m_itemBillboardQuadMesh->m_SubMeshes.empty() ) return;

	const SubMesh& sm = m_itemBillboardQuadMesh->m_SubMeshes[0];

	if ( sm.indices.empty() )
		return;

	const XMFLOAT3 cameraPos = camera->GetPosition();

	std::vector<const ItemBillboardEntry*> visibleItems;
	visibleItems.reserve(m_itemBillboards.size());

	for ( const ItemBillboardEntry& item : m_itemBillboards )
	{
		if ( !item.active )
			continue;

		if ( !item.transparent )
			continue;

		if ( item.distanceCulled )
			continue;

		visibleItems.push_back(&item);
	}

	if ( visibleItems.empty() )
		return;

	std::sort(
		visibleItems.begin(),
		visibleItems.end(),
		[ &cameraPos ] (const ItemBillboardEntry* a, const ItemBillboardEntry* b)
		{
			const float adx = a->position.x - cameraPos.x;
			const float ady = a->position.y - cameraPos.y;
			const float adz = a->position.z - cameraPos.z;

			const float bdx = b->position.x - cameraPos.x;
			const float bdy = b->position.y - cameraPos.y;
			const float bdz = b->position.z - cameraPos.z;

			const float aDistSq = adx * adx + ady * ady + adz * adz;
			const float bDistSq = bdx * bdx + bdy * bdy + bdz * bdz;

			// transparent는 뒤에서 앞으로
			return aDistSq > bDistSq;
		}
	);

	const XMFLOAT3 targetPos = camera->GetPosition();

	UINT visibleInstanceCount = 0;

	for ( const ItemBillboardEntry* item : visibleItems )
	{
		if ( !item )
			continue;

		if ( visibleInstanceCount >= m_transparentItemBillboardInstanceBufferCapacity )
			break;

		ItemBillboardInstanceVertex& dst =
			mappedTransparentItemBillboardInstanceBuffer[visibleInstanceCount];

		if ( item->kind == EItemBillboardKind::BossSummonCircle ||
			item->kind == EItemBillboardKind::BossSummonGlow ||
			item->kind == EItemBillboardKind::BossShockwave )
		{
			StoreXZPlaneItemBillboardWorldRows(
				dst,
				item->position,
				item->yOffset,
				item->width,
				item->height,
				item->materialId
			);
		}
		else
		{
			StoreCylindricalBillboardWorldRows(
				dst,
				item->position,
				item->yOffset,
				item->width,
				item->height,
				targetPos,
				item->materialId
			);
		}

		++visibleInstanceCount;
	}

	if ( visibleInstanceCount == 0 )
		return;

	m_transparentItemBillboardShader->Render(cmd, camera, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
	vbViews[0] = sm.vbView;

	vbViews[1].BufferLocation =
		transparentItemBillboardInstanceBuffer->GetGPUVirtualAddress();

	vbViews[1].SizeInBytes =
		sizeof(ItemBillboardInstanceVertex) * visibleInstanceCount;

	vbViews[1].StrideInBytes =
		sizeof(ItemBillboardInstanceVertex);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 2, vbViews);
	cmd->IASetIndexBuffer(&sm.ibView);

	cmd->DrawIndexedInstanced(static_cast< UINT >( sm.indices.size() ), visibleInstanceCount, 0, 0, 0);
}

void CGameScene::RenderMuzzleFlashes(
	ID3D12GraphicsCommandList* cmd,
	CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_muzzleFlashShader ) return;
	if ( !m_itemBillboardQuadMesh ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	ID3D12Resource* muzzleFlashInstanceBuffer =
		m_pd3dMuzzleFlashInstanceBuffer[frameIndex].Get();

	MuzzleFlashInstanceVertex* mappedMuzzleFlashInstanceBuffer =
		m_pMappedMuzzleFlashInstanceBuffer[frameIndex];

	if ( !muzzleFlashInstanceBuffer ) return;
	if ( !mappedMuzzleFlashInstanceBuffer ) return;
	if ( m_muzzleFlashInstanceBufferCapacity == 0 ) return;
	if ( m_itemBillboardQuadMesh->m_SubMeshes.empty() ) return;

	const SubMesh& sm = m_itemBillboardQuadMesh->m_SubMeshes[0];

	if ( sm.indices.empty() )
		return;

	const XMFLOAT3 cameraPos = camera->GetPosition();

	UINT visibleInstanceCount = 0;

	for ( const MuzzleFlashEntry& flash : m_muzzleFlashes )
	{
		if ( !flash.active )
			continue;

		if ( visibleInstanceCount >= m_muzzleFlashInstanceBufferCapacity )
			break;

		const float ageRatio = ( flash.lifetime > 1.0e-6f ) ? std::clamp(flash.age / flash.lifetime, 0.0f, 1.0f) : 1.0f;

		const float width = flash.startWidth + ( flash.endWidth - flash.startWidth ) * ageRatio;
		const float height = flash.startHeight + ( flash.endHeight - flash.startHeight ) * ageRatio;

		MuzzleFlashInstanceVertex& dst =
			mappedMuzzleFlashInstanceBuffer[visibleInstanceCount]; 
		StoreMuzzleFlashWorldRows(dst, flash.position, width, height, cameraPos);

		dst.color = flash.color;
		dst.params0 = XMFLOAT4(ageRatio, flash.intensity, flash.rotationRad, flash.seed);
		dst.params1 = XMFLOAT4(static_cast< float >( static_cast< UINT >( flash.kind ) ), 0.0f, 0.0f, 0.0f);

		++visibleInstanceCount;
	}

	if ( visibleInstanceCount == 0 )
		return;

	m_muzzleFlashShader->Render(cmd, camera, nullptr);

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

	cmd->DrawIndexedInstanced(static_cast< UINT >( sm.indices.size() ), visibleInstanceCount, 0, 0, 0);
}

void CGameScene::RenderBossPoisonProjectiles(
	ID3D12GraphicsCommandList* cmd,
	CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_bossPoisonProjectileShader ) return;
	if ( !m_itemBillboardQuadMesh ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	ID3D12Resource* instanceBuffer =
		m_pd3dBossPoisonProjectileInstanceBuffer[frameIndex].Get();

	MuzzleFlashInstanceVertex* mappedInstanceBuffer =
		m_pMappedBossPoisonProjectileInstanceBuffer[frameIndex];

	if ( !instanceBuffer ) return;
	if ( !mappedInstanceBuffer ) return;
	if ( m_bossPoisonProjectileInstanceBufferCapacity == 0 ) return;
	if ( m_itemBillboardQuadMesh->m_SubMeshes.empty() ) return;

	const SubMesh& sm = m_itemBillboardQuadMesh->m_SubMeshes[0];

	if ( sm.indices.empty() )
		return;

	const XMFLOAT3 cameraPos = camera->GetPosition();

	std::vector<const BossPoisonProjectileEntry*> visibleProjectiles;
	visibleProjectiles.reserve(m_bossPoisonProjectiles.size());

	for ( const BossPoisonProjectileEntry& entry : m_bossPoisonProjectiles )
	{
		if ( entry.active )
			visibleProjectiles.push_back(&entry);
	}

	if ( visibleProjectiles.empty() )
		return;

	std::sort(
		visibleProjectiles.begin(),
		visibleProjectiles.end(),
		[ &cameraPos ](
			const BossPoisonProjectileEntry* a,
			const BossPoisonProjectileEntry* b)
		{
			const float adx = a->position.x - cameraPos.x;
			const float ady = a->position.y - cameraPos.y;
			const float adz = a->position.z - cameraPos.z;

			const float bdx = b->position.x - cameraPos.x;
			const float bdy = b->position.y - cameraPos.y;
			const float bdz = b->position.z - cameraPos.z;

			const float aDistSq = adx * adx + ady * ady + adz * adz;
			const float bDistSq = bdx * bdx + bdy * bdy + bdz * bdz;

			// alpha blend는 뒤에서 앞으로.
			return aDistSq > bDistSq;
		}
	);

	UINT visibleInstanceCount = 0;

	for ( const BossPoisonProjectileEntry* entry : visibleProjectiles )
	{
		if ( !entry )
			continue;

		if ( visibleInstanceCount >= m_bossPoisonProjectileInstanceBufferCapacity )
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

		// color.a만 전체 alpha 계수로 쓴다. 실제 보라/녹색은 HLSL에서 만든다.
		dst.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		// x = unused
		// y = coreDiameter
		// z = gasDiameter
		// w = visualSeed
		dst.params0 = XMFLOAT4(
			0.0f,
			entry->coreDiameter,
			entry->gasDiameter,
			entry->visualSeed
		);

		// x = coreRadius
		// y = speed
		// z/w = reserved
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

	m_bossPoisonProjectileShader->Render(cmd, camera, nullptr);

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

void CGameScene::RenderSwordTrails(
	ID3D12GraphicsCommandList* cmd,
	CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_swordTrailShader ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	ID3D12Resource* swordTrailVertexBuffer =
		m_pd3dSwordTrailVertexBuffer[frameIndex].Get();

	SwordTrailVertex* mappedSwordTrailVertexBuffer =
		m_pMappedSwordTrailVertexBuffer[frameIndex];

	if ( !swordTrailVertexBuffer ) return;
	if ( !mappedSwordTrailVertexBuffer ) return;
	if ( m_swordTrailVertexBufferCapacity == 0 ) return;

	struct DrawRange
	{
		UINT startVertex = 0;
		UINT vertexCount = 0;
	};

	std::vector<DrawRange> drawRanges;
	drawRanges.reserve(m_swordTrails.size());

	UINT vertexCursor = 0;

	for ( const SwordTrailEntry& trail : m_swordTrails )
	{
		if ( !trail.active )
			continue;

		const size_t sampleCount = trail.samples.size();

		if ( sampleCount < 2 )
			continue;

		const UINT neededVertices =
			static_cast< UINT >(sampleCount * 2);

		if ( vertexCursor + neededVertices > m_swordTrailVertexBufferCapacity )
			break;

		float trailFade = 1.0f;

		const float sampleAge = trail.age - trail.startDelay;

		// startDelay 이전에는 샘플도 없겠지만, 방어적으로 렌더하지 않게 처리.
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

			// 오래된 샘플은 약하게, 최신 샘플은 강하게.
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

	m_swordTrailShader->Render(cmd, camera, nullptr);

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
	if ( !m_monsterSwordTrailShader ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kFrameResourceCount;

	ID3D12Resource* vertexBuffer =
		m_pd3dMonsterSwordTrailVertexBuffer[frameIndex].Get();

	MonsterSwordTrailVertex* mappedVertexBuffer =
		m_pMappedMonsterSwordTrailVertexBuffer[frameIndex];

	if ( !vertexBuffer ) return;
	if ( !mappedVertexBuffer ) return;
	if ( m_monsterSwordTrailVertexBufferCapacity == 0 ) return;

	struct DrawRange
	{
		UINT startVertex = 0;
		UINT vertexCount = 0;
	};

	std::vector<DrawRange> drawRanges;
	drawRanges.reserve(m_monsterSwordTrails.size());

	UINT vertexCursor = 0;

	for ( const MonsterSwordTrailEntry& trail : m_monsterSwordTrails )
	{
		if ( !trail.active )
			continue;

		const size_t sampleCount = trail.samples.size();

		if ( sampleCount < 2 )
			continue;

		const UINT neededVertices =
			static_cast< UINT >(sampleCount * 2);

		if ( vertexCursor + neededVertices > m_monsterSwordTrailVertexBufferCapacity )
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

	m_monsterSwordTrailShader->Render(cmd, camera, nullptr);

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