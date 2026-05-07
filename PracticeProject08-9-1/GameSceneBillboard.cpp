//-----------------------------------------------------------------------------
// File: GameSceneBillboard.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScene.h"

#include <algorithm>
#include <random>

#include "Material.h"
#include "Texture.h"
#include "Mesh.h"
#include "Camera.h"
#include "Object.h"
#include "PlayerEquipmentComponent.h"

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

	static void ComputeSwordTrailRootTip(
		const CGameObject* swordObject,
		XMFLOAT3& outRoot,
		XMFLOAT3& outTip)
	{
		if ( !swordObject )
		{
			outRoot = XMFLOAT3(0.0f, 0.0f, 0.0f);
			outTip = XMFLOAT3(0.0f, 0.0f, 0.0f);
			return;
		}

		const XMFLOAT4X4& W = swordObject->GetWorldMatrix();

		// 튜닝 포인트:
		// 검 모델의 길이 방향이 local Z가 아니면 이 두 값을 바꿔야 한다.
		// 예: local Y가 검 길이면 (0, 0.1, 0), (0, 1.4, 0) 식으로 바꾼다.
		static constexpr XMFLOAT3 kSwordTrailRootLocal =
			XMFLOAT3(0.0f, 0.0f, 0.10f);

		static constexpr XMFLOAT3 kSwordTrailTipLocal =
			XMFLOAT3(0.0f, 0.0f, 1.45f);

		outRoot = TransformLocalPoint(W, kSwordTrailRootLocal);
		outTip = TransformLocalPoint(W, kSwordTrailTipLocal);
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
		if ( !trail.swordObject )
			return false;

		SwordTrailSample sample{};
		ComputeSwordTrailRootTip(
			trail.swordObject,
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
	m_itemBillboards.reserve(kKeyItemBillboardCount);

	for ( UINT i = 0; i < kKeyItemBillboardCount; ++i )
	{
		ItemBillboardEntry key{};
		key.active = true;
		key.distanceCulled = false;
		key.kind = EItemBillboardKind::Key;

		key.position = keyPositions[i];

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

	m_itemBillboardInstanceBufferCapacity =
		static_cast< UINT >( m_itemBillboards.size() );

	if ( m_itemBillboardInstanceBufferCapacity == 0 )
		return;

	const UINT instanceBufferBytes =
		sizeof(ItemBillboardInstanceVertex) *
		m_itemBillboardInstanceBufferCapacity;

	m_pd3dItemBillboardInstanceBuffer = ::CreateBufferResource(
		dev,
		cmd,
		nullptr,
		instanceBufferBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr
	);

	m_pd3dItemBillboardInstanceBuffer->Map(
		0,
		nullptr,
		reinterpret_cast< void** >( &m_pMappedItemBillboardInstanceBuffer )
	);

	m_transparentItemBillboardInstanceBufferCapacity =
		static_cast< UINT >( m_itemBillboards.size() );

	if ( m_transparentItemBillboardInstanceBufferCapacity > 0 )
	{
		const UINT transparentInstanceBufferBytes =
			sizeof(ItemBillboardInstanceVertex) *
			m_transparentItemBillboardInstanceBufferCapacity;

		m_pd3dTransparentItemBillboardInstanceBuffer =
			::CreateBufferResource(
				dev,
				cmd,
				nullptr,
				transparentInstanceBufferBytes,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr
			);

		m_pd3dTransparentItemBillboardInstanceBuffer->Map(
			0,
			nullptr,
			reinterpret_cast< void** >(
				&m_pMappedTransparentItemBillboardInstanceBuffer
				)
		);

		BuildMuzzleFlashBatch(dev, cmd, dsvFormat);
		BuildSwordTrailBatch(dev, cmd, dsvFormat);
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

	m_pd3dMuzzleFlashInstanceBuffer = ::CreateBufferResource(
		dev,
		cmd,
		nullptr,
		instanceBufferBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr
	);

	m_pd3dMuzzleFlashInstanceBuffer->Map(
		0,
		nullptr,
		reinterpret_cast< void** >( &m_pMappedMuzzleFlashInstanceBuffer )
	);
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

	m_pd3dSwordTrailVertexBuffer = ::CreateBufferResource(
		dev,
		cmd,
		nullptr,
		bufferBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr
	);

	m_pd3dSwordTrailVertexBuffer->Map(
		0,
		nullptr,
		reinterpret_cast< void** >( &m_pMappedSwordTrailVertexBuffer )
	);
}

void CGameScene::ReleaseMuzzleFlashGpuResources()
{
	if ( m_pd3dMuzzleFlashInstanceBuffer )
	{
		if ( m_pMappedMuzzleFlashInstanceBuffer )
		{
			m_pd3dMuzzleFlashInstanceBuffer->Unmap(0, nullptr);
			m_pMappedMuzzleFlashInstanceBuffer = nullptr;
		}

		m_pd3dMuzzleFlashInstanceBuffer.Reset();
	}

	m_muzzleFlashInstanceBufferCapacity = 0;
}

void CGameScene::ReleaseSwordTrailGpuResources()
{
	if ( m_pd3dSwordTrailVertexBuffer )
	{
		if ( m_pMappedSwordTrailVertexBuffer )
		{
			m_pd3dSwordTrailVertexBuffer->Unmap(0, nullptr);
			m_pMappedSwordTrailVertexBuffer = nullptr;
		}

		m_pd3dSwordTrailVertexBuffer.Reset();
	}

	m_swordTrailVertexBufferCapacity = 0;
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
			e->seed = seedDist(rng);

			e->color = XMFLOAT4(1.0f, 0.48f, 0.10f, alpha);
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
			e->seed = seedDist(rng);

			e->color = XMFLOAT4(1.0f, 0.72f, 0.22f, 0.9f);
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
			e->seed = seedDist(rng);

			e->color = XMFLOAT4(1.0f, 0.70f, 0.18f, 1.0f);
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
	trail->owner = owner;
	trail->swordObject = swordObject;

	trail->age = 0.0f;

	trail->startDelay = 0.340f;
	trail->sampleDuration = 0.240f;
	trail->fadeDuration = 0.120f;
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

		float df = 1.0f - flash.drag * dt;
		const float dragFactor = (df > 0.0f) ? df : 0.0f;

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

		trail.age += dt;

		const float totalLife =
			trail.startDelay +
			trail.sampleDuration +
			trail.fadeDuration;

		if ( trail.age >= totalLife )
		{
			trail.active = false;
			trail.owner = nullptr;
			trail.swordObject = nullptr;
			trail.age = 0.0f;
			trail.samples.clear();
			continue;
		}

		// 아직 칼을 뒤로 빼는 준비 구간이면 샘플링하지 않는다.
		if ( trail.age < trail.startDelay )
			continue;

		const float sampleAge = trail.age - trail.startDelay;

		// startDelay 이후 sampleDuration 동안만 검 위치를 샘플링한다.
		if ( sampleAge <= trail.sampleDuration )
		{
			AppendSwordTrailSample(trail, kSwordTrailMaxSamples);
		}
	}
}

void CGameScene::ReleaseItemBillboardGpuResources()
{
	if ( m_pd3dItemBillboardInstanceBuffer )
	{
		if ( m_pMappedItemBillboardInstanceBuffer )
		{
			m_pd3dItemBillboardInstanceBuffer->Unmap(0, nullptr);
			m_pMappedItemBillboardInstanceBuffer = nullptr;
		}

		m_pd3dItemBillboardInstanceBuffer.Reset();
	}

	m_itemBillboardInstanceBufferCapacity = 0;

	if ( m_pd3dTransparentItemBillboardInstanceBuffer )
	{
		if ( m_pMappedTransparentItemBillboardInstanceBuffer )
		{
			m_pd3dTransparentItemBillboardInstanceBuffer->Unmap(0, nullptr);
			m_pMappedTransparentItemBillboardInstanceBuffer = nullptr;
		}

		m_pd3dTransparentItemBillboardInstanceBuffer.Reset();
	}

	m_transparentItemBillboardInstanceBufferCapacity = 0;

	ReleaseMuzzleFlashGpuResources();
	ReleaseSwordTrailGpuResources();
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
	for ( ItemBillboardEntry& item : m_itemBillboards )
	{
		if ( !item.active )
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

				break;
			}
		}
	}
}

void CGameScene::RenderItemBillboards(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_itemBillboardShader ) return;
	if ( !m_itemBillboardQuadMesh ) return;
	if ( !m_pd3dItemBillboardInstanceBuffer ) return;
	if ( !m_pMappedItemBillboardInstanceBuffer ) return;
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
			m_pMappedItemBillboardInstanceBuffer[visibleInstanceCount];

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
		m_pd3dItemBillboardInstanceBuffer->GetGPUVirtualAddress();

	vbViews[1].SizeInBytes =
		sizeof(ItemBillboardInstanceVertex) * visibleInstanceCount;

	vbViews[1].StrideInBytes =
		sizeof(ItemBillboardInstanceVertex);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 2, vbViews);
	cmd->IASetIndexBuffer(&sm.ibView);

	cmd->DrawIndexedInstanced(static_cast< UINT >( sm.indices.size() ), visibleInstanceCount, 0, 0, 0);
}

void CGameScene::RenderTransparentItemBillboards(
	ID3D12GraphicsCommandList* cmd,
	CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::RenderTransparentItemBillboards");

	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_transparentItemBillboardShader ) return;
	if ( !m_itemBillboardQuadMesh ) return;
	if ( !m_pd3dTransparentItemBillboardInstanceBuffer ) return;
	if ( !m_pMappedTransparentItemBillboardInstanceBuffer ) return;
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
			m_pMappedTransparentItemBillboardInstanceBuffer[visibleInstanceCount];

		StoreCylindricalBillboardWorldRows(
			dst,
			item->position,
			item->yOffset,
			item->width,
			item->height,
			targetPos,
			item->materialId
		);

		++visibleInstanceCount;
	}

	if ( visibleInstanceCount == 0 )
		return;

	m_transparentItemBillboardShader->Render(cmd, camera, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
	vbViews[0] = sm.vbView;

	vbViews[1].BufferLocation =
		m_pd3dTransparentItemBillboardInstanceBuffer->GetGPUVirtualAddress();

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
	PROFILE_RENDER_SCOPE("GameScene::RenderMuzzleFlashes");

	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_muzzleFlashShader ) return;
	if ( !m_itemBillboardQuadMesh ) return;
	if ( !m_pd3dMuzzleFlashInstanceBuffer ) return;
	if ( !m_pMappedMuzzleFlashInstanceBuffer ) return;
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

		MuzzleFlashInstanceVertex& dst = m_pMappedMuzzleFlashInstanceBuffer[visibleInstanceCount];
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
		m_pd3dMuzzleFlashInstanceBuffer->GetGPUVirtualAddress();

	vbViews[1].SizeInBytes =
		sizeof(MuzzleFlashInstanceVertex) * visibleInstanceCount;

	vbViews[1].StrideInBytes =
		sizeof(MuzzleFlashInstanceVertex);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 2, vbViews);
	cmd->IASetIndexBuffer(&sm.ibView);

	cmd->DrawIndexedInstanced(static_cast< UINT >( sm.indices.size() ), visibleInstanceCount, 0, 0, 0);
}

void CGameScene::RenderSwordTrails(
	ID3D12GraphicsCommandList* cmd,
	CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::RenderSwordTrails");

	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_swordTrailShader ) return;
	if ( !m_pd3dSwordTrailVertexBuffer ) return;
	if ( !m_pMappedSwordTrailVertexBuffer ) return;
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

			// 기본 색상. 검 궤적은 흰색-푸른색 계열.
			const XMFLOAT4 color = XMFLOAT4(0.55f, 0.80f, 1.0f, alpha);

			const SwordTrailSample& sample = trail.samples[i];

			SwordTrailVertex& v0 = m_pMappedSwordTrailVertexBuffer[vertexCursor++];

			v0.position = sample.root;
			v0.uv = XMFLOAT2(u, 0.0f);
			v0.color = color;

			SwordTrailVertex& v1 = m_pMappedSwordTrailVertexBuffer[vertexCursor++];

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
	vbView.BufferLocation = m_pd3dSwordTrailVertexBuffer->GetGPUVirtualAddress();
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