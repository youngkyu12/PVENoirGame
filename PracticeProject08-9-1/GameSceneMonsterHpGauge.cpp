//-----------------------------------------------------------------------------
// File: GameSceneMonsterHpGauge.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"

namespace
{
	void StoreMonsterHpGaugeWorldRows(ItemBillboardInstanceVertex& dst, const XMFLOAT3& objectPosition, float yOffset, float maxWidth, float height, float hpRatio, const XMFLOAT3& targetPosition, UINT materialId)
	{
		const float ratio = std::clamp(hpRatio, 0.0f, 1.0f);
		const float width = maxWidth * ratio;

		const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		XMVECTOR center = XMLoadFloat3(&objectPosition);
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

		center = XMVectorAdd(center, XMVectorScale(right, ( maxWidth - width ) * 0.5f));

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
}

void CGameScene::BuildMonsterHpGaugeBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, UINT rtCount, DXGI_FORMAT* rtvFormats, DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	ReleaseMonsterHpGaugeGpuResources();

	m_monsterHpGaugeState.shader = std::make_shared<CItemBillboardShader>();
	m_monsterHpGaugeState.shader->CreateShader(dev, m_pd3dGraphicsRootSignature.Get(), rtCount, rtvFormats, dsvFormat);

	m_monsterHpGaugeState.hpTexture = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 1);
	m_monsterHpGaugeState.hpTexture->LoadTextureFromFile(dev, cmd, L"Assets/UI/HP.dds", RESOURCE_TEXTURE2D, 0);
	CScene::m_pDescriptorHeap->CreateShaderResourceViews(dev, m_monsterHpGaugeState.hpTexture.get(), ROOT_PARAMETER_GLOBAL_SRV);
	SetMaterialDiffuseSrvIndex(static_cast< int >( kMonsterHpGaugeMaterialId ), m_monsterHpGaugeState.hpTexture->GetBaseSrvIndex());

	m_monsterHpGaugeState.quadMesh = CreateItemBillboardQuadMesh(dev, cmd);

	const UINT monsterHpGaugeCapacity = m_ghoulCount + m_swordManCount + m_bowManCount + m_MutantCount;

	if ( monsterHpGaugeCapacity == 0 )
		return;

	m_monsterHpGaugeState.instanceBuffer.Create(dev, cmd, monsterHpGaugeCapacity, [ dev, cmd ] (UINT bufferBytes) { return ::CreateBufferResource(dev, cmd, nullptr, bufferBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr); });
}

void CGameScene::ReleaseMonsterHpGaugeGpuResources()
{
	m_monsterHpGaugeState.instanceBuffer.Release();
	m_monsterHpGaugeState.hpTexture.reset();
	m_monsterHpGaugeState.shader.reset();
	m_monsterHpGaugeState.quadMesh.reset();
}

bool CGameScene::GetMonsterHpGaugeDesc(const SkinnedWorldLodEntry& entry, float& outYOffset, float& outMaxWidth, float& outHeight) const
{
	outYOffset = 0.0f;
	outMaxWidth = 0.0f;
	outHeight = 0.12f;

	if ( entry.assetName == "Ghoul" )
	{
		outYOffset = 2.0f;
		outMaxWidth = 1.0f;
		return true;
	}

	if ( entry.assetName == "BowMan" || entry.assetName == "SwordMan" )
	{
		outYOffset = 3.5f;
		outMaxWidth = 1.5f;
		return true;
	}

	if ( entry.assetName == "Mutant" )
	{
		outYOffset = 3.5f;
		outMaxWidth = 2.0f;
		return true;
	}

	return false;
}

bool CGameScene::IsSkinnedMonsterHpGaugeRenderAllowed(const SkinnedWorldLodEntry& entry) const
{
	if ( !entry.object )
		return false;

	float yOffset = 0.0f;
	float maxWidth = 0.0f;
	float height = 0.0f;

	if ( !GetMonsterHpGaugeDesc(entry, yOffset, maxWidth, height) )
		return false;

	if ( entry.skinnedBatchObjectIndex >= static_cast< UINT >( m_skinnedBatch.objectRefs.size() ) )
		return false;

	if ( entry.skinnedBatchObjectIndex < static_cast< UINT >(m_skinnedDistanceCullFlags.size()) && m_skinnedDistanceCullFlags[entry.skinnedBatchObjectIndex] != 0 )
		return false;

	if ( entry.skinnedBatchObjectIndex < static_cast< UINT >(m_skinnedOcclusionCullFlags.size()) && m_skinnedOcclusionCullFlags[entry.skinnedBatchObjectIndex] != 0 )
		return false;

	const XMFLOAT3 objectPos = entry.object->GetPosition();

	if ( objectPos.y < -50.0f )
		return false;

	const CHealthComponent* health = entry.object->GetComponent<CHealthComponent>();

	if ( !health )
		return false;

	if ( health->GetCurrentHp() <= 0 || health->GetMaxHp() <= 0 )
		return false;

	return true;
}

void CGameScene::RenderMonsterHpGauges(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_monsterHpGaugeState.shader ) return;
	if ( !m_monsterHpGaugeState.quadMesh ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kSceneBatchFrameResourceCount;

	ID3D12Resource* instanceBuffer = m_monsterHpGaugeState.instanceBuffer.Resource(frameIndex);
	ItemBillboardInstanceVertex* mappedInstanceBuffer = m_monsterHpGaugeState.instanceBuffer.Mapped(frameIndex);
	const UINT instanceBufferCapacity = m_monsterHpGaugeState.instanceBuffer.Capacity();

	if ( !instanceBuffer ) return;
	if ( !mappedInstanceBuffer ) return;
	if ( instanceBufferCapacity == 0 ) return;
	if ( m_monsterHpGaugeState.quadMesh->m_SubMeshes.empty() ) return;

	const SubMesh& sm = m_monsterHpGaugeState.quadMesh->m_SubMeshes[0];

	if ( sm.indices.empty() )
		return;

	const XMFLOAT3 targetPos = camera->GetPosition();

	UINT visibleInstanceCount = 0;

	for ( const SkinnedWorldLodEntry& entry : m_skinnedWorldLodEntries )
	{
		if ( !IsSkinnedMonsterHpGaugeRenderAllowed(entry) )
			continue;

		float yOffset = 0.0f;
		float maxWidth = 0.0f;
		float height = 0.0f;

		if ( !GetMonsterHpGaugeDesc(entry, yOffset, maxWidth, height) )
			continue;

		const CHealthComponent* health = entry.object->GetComponent<CHealthComponent>();

		if ( !health )
			continue;

		const float hpRatio = health->GetHpRatio();

		if ( hpRatio <= 0.0f )
			continue;

		if ( visibleInstanceCount >= instanceBufferCapacity )
			break;

		StoreMonsterHpGaugeWorldRows(mappedInstanceBuffer[visibleInstanceCount], entry.object->GetPosition(), yOffset, maxWidth, height, hpRatio, targetPos, kMonsterHpGaugeMaterialId);

		++visibleInstanceCount;
	}

	if ( visibleInstanceCount == 0 )
		return;

	m_monsterHpGaugeState.shader->Render(cmd, camera, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
	vbViews[0] = sm.vbView;

	vbViews[1].BufferLocation = instanceBuffer->GetGPUVirtualAddress();
	vbViews[1].SizeInBytes = sizeof(ItemBillboardInstanceVertex) * visibleInstanceCount;
	vbViews[1].StrideInBytes = sizeof(ItemBillboardInstanceVertex);

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmd->IASetVertexBuffers(0, 2, vbViews);
	cmd->IASetIndexBuffer(&sm.ibView);

	cmd->DrawIndexedInstanced(static_cast< UINT >( sm.indices.size() ), visibleInstanceCount, 0, 0, 0);
}