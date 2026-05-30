//-----------------------------------------------------------------------------
// File: GameSceneItemBillboard.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"
#include "GameSceneBillboardCommon.h"

using namespace GameSceneBillboardCommon;

std::shared_ptr<CMesh> CGameScene::CreateItemBillboardQuadMesh(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
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

void CGameScene::AddPotionItemBillboardEntries()
{
	const std::array<XMFLOAT3, kPotionItemKindCount> potionPositions =
	{
		XMFLOAT3(0.0f, 0.0f, -140.0f),
		XMFLOAT3(2.0f, 0.0f, -140.0f),
		XMFLOAT3(4.0f, 0.0f, -140.0f),
		XMFLOAT3(6.0f, 0.0f, -140.0f)
	};

	const std::array<EItemBillboardKind, kPotionItemKindCount> potionKinds =
	{
		EItemBillboardKind::HealPotion,
		EItemBillboardKind::AttackPowerPotion,
		EItemBillboardKind::DefensePotion,
		EItemBillboardKind::MoveSpeedPotion
	};

	const std::array<UINT, kPotionItemKindCount> potionMaterialIds =
	{
		kHealPotionItemBillboardMaterialId,
		kAttackPotionItemBillboardMaterialId,
		kDefensePotionItemBillboardMaterialId,
		kMoveSpeedPotionItemBillboardMaterialId
	};

	for ( UINT slot = 0; slot < kPotionItemKindCount; ++slot )
	{
		for ( UINT i = 0; i < kPotionItemMaxCountPerKind; ++i )
		{
			ItemBillboardEntry potion{};

			potion.active = true;
			potion.distanceCulled = false;
			potion.transparent = true;

			potion.kind = potionKinds[slot];
			potion.inventorySlot = static_cast< int >(slot);

			potion.position = potionPositions[slot];

			potion.megaGridNumber =
				m_sceneGrid.MegaGridNumberFromWorldPosition(
					potion.position.x,
					potion.position.z
				);

			potion.width = 1.25f;
			potion.height = 1.25f;
			potion.yOffset = 1.20f;

			potion.cullDistance = 300.0f;

			potion.pickupRadius = 1.25f;
			potion.pickupHeightTolerance = 2.0f;

			potion.materialId = potionMaterialIds[slot];

			m_itemBillboardState.entries.push_back(potion);
		}
	}
}

void CGameScene::BuildItemBillboardBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, UINT rtCount, DXGI_FORMAT* rtvFormats, DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_itemBillboardState.entries.clear();

	m_itemBillboardState.shader = std::make_shared<CItemBillboardShader>();
	m_itemBillboardState.shader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		rtCount,
		rtvFormats,
		dsvFormat
	);

	m_itemBillboardState.transparentShader = std::make_shared<CTransparentItemBillboardShader>();

	DXGI_FORMAT transparentRtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_itemBillboardState.transparentShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		1,
		&transparentRtvFormat,
		dsvFormat
	);

	{
		m_itemBillboardState.keyTexture = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 1);

		m_itemBillboardState.keyTexture->LoadTextureFromFile(dev, cmd, L"Assets/Particle/Key.dds", RESOURCE_TEXTURE2D, 0);

		CScene::m_pDescriptorHeap->CreateShaderResourceViews(dev, m_itemBillboardState.keyTexture.get(), ROOT_PARAMETER_GLOBAL_SRV);

		SetKeyItemDiffuseSrvIndex(m_itemBillboardState.keyTexture->GetBaseSrvIndex());
		SetTransparentItemDiffuseSrvIndex(m_itemBillboardState.keyTexture->GetBaseSrvIndex());
	}

	{
		const std::array<const wchar_t*, kPotionItemKindCount> potionTexturePaths =
		{
			L"Assets/UI/Potion_Heal.dds",
			L"Assets/UI/Potion_AttackUP.dds",
			L"Assets/UI/Potion_DeffenceUP.dds",
			L"Assets/UI/Potion_SpeedUP.dds"
		};

		for ( UINT i = 0; i < kPotionItemKindCount; ++i )
		{
			m_itemBillboardState.potionTextures[i] = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 1);

			m_itemBillboardState.potionTextures[i]->LoadTextureFromFile(dev, cmd, potionTexturePaths[i], RESOURCE_TEXTURE2D, 0);

			CScene::m_pDescriptorHeap->CreateShaderResourceViews(dev, m_itemBillboardState.potionTextures[i].get(), ROOT_PARAMETER_GLOBAL_SRV);

			SetMaterialDiffuseSrvIndex(static_cast< int >(kPotionItemBillboardMaterialBaseId + i), m_itemBillboardState.potionTextures[i]->GetBaseSrvIndex());
		}
	}

	{
		m_itemBillboardState.bossSummonCircleTexture = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 1);

		m_itemBillboardState.bossSummonCircleTexture->LoadTextureFromFile(
			dev,
			cmd,
			L"Assets/Particle/mhj.dds",
			RESOURCE_TEXTURE2D,
			0
		);

		CScene::m_pDescriptorHeap->CreateShaderResourceViews(
			dev,
			m_itemBillboardState.bossSummonCircleTexture.get(),
			ROOT_PARAMETER_GLOBAL_SRV
		);

		SetBossSummonCircleDiffuseSrvIndex(
			m_itemBillboardState.bossSummonCircleTexture->GetBaseSrvIndex()
		);

		SetBossCallSummonCircleDiffuseSrvIndex(
			m_itemBillboardState.bossSummonCircleTexture->GetBaseSrvIndex()
		);
	}

	m_itemBillboardState.quadMesh = CreateItemBillboardQuadMesh(dev, cmd);

	if ( !m_itemBillboardState.quadMesh )
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

	m_itemBillboardState.entries.clear();
	m_itemBillboardState.entries.reserve(
		kKeyItemBillboardCount +
		kPotionItemBillboardCount +
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

		m_itemBillboardState.entries.push_back(key);
	}

	AddPotionItemBillboardEntries();

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

		m_itemBillboardState.entries.push_back(summonGlow);
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

		m_itemBillboardState.entries.push_back(summonCircle);
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

		m_itemBillboardState.entries.push_back(shockwave);
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

		m_itemBillboardState.entries.push_back(shockwaveWall);
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

		m_itemBillboardState.entries.push_back(circle);
	}

	const UINT itemBillboardInstanceBufferCapacity =
		static_cast< UINT >( m_itemBillboardState.entries.size() );

	if ( itemBillboardInstanceBufferCapacity == 0 )
		return;

	m_itemBillboardState.instanceBuffer.Create(dev, cmd, itemBillboardInstanceBufferCapacity, [ dev, cmd ] (UINT bufferBytes)
	{
		return ::CreateBufferResource(dev, cmd, nullptr, bufferBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
	});

	const UINT transparentItemBillboardInstanceBufferCapacity =
		static_cast< UINT >( m_itemBillboardState.entries.size() );

	if ( transparentItemBillboardInstanceBufferCapacity > 0 )
	{
		m_itemBillboardState.transparentInstanceBuffer.Create(dev, cmd, transparentItemBillboardInstanceBufferCapacity, [ dev, cmd ] (UINT bufferBytes)
		{
			return ::CreateBufferResource(dev, cmd, nullptr, bufferBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
		});

		BuildMuzzleFlashBatch(dev, cmd, dsvFormat);
		BuildGunSmokeBatch(dev, cmd, dsvFormat);
		BuildBossPoisonProjectileBatch(dev, cmd, dsvFormat);
		BuildSwordTrailBatch(dev, cmd, dsvFormat);
		BuildMonsterSwordTrailBatch(dev, cmd, dsvFormat);
		BuildArrowTrailBatch(dev, cmd, dsvFormat);
		BuildMonsterArrowTrailBatch(dev, cmd, dsvFormat);
		BuildBossCallSummonWwwBatch(dev, cmd, dsvFormat);
	}
}

void CGameScene::ReleaseItemBillboardGpuResources()
{
	m_itemBillboardState.instanceBuffer.Release();
	m_itemBillboardState.transparentInstanceBuffer.Release();
}

void CGameScene::UpdateItemBillboardDistanceCullSelection(CCamera* camera)
{
	if ( !camera )
		return;

	const XMFLOAT3 cameraPos = camera->GetPosition();

	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
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


bool CGameScene::DoesPlayerOverlapItemBillboard(const CGameObject* player, const ItemBillboardEntry& item) const
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

	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
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


void CGameScene::RenderItemBillboards(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_itemBillboardState.shader ) return;
	if ( !m_itemBillboardState.quadMesh ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kSceneBatchFrameResourceCount;

	ID3D12Resource* itemBillboardInstanceBuffer = m_itemBillboardState.instanceBuffer.Resource(frameIndex);
	ItemBillboardInstanceVertex* mappedItemBillboardInstanceBuffer = m_itemBillboardState.instanceBuffer.Mapped(frameIndex);
	const UINT itemBillboardInstanceBufferCapacity = m_itemBillboardState.instanceBuffer.Capacity();

	if ( !itemBillboardInstanceBuffer ) return;
	if ( !mappedItemBillboardInstanceBuffer ) return;
	if ( itemBillboardInstanceBufferCapacity == 0 ) return;
	if ( m_itemBillboardState.quadMesh->m_SubMeshes.empty() ) return;

	const SubMesh& sm = m_itemBillboardState.quadMesh->m_SubMeshes[0];

	if ( sm.indices.empty() )
		return;

	const XMFLOAT3 targetPos = camera->GetPosition();

	UINT visibleInstanceCount = 0;

	for ( const ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( !item.active )
			continue;

		if ( item.transparent )
			continue;

		if ( item.distanceCulled )
			continue;

		if ( visibleInstanceCount >= itemBillboardInstanceBufferCapacity )
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

	m_itemBillboardState.shader->Render(cmd, camera, nullptr);

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

void CGameScene::RenderTransparentItemBillboards(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_itemBillboardState.transparentShader ) return;
	if ( !m_itemBillboardState.quadMesh ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kSceneBatchFrameResourceCount;

	ID3D12Resource* transparentItemBillboardInstanceBuffer = m_itemBillboardState.transparentInstanceBuffer.Resource(frameIndex);
	ItemBillboardInstanceVertex* mappedTransparentItemBillboardInstanceBuffer = m_itemBillboardState.transparentInstanceBuffer.Mapped(frameIndex);
	const UINT transparentItemBillboardInstanceBufferCapacity = m_itemBillboardState.transparentInstanceBuffer.Capacity();

	if ( !transparentItemBillboardInstanceBuffer ) return;
	if ( !mappedTransparentItemBillboardInstanceBuffer ) return;
	if ( transparentItemBillboardInstanceBufferCapacity == 0 ) return;
	if ( m_itemBillboardState.quadMesh->m_SubMeshes.empty() ) return;

	const SubMesh& sm = m_itemBillboardState.quadMesh->m_SubMeshes[0];

	if ( sm.indices.empty() )
		return;

	const XMFLOAT3 targetPos = camera->GetPosition();

	std::vector<const ItemBillboardEntry*> visibleItems;
	visibleItems.reserve(m_itemBillboardState.entries.size());

	for ( const ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( !item.active )
			continue;

		if ( !item.transparent )
			continue;

		if ( item.distanceCulled )
			continue;

		visibleItems.push_back(&item);
	}

	std::sort(
		visibleItems.begin(),
		visibleItems.end(),
		[ &targetPos ] (const ItemBillboardEntry* a, const ItemBillboardEntry* b)
		{
			const float ax = a->position.x - targetPos.x;
			const float ay = a->position.y - targetPos.y;
			const float az = a->position.z - targetPos.z;

			const float bx = b->position.x - targetPos.x;
			const float by = b->position.y - targetPos.y;
			const float bz = b->position.z - targetPos.z;

			const float ad = ax * ax + ay * ay + az * az;
			const float bd = bx * bx + by * by + bz * bz;

			return ad > bd;
		}
	);

	UINT visibleInstanceCount = 0;

	for ( const ItemBillboardEntry* item : visibleItems )
	{
		if ( !item )
			continue;

		if ( visibleInstanceCount >= transparentItemBillboardInstanceBufferCapacity )
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

	m_itemBillboardState.transparentShader->Render(cmd, camera, nullptr);

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
