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

	static void StoreOrientedMuzzleFlashWorldRows(
		MuzzleFlashInstanceVertex& dst,
		const XMFLOAT3& centerPosition,
		const XMFLOAT3& rightAxis,
		const XMFLOAT3& upAxis,
		const XMFLOAT3& forwardAxis,
		float width,
		float height)
	{
		XMVECTOR right = XMLoadFloat3(&rightAxis);
		XMVECTOR up = XMLoadFloat3(&upAxis);
		XMVECTOR forward = XMLoadFloat3(&forwardAxis);

		if ( XMVectorGetX(XMVector3LengthSq(right)) <= 1.0e-8f )
			right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		else
			right = XMVector3Normalize(right);

		if ( XMVectorGetX(XMVector3LengthSq(up)) <= 1.0e-8f )
			up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		else
			up = XMVector3Normalize(up);

		if ( XMVectorGetX(XMVector3LengthSq(forward)) <= 1.0e-8f )
		{
			forward = XMVector3Cross(right, up);

			if ( XMVectorGetX(XMVector3LengthSq(forward)) <= 1.0e-8f )
				forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			else
				forward = XMVector3Normalize(forward);
		}
		else
		{
			forward = XMVector3Normalize(forward);
		}

		const XMVECTOR scaledRight = XMVectorScale(right, width);
		const XMVECTOR scaledUp = XMVectorScale(up, height);

		XMFLOAT3 r{};
		XMFLOAT3 u{};
		XMFLOAT3 f{};
		XMFLOAT3 c{};

		XMStoreFloat3(&r, scaledRight);
		XMStoreFloat3(&u, scaledUp);
		XMStoreFloat3(&f, forward);
		XMStoreFloat3(&c, XMLoadFloat3(&centerPosition));

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

	static XMFLOAT3 GetWeaponFireworkOrigin(CGameObject* weaponObject)
	{
		if ( !weaponObject )
			return XMFLOAT3(0.0f, 0.0f, 0.0f);

		const XMFLOAT4X4& W = weaponObject->GetWorldMatrix();

		// 무기 pivot 기준. 너무 손 안쪽에 붙어 보이면 y만 살짝 올린다.
		return XMFLOAT3(
			W._41,
			W._42 + 0.18f,
			W._43
		);
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
		// position						normal						uv					tangent
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

		SetBossCallSummonCircleDiffuseSrvIndex(
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
		kKeyItemBillboardCount +
		3 +
		kBossShockwaveWallSegmentCount +
		kBossCallSummonCircleMaxCount
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

		summonCircle.active = false;
		summonCircle.distanceCulled = true;

		summonCircle.transparent = true;
		summonCircle.kind = EItemBillboardKind::BossSummonCircle;

		summonCircle.megaGridNumber = 5;

		summonCircle.position = XMFLOAT3(400.0f, 0.0f, 0.0f);

		summonCircle.width = 100.0f;
		summonCircle.height = 100.0f;

		summonCircle.yOffset = 0.05f;

		summonCircle.cullDistance = 1000000.0f;

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

	for ( UINT i = 0; i < kBossCallSummonCircleMaxCount; ++i )
	{
		ItemBillboardEntry circle{};
		circle.active = false;
		circle.distanceCulled = true;
		circle.transparent = true;

		circle.kind = EItemBillboardKind::BossCallSummonCircle;
		circle.megaGridNumber = 5;

		circle.position = XMFLOAT3(0.0f, 0.0f, 0.0f);

		circle.width = 1.0f;
		circle.height = 1.0f;
		circle.yOffset = 0.05f;

		circle.cullDistance = 1000000.0f;

		circle.pickupRadius = 0.0f;
		circle.pickupHeightTolerance = 0.0f;

		circle.materialId = kBossCallSummonCircleMaterialId;

		m_itemBillboards.push_back(circle);
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

			e->color = XMFLOAT4(1.0f, 0.52f, 0.08f, 1.0f);
		};

	spawnCore(0.55f * 1.10f, 0.045f, 2.2f, 1.0f);
	spawnCore( 0.80f * 1.10f, 0.065f, 1.5f, 0.75f);

	spawnRing();

	constexpr int kMuzzleFlashSparkCount = 14;

	for ( int i = 0; i < kMuzzleFlashSparkCount; ++i )
	{
		spawnSpark(rotDist(rng));
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
		MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashes);
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
	float afterimageSizeScale)
{
	if ( alpha <= 0.001f )
		return;

	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> zeroOneDist(0.0f, 1.0f);
	static std::uniform_real_distribution<float> unitDist(-1.0f, 1.0f);
	static std::uniform_real_distribution<float> rotDist(0.0f, XM_2PI);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);

	const float brightness = std::clamp(alpha, 0.0f, 1.0f);

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
		MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashes);
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
		e->lifetime = 0.38f + zeroOneDist(rng) * 0.24f;

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
		MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashes);
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

		e->lifetime = 0.45f + zeroOneDist(rng) * 0.35f;

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
		MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashes);
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
#ifndef USING_NETWORK
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
			MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashes);

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

#else
	UNREFERENCED_PARAMETER(boss);
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

	SwordTrailEntry* trail = AcquireFreeSwordTrailEntry(m_swordTrails);
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
		AcquireFreeMonsterSwordTrailEntry(m_monsterSwordTrails);

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

	m_bossCallSummonCircleVisualState.alpha = alpha;
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
	m_activeBossCallSummonCircleItemIndices.clear();

	for ( ItemBillboardEntry& item : m_itemBillboards )
	{
		if ( item.kind != EItemBillboardKind::BossCallSummonCircle )
			continue;

		item.active = false;
		item.distanceCulled = true;
		item.width = 0.0f;
		item.height = 0.0f;
	}

	m_bossCallSummonCircleVisualState = BossCallSummonCircleVisualState{};
	m_bossCallSummonGlowParticleEmitAccumulatorSec = 0.0f;

	SetBossCallSummonCircleAlpha(0.0f);
}

void CGameScene::AddBossCallSummonCircle(
	const XMFLOAT3& center,
	EEnemySpawnerEnemyKind kind)
{
	const float size = GetBossCallSummonCircleSize(kind);

	for ( size_t i = 0; i < m_itemBillboards.size(); ++i )
	{
		ItemBillboardEntry& item = m_itemBillboards[i];

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

		m_activeBossCallSummonCircleItemIndices.push_back(i);
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

	if ( m_activeBossCallSummonCircleItemIndices.empty() )
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

	m_bossCallSummonCircleVisualState = BossCallSummonCircleVisualState{};
	m_bossCallSummonCircleVisualState.active = true;
	m_bossCallSummonCircleVisualState.fadingIn = true;
	m_bossCallSummonCircleVisualState.fadingOut = false;
	m_bossCallSummonCircleVisualState.ageSec = 0.0f;
	m_bossCallSummonCircleVisualState.durationSec =
		( fadeInDurationSec > 1.0e-6f ) ? fadeInDurationSec : 0.001f;
	m_bossCallSummonCircleVisualState.alpha = 0.0f;

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

#ifndef USING_NETWORK
		char buf[512];
		sprintf_s(
			buf,
			"[BossCallSummonCircle][Begin] call=%d plan=%zu activeCircle=%zu fadeIn=%.3f sfxPos=(%.3f, %.3f, %.3f)\n",
			callIndex,
			m_bossCallSummonPlanEntries.size(),
			m_activeBossCallSummonCircleItemIndices.size(),
			m_bossCallSummonCircleVisualState.durationSec,
			sfxPos.x,
			sfxPos.y,
			sfxPos.z
		);
		OutputDebugStringA(buf);
#endif
	}

#else
	UNREFERENCED_PARAMETER(callIndex);
	UNREFERENCED_PARAMETER(fadeInDurationSec);
#endif
}

void CGameScene::StartBossCallSummonCircleFadeOut()
{
#ifndef USING_NETWORK
	if ( !m_bossCallSummonCircleVisualState.active )
		return;

	if ( m_activeBossCallSummonCircleItemIndices.empty() )
		return;

	m_bossCallSummonCircleVisualState.fadingIn = false;
	m_bossCallSummonCircleVisualState.fadingOut = true;
	m_bossCallSummonCircleVisualState.ageSec = 0.0f;
	m_bossCallSummonCircleVisualState.durationSec =
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

	if ( m_activeBossCallSummonCircleItemIndices.empty() )
		return;

	const float emitIntervalSec =
		( kBossCallSummonGlowParticleEmitIntervalSec > 1.0e-6f )
		? kBossCallSummonGlowParticleEmitIntervalSec
		: 0.001f;

	m_bossCallSummonGlowParticleEmitAccumulatorSec += dt;

	if ( m_bossCallSummonGlowParticleEmitAccumulatorSec < emitIntervalSec )
		return;

	int emitCount =
		static_cast< int >(
			m_bossCallSummonGlowParticleEmitAccumulatorSec /
			emitIntervalSec
		);

	m_bossCallSummonGlowParticleEmitAccumulatorSec =
		std::fmod(
			m_bossCallSummonGlowParticleEmitAccumulatorSec,
			emitIntervalSec
		);

	if ( emitCount > 3 )
		emitCount = 3;

	for ( size_t itemIndex : m_activeBossCallSummonCircleItemIndices )
	{
		if ( itemIndex >= m_itemBillboards.size() )
			continue;

		const ItemBillboardEntry& item = m_itemBillboards[itemIndex];

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
					kBossCallSummonAfterimageParticleSizeScale
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
	if ( !m_bossCallSummonCircleVisualState.active )
		return;

	if ( dt < 0.0f )
		dt = 0.0f;

	BossCallSummonCircleVisualState& state =
		m_bossCallSummonCircleVisualState;

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

		ResetBossShockwaveWindSfxTracking();

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

	ApplyBossShockwavePushToLocalPlayer(m_bossShockwavePrevRadius, radius);
	UpdateBossShockwaveWindSfx(radius);

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
	m_bossPoisonProjectiles.clear();
	m_bossPoisonProjectiles.resize(kBossPoisonProjectileMaxCount);

	m_bossPoisonSpellCastStates.clear();
	m_bossMeleeSlashCastStates.clear();
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
		MuzzleFlashEntry* e = AcquireFreeMuzzleFlashEntry(m_muzzleFlashes);

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
			item->kind == EItemBillboardKind::BossShockwave ||
			item->kind == EItemBillboardKind::BossCallSummonCircle )
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