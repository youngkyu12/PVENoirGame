//-----------------------------------------------------------------------------
// File: GameSceneBossSummonEffects.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"


void CGameScene::BuildBossCallSummonWwwBatch(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, DXGI_FORMAT dsvFormat)
{
	if ( !dev || !cmd )
		return;

	m_bossCallSummonWwwEffect.shader = std::make_shared<CSwordTrailShader>();

	DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	m_bossCallSummonWwwEffect.shader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		1,
		&rtvFormat,
		dsvFormat
	);

	m_bossCallSummonWwwEffect.entries.clear();
	m_bossCallSummonWwwEffect.entries.resize(kBossCallSummonWwwMaxCount);

	m_bossCallSummonWwwEffect.vertexBuffer.Create(dev, cmd, kBossCallSummonWwwMaxVertices, [ dev, cmd ] (UINT bufferBytes)
	{
		return ::CreateBufferResource(dev, cmd, nullptr, bufferBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
	});
}

void CGameScene::ReleaseBossCallSummonWwwGpuResources()
{
	m_bossCallSummonWwwEffect.vertexBuffer.Release();
}

void CGameScene::SpawnBossCallSummonWwwEffect(const XMFLOAT3& center, EEnemySpawnerEnemyKind kind)
{
	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> zeroOneDist(0.0f, 1.0f);
	static std::uniform_real_distribution<float> lifeDist(0.95f, 1.30f);
	static std::uniform_real_distribution<float> seedDist(0.0f, 1000.0f);

	BossCallSummonWwwEntry* entry = nullptr;

	for ( BossCallSummonWwwEntry& candidate : m_bossCallSummonWwwEffect.entries )
	{
		if ( candidate.active )
			continue;

		entry = &candidate;
		break;
	}

	if ( !entry )
		return;

	const float circleSize = GetBossCallSummonCircleSize(kind);
	const float radius = std::max(0.10f, circleSize * 0.5f);
	const XMFLOAT3 fixedCenter = AlignPositionYToTerrainGround(center, 0.0f);

	*entry = BossCallSummonWwwEntry{};

	entry->active = true;
	entry->center = fixedCenter;

	entry->radius = radius;
	entry->maxHeight = radius * 2.0f;

	entry->age = 0.0f;
	entry->lifetime = lifeDist(rng);

	entry->retargetTimer = 0.0f;
	entry->seed = seedDist(rng);

	entry->color = XMFLOAT4(0.10f, 0.90f, 0.18f, 0.78f);

	for ( UINT i = 0; i < kBossCallSummonWwwPeakCount; ++i )
	{
		const float h = zeroOneDist(rng) * entry->maxHeight;

		entry->peakHeights[i] = h;
		entry->targetPeakHeights[i] = zeroOneDist(rng) * entry->maxHeight;
		entry->peakMoveSpeeds[i] = entry->maxHeight * ( 2.8f + zeroOneDist(rng) * 4.2f );
	}
}

void CGameScene::UpdateBossCallSummonWwwEffects(float dt)
{
	if ( dt <= 0.0f )
		return;

	static std::mt19937 rng{ std::random_device{}( ) };

	static std::uniform_real_distribution<float> zeroOneDist(0.0f, 1.0f);

	for ( BossCallSummonWwwEntry& entry : m_bossCallSummonWwwEffect.entries )
	{
		if ( !entry.active )
			continue;

		entry.age += dt;

		if ( entry.age >= entry.lifetime )
		{
			entry.active = false;
			continue;
		}

		entry.retargetTimer -= dt;

		if ( entry.retargetTimer <= 0.0f )
		{
			// 모든 꼭짓점을 매번 동시에 바꾸면 규칙적으로 보이므로,
			// 일부 꼭짓점만 랜덤하게 새 목표 높이를 받는다.
			for ( UINT i = 0; i < kBossCallSummonWwwPeakCount; ++i )
			{
				if ( zeroOneDist(rng) < 0.68f )
				{
					entry.targetPeakHeights[i] =
						zeroOneDist(rng) * entry.maxHeight;

					entry.peakMoveSpeeds[i] =
						entry.maxHeight *
						( 2.4f + zeroOneDist(rng) * 5.0f );
				}
			}

			entry.retargetTimer =
				0.035f + zeroOneDist(rng) * 0.085f;
		}

		for ( UINT i = 0; i < kBossCallSummonWwwPeakCount; ++i )
		{
			const float target = entry.targetPeakHeights[i];
			const float current = entry.peakHeights[i];

			const float maxStep =
				entry.peakMoveSpeeds[i] * dt;

			const float delta =
				std::clamp(
					target - current,
					-maxStep,
					maxStep
				);

			entry.peakHeights[i] =
				std::clamp(
					current + delta,
					0.0f,
					entry.maxHeight
				);
		}
	}
}
void CGameScene::RenderBossCallSummonWwwEffects(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !camera ) return;
	if ( !m_bossCallSummonWwwEffect.shader ) return;

	const UINT frameIndex = m_nFrameResourceIndex % kSceneBatchFrameResourceCount;

	ID3D12Resource* vertexBuffer = m_bossCallSummonWwwEffect.vertexBuffer.Resource(frameIndex);
	SwordTrailVertex* mappedVertexBuffer = m_bossCallSummonWwwEffect.vertexBuffer.Mapped(frameIndex);
	const UINT bossCallSummonWwwVertexBufferCapacity = m_bossCallSummonWwwEffect.vertexBuffer.Capacity();

	if ( !vertexBuffer ) return;
	if ( !mappedVertexBuffer ) return;
	if ( bossCallSummonWwwVertexBufferCapacity == 0 ) return;

	struct DrawRange
	{
		UINT startVertex = 0;
		UINT vertexCount = 0;
	};

	std::vector<DrawRange> fillRanges;
	std::vector<DrawRange> outlineRanges;

	fillRanges.reserve(m_bossCallSummonWwwEffect.entries.size());
	outlineRanges.reserve(m_bossCallSummonWwwEffect.entries.size());

	UINT vertexCursor = 0;

	auto Smooth01 =
		[ ] (float x) -> float
		{
			x = std::clamp(x, 0.0f, 1.0f);
			return x * x * ( 3.0f - 2.0f * x );
		};

	auto ComputePoint =
		[ ](
			const BossCallSummonWwwEntry& entry,
			UINT wrappedPoint
		) -> XMFLOAT3
		{
			const UINT basePointCount =
				kBossCallSummonWwwPeakCount * 2;

			wrappedPoint %= basePointCount;

			const bool isPeak =
				( wrappedPoint & 1u ) != 0u;

			const UINT peakIndex =
				( wrappedPoint / 2u ) %
				kBossCallSummonWwwPeakCount;

			const float angle =
				XM_2PI *
				static_cast< float >( wrappedPoint ) /
				static_cast< float >( basePointCount );

			const float cx = std::cos(angle);
			const float sz = std::sin(angle);

			const float y =
				isPeak
				? entry.peakHeights[peakIndex]
				: 0.0f;

			XMFLOAT3 p{};
			p.x = entry.center.x + cx * entry.radius;
			p.y = entry.center.y + y;
			p.z = entry.center.z + sz * entry.radius;

			return p;
		};

	for ( const BossCallSummonWwwEntry& entry : m_bossCallSummonWwwEffect.entries )
	{
		if ( !entry.active )
			continue;

		const UINT neededFillVertices =
			kBossCallSummonWwwFillVertexCount;

		const UINT neededOutlineVertices =
			kBossCallSummonWwwOutlineVertexCount;

		const UINT neededVertices =
			neededFillVertices + neededOutlineVertices;

		if ( vertexCursor + neededVertices > bossCallSummonWwwVertexBufferCapacity )
		{
			break;
		}

		const float ageRatio =
			( entry.lifetime > 1.0e-6f )
			? std::clamp(entry.age / entry.lifetime, 0.0f, 1.0f)
			: 1.0f;

		const float birth =
			Smooth01(ageRatio / 0.12f);

		const float death =
			1.0f -
			Smooth01(( ageRatio - 0.62f ) / 0.38f);

		const float alpha =
			std::clamp(
				entry.color.w * birth * death,
				0.0f,
				1.0f
			);

		if ( alpha <= 0.002f )
			continue;

		const UINT basePointCount =
			kBossCallSummonWwwPeakCount * 2;

		// -----------------------------------------------------------------
		// 1) WWW 내부 채움.
		// 각 peak마다 bottom_i -> peak_i -> bottom_i+1 삼각형을 만든다.
		// RGB는 잔광과 동일하게 쓰고, additive 과노출 방지를 위해 alpha만 낮춘다.
		// -----------------------------------------------------------------
		{
			const UINT fillStartVertex = vertexCursor;

			const float fillAlpha =
				std::clamp(alpha * 0.38f, 0.0f, 1.0f);

			const XMFLOAT4 fillColor(
				entry.color.x,
				entry.color.y,
				entry.color.z,
				fillAlpha
			);

			for ( UINT peakIndex = 0;
				  peakIndex < kBossCallSummonWwwPeakCount;
				  ++peakIndex )
			{
				const UINT bottomAIndex =
					peakIndex * 2u;

				const UINT peakPointIndex =
					peakIndex * 2u + 1u;

				const UINT bottomBIndex =
					( peakIndex * 2u + 2u ) % basePointCount;

				const XMFLOAT3 bottomA =
					ComputePoint(entry, bottomAIndex);

				const XMFLOAT3 peak =
					ComputePoint(entry, peakPointIndex);

				const XMFLOAT3 bottomB =
					ComputePoint(entry, bottomBIndex);

				SwordTrailVertex& v0 =
					mappedVertexBuffer[vertexCursor++];

				v0.position = bottomA;
				v0.uv = XMFLOAT2(1.0f, 0.5f);
				v0.color = fillColor;

				SwordTrailVertex& v1 =
					mappedVertexBuffer[vertexCursor++];

				v1.position = peak;
				v1.uv = XMFLOAT2(1.0f, 0.5f);
				v1.color = fillColor;

				SwordTrailVertex& v2 =
					mappedVertexBuffer[vertexCursor++];

				v2.position = bottomB;
				v2.uv = XMFLOAT2(1.0f, 0.5f);
				v2.color = fillColor;
			}

			DrawRange fillRange{};
			fillRange.startVertex = fillStartVertex;
			fillRange.vertexCount = vertexCursor - fillStartVertex;

			if ( fillRange.vertexCount >= 3 )
				fillRanges.push_back(fillRange);
		}

		// -----------------------------------------------------------------
		// 2) WWW 외곽선.
		// 기존처럼 원 둘레를 따라 두께 있는 TRIANGLESTRIP을 만든다.
		// -----------------------------------------------------------------
		{
			const float halfThickness =
				std::max(0.035f, entry.radius * 0.030f);

			const UINT outlineStartVertex = vertexCursor;

			const XMFLOAT4 outlineColor(
				entry.color.x,
				entry.color.y,
				entry.color.z,
				alpha
			);

			for ( UINT pointIndex = 0;
				  pointIndex < kBossCallSummonWwwPathPointCount;
				  ++pointIndex )
			{
				const UINT wrappedPoint =
					pointIndex % basePointCount;

				const float angle =
					XM_2PI *
					static_cast< float >(wrappedPoint) /
					static_cast< float >(basePointCount);

				const float cx = std::cos(angle);
				const float sz = std::sin(angle);

				const XMFLOAT3 radial(cx, 0.0f, sz);
				const XMFLOAT3 p =
					ComputePoint(entry, wrappedPoint);

				SwordTrailVertex& v0 =
					mappedVertexBuffer[vertexCursor++];

				v0.position = XMFLOAT3(
					p.x - radial.x * halfThickness,
					p.y,
					p.z - radial.z * halfThickness
				);

				v0.uv = XMFLOAT2(1.0f, 0.0f);
				v0.color = outlineColor;

				SwordTrailVertex& v1 =
					mappedVertexBuffer[vertexCursor++];

				v1.position = XMFLOAT3(
					p.x + radial.x * halfThickness,
					p.y,
					p.z + radial.z * halfThickness
				);

				v1.uv = XMFLOAT2(1.0f, 1.0f);
				v1.color = outlineColor;
			}

			DrawRange outlineRange{};
			outlineRange.startVertex = outlineStartVertex;
			outlineRange.vertexCount = vertexCursor - outlineStartVertex;

			if ( outlineRange.vertexCount >= 4 )
				outlineRanges.push_back(outlineRange);
		}
	}

	if ( fillRanges.empty() && outlineRanges.empty() )
		return;

	m_bossCallSummonWwwEffect.shader->Render(cmd, camera, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView{};
	vbView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vbView.SizeInBytes = sizeof(SwordTrailVertex) * vertexCursor;
	vbView.StrideInBytes = sizeof(SwordTrailVertex);

	cmd->IASetVertexBuffers(0, 1, &vbView);
	cmd->IASetIndexBuffer(nullptr);

	// 내부 면 먼저 그림.
	if ( !fillRanges.empty() )
	{
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for ( const DrawRange& range : fillRanges )
		{
			if ( range.vertexCount < 3 )
				continue;

			cmd->DrawInstanced(range.vertexCount, 1, range.startVertex, 0);
		}
	}

	// 외곽선은 나중에 그려서 WWW 윤곽이 살아나게 한다.
	if ( !outlineRanges.empty() )
	{
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		for ( const DrawRange& range : outlineRanges )
		{
			if ( range.vertexCount < 4 )
				continue;

			cmd->DrawInstanced(range.vertexCount, 1, range.startVertex, 0);
		}
	}
}

void CGameScene::SpawnBossSummonCircle(const XMFLOAT3& center, float alpha)
{
	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( item.kind != EItemBillboardKind::BossSummonCircle )
			continue;

		const XMFLOAT3 fixedCenter = AlignPositionYToTerrainGround(center, 0.0f);

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
	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( item.kind != EItemBillboardKind::BossSummonGlow )
			continue;

		const XMFLOAT3 fixedCenter = AlignPositionYToTerrainGround(center, 0.0f);

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

	m_itemBillboardState.bossCallSummonCircleVisual.alpha = alpha;
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
	m_itemBillboardState.activeBossCallSummonCircleItemIndices.clear();

	for ( ItemBillboardEntry& item : m_itemBillboardState.entries )
	{
		if ( item.kind != EItemBillboardKind::BossCallSummonCircle )
			continue;

		item.active = false;
		item.distanceCulled = true;
		item.width = 0.0f;
		item.height = 0.0f;
	}

	m_itemBillboardState.bossCallSummonCircleVisual = BossCallSummonCircleVisualState{};
	m_itemBillboardState.bossCallSummonGlowParticleEmitAccumulatorSec = 0.0f;

	SetBossCallSummonCircleAlpha(0.0f);
}

void CGameScene::AddBossCallSummonCircle(const XMFLOAT3& center, EEnemySpawnerEnemyKind kind)
{
	const float size = GetBossCallSummonCircleSize(kind);

	for ( size_t i = 0; i < m_itemBillboardState.entries.size(); ++i )
	{
		ItemBillboardEntry& item = m_itemBillboardState.entries[i];

		if ( item.kind != EItemBillboardKind::BossCallSummonCircle )
			continue;

		if ( item.active )
			continue;

		const XMFLOAT3 fixedCenter = AlignPositionYToTerrainGround(center, 0.0f);

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

		m_itemBillboardState.activeBossCallSummonCircleItemIndices.push_back(i);
		return;
	}

	OutputDebugStringA("[BossCallSummonCircle] add failed: no free BossCallSummonCircle entry.\n");
}

#ifdef USING_NETWORK
XMFLOAT3 CGameScene::ComputeNetworkBossCallSummonVisualPosition(
	EEnemySpawnerEnemyKind kind,
	int index,
	int totalCount) const
{
	const float angleCount =
		static_cast< float >( std::max(1, totalCount) );

	const float angle =
		XM_2PI *
		static_cast< float >( std::max(0, index) ) /
		angleCount;

	float radius = 36.0f;

	switch ( kind )
	{
	case EEnemySpawnerEnemyKind::BowMan:
		radius = 52.0f;
		break;

	case EEnemySpawnerEnemyKind::SwordMan:
		radius = 44.0f;
		break;

	case EEnemySpawnerEnemyKind::Mutant:
		radius = 26.0f;
		break;

	case EEnemySpawnerEnemyKind::Ghoul:
	default:
		radius = 34.0f;
		break;
	}

	XMFLOAT3 pos = XMFLOAT3(0.0f, 0.0f, 400.0f);
	pos.x += std::cos(angle) * radius;
	pos.z += std::sin(angle) * radius;

	return AlignPositionYToTerrainGround(pos, 0.0f);
}

void CGameScene::BeginNetworkBossCallMonsterSummonVisuals(
	int callIndex,
	float fadeInDurationSec)
{
	ClearBossCallSummonCircleVisuals();
	m_networkBossCallSummonVisualPreviews.clear();

	if ( callIndex < 1 || callIndex > 3 )
		return;

	auto AddVisualKind =
		[ this ](EEnemySpawnerEnemyKind kind, int count)
		{
			if ( count <= 0 )
				return;

			for ( int i = 0; i < count; ++i )
			{
				BossCallSummonVisualPreview preview{};
				preview.kind = kind;
				preview.position =
					ComputeNetworkBossCallSummonVisualPosition(kind, i, count);

				m_networkBossCallSummonVisualPreviews.push_back(preview);
				AddBossCallSummonCircle(preview.position, preview.kind);
			}
		};

	switch ( callIndex )
	{
	case 1:
		AddVisualKind(EEnemySpawnerEnemyKind::Ghoul, 30);
		break;

	case 2:
		AddVisualKind(EEnemySpawnerEnemyKind::Ghoul, 20);
		AddVisualKind(EEnemySpawnerEnemyKind::BowMan, 5);
		AddVisualKind(EEnemySpawnerEnemyKind::SwordMan, 5);
		break;

	case 3:
		AddVisualKind(EEnemySpawnerEnemyKind::Ghoul, 20);
		AddVisualKind(EEnemySpawnerEnemyKind::BowMan, 5);
		AddVisualKind(EEnemySpawnerEnemyKind::SwordMan, 5);
		AddVisualKind(EEnemySpawnerEnemyKind::Mutant, 5);
		break;

	default:
		break;
	}

	if ( m_itemBillboardState.activeBossCallSummonCircleItemIndices.empty() )
		return;

	m_itemBillboardState.bossCallSummonCircleVisual =
		BossCallSummonCircleVisualState{};
	m_itemBillboardState.bossCallSummonCircleVisual.active = true;
	m_itemBillboardState.bossCallSummonCircleVisual.fadingIn = true;
	m_itemBillboardState.bossCallSummonCircleVisual.fadingOut = false;
	m_itemBillboardState.bossCallSummonCircleVisual.ageSec = 0.0f;
	m_itemBillboardState.bossCallSummonCircleVisual.durationSec =
		( fadeInDurationSec > 1.0e-6f )
		? fadeInDurationSec
		: 0.001f;
	m_itemBillboardState.bossCallSummonCircleVisual.alpha = 0.0f;

	SetBossCallSummonCircleAlpha(0.0f);

	char buf[256];
	sprintf_s(
		buf,
		"[NetworkBossCallCircle] call=%d previews=%zu activeCircles=%zu\n",
		callIndex,
		m_networkBossCallSummonVisualPreviews.size(),
		m_itemBillboardState.activeBossCallSummonCircleItemIndices.size()
	);
	OutputDebugStringA(buf);
}
#endif

void CGameScene::BeginBossCallMonsterSummonVisuals(int callIndex, float fadeInDurationSec)
{
	ClearBossCallSummonCircleVisuals();

	m_bossCallSummonPlanCallIndex = -1;
	m_bossCallSummonPlanEntries.clear();

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

			const int found = PeekLogicalSpawnerEntries(megaGridNumber, kind, count, previews);

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

	if ( m_itemBillboardState.activeBossCallSummonCircleItemIndices.empty() )
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

	m_itemBillboardState.bossCallSummonCircleVisual = BossCallSummonCircleVisualState{};
	m_itemBillboardState.bossCallSummonCircleVisual.active = true;
	m_itemBillboardState.bossCallSummonCircleVisual.fadingIn = true;
	m_itemBillboardState.bossCallSummonCircleVisual.fadingOut = false;
	m_itemBillboardState.bossCallSummonCircleVisual.ageSec = 0.0f;
	m_itemBillboardState.bossCallSummonCircleVisual.durationSec =
		( fadeInDurationSec > 1.0e-6f ) ? fadeInDurationSec : 0.001f;
	m_itemBillboardState.bossCallSummonCircleVisual.alpha = 0.0f;

	SetBossCallSummonCircleAlpha(0.0f);

	{
		XMFLOAT3 sfxPos = AlignPositionYToTerrainGround(XMFLOAT3(400.0f, 0.0f, 400.0f), 0.0f);

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

	}
}

void CGameScene::StartBossCallSummonCircleFadeOut()
{
	if ( !m_itemBillboardState.bossCallSummonCircleVisual.active )
		return;

	if ( m_itemBillboardState.activeBossCallSummonCircleItemIndices.empty() )
		return;

	m_itemBillboardState.bossCallSummonCircleVisual.fadingIn = false;
	m_itemBillboardState.bossCallSummonCircleVisual.fadingOut = true;
	m_itemBillboardState.bossCallSummonCircleVisual.ageSec = 0.0f;
	m_itemBillboardState.bossCallSummonCircleVisual.durationSec =
		( kBossCallSummonCircleFadeOutDurationSec > 1.0e-6f )
		? kBossCallSummonCircleFadeOutDurationSec
		: 0.001f;
}

void CGameScene::EmitBossCallSummonCircleGlowParticles(
	float dt,
	float alpha)
{
	if ( dt <= 0.0f )
		return;

	if ( alpha <= 0.001f )
		return;

	if ( m_itemBillboardState.activeBossCallSummonCircleItemIndices.empty() )
		return;

	const float emitIntervalSec =
		( kBossCallSummonGlowParticleEmitIntervalSec > 1.0e-6f )
		? kBossCallSummonGlowParticleEmitIntervalSec
		: 0.001f;

	m_itemBillboardState.bossCallSummonGlowParticleEmitAccumulatorSec += dt;

	if ( m_itemBillboardState.bossCallSummonGlowParticleEmitAccumulatorSec < emitIntervalSec )
		return;

	int emitCount =
		static_cast< int >(
			m_itemBillboardState.bossCallSummonGlowParticleEmitAccumulatorSec /
			emitIntervalSec
		);

	m_itemBillboardState.bossCallSummonGlowParticleEmitAccumulatorSec =
		std::fmod(
			m_itemBillboardState.bossCallSummonGlowParticleEmitAccumulatorSec,
			emitIntervalSec
		);

	if ( emitCount > 3 )
		emitCount = 3;

	for ( size_t itemIndex : m_itemBillboardState.activeBossCallSummonCircleItemIndices )
	{
		if ( itemIndex >= m_itemBillboardState.entries.size() )
			continue;

		const ItemBillboardEntry& item = m_itemBillboardState.entries[itemIndex];

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
					kBossCallSummonAfterimageParticleSizeScale,
					kBossCallSummonGlowParticleLifetimeScale
				);
			}
		}
	}
}

void CGameScene::UpdateBossCallSummonCircles(float dt)
{
	if ( !m_itemBillboardState.bossCallSummonCircleVisual.active )
		return;

	if ( dt < 0.0f )
		dt = 0.0f;

	BossCallSummonCircleVisualState& state =
		m_itemBillboardState.bossCallSummonCircleVisual;

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

	// fade-in 완료 후 실제 몬스터 생성 전까지도
	// 마법진 잔광을 계속 발생시킨다.
	if ( state.active )
	{
		SetBossCallSummonCircleAlpha(1.0f);
		EmitBossCallSummonCircleGlowParticles(dt, 1.0f);
		return;
	}

}
