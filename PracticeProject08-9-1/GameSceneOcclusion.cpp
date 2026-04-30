//-----------------------------------------------------------------------------
// File: GameSceneOcclusion.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScene.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "Camera.h"
#include "ColliderComponent.h"
#include "GlobalValues.h"
#include "Mesh.h"
#include "Object.h"

namespace
{
	static XMFLOAT4X4 BuildOcclusionWorldMatrixFromOOBB(const BoundingOrientedBox& box)
	{
		XMFLOAT4X4 out{};

		const XMMATRIX S = XMMatrixScaling(
			box.Extents.x * 2.0f,
			box.Extents.y * 2.0f,
			box.Extents.z * 2.0f
		);

		const XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&box.Orientation));

		const XMMATRIX T = XMMatrixTranslation(
			box.Center.x,
			box.Center.y,
			box.Center.z
		);

		XMStoreFloat4x4(&out, S * R * T);
		return out;
	}

	static std::shared_ptr<CMesh> CreateStaticOcclusionLocalUnitBoxMesh(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd)
	{
		if ( !dev || !cmd )
			return nullptr;

		struct OcclusionVertex
		{
			XMFLOAT3 position;
			XMFLOAT3 normal;
			XMFLOAT2 uv;
			XMFLOAT4 tangent;
		};

		const OcclusionVertex vertices[ ] =
		{
			{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },

			{ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, +1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT4(-1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, +1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT4(-1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, +1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT4(-1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, +1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT4(-1.0f, 0.0f, 0.0f, 1.0f) },

			{ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, -1.0f, 1.0f) },
			{ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, -1.0f, 1.0f) },
			{ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, -1.0f, 1.0f) },
			{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, -1.0f, 1.0f) },

			{ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(+1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, +1.0f, 1.0f) },
			{ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(+1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, +1.0f, 1.0f) },
			{ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(+1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT4(0.0f, 0.0f, +1.0f, 1.0f) },
			{ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(+1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, +1.0f, 1.0f) },

			{ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, +1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, +1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, +1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, +1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },

			{ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		};

		const UINT indices[ ] =
		{
			0, 1, 2, 0, 2, 3,
			4, 5, 6, 4, 6, 7,
			8, 9,10, 8,10,11,
			12,13,14,12,14,15,
			16,17,18,16,18,19,
			20,21,22,20,22,23
		};

		auto mesh = std::make_shared<CMesh>(dev, cmd);
		mesh->m_SubMeshes.resize(1);

		SubMesh& sm = mesh->m_SubMeshes[0];

		sm.positions.reserve(_countof(vertices));
		sm.normals.reserve(_countof(vertices));
		sm.uvs.reserve(_countof(vertices));
		sm.tangents.reserve(_countof(vertices));

		for ( const OcclusionVertex& v : vertices )
		{
			sm.positions.push_back(v.position);
			sm.normals.push_back(v.normal);
			sm.uvs.push_back(v.uv);
			sm.tangents.push_back(v.tangent);
		}

		sm.indices.assign(std::begin(indices), std::end(indices));

		sm.subMeshMin = XMFLOAT3(-0.5f, -0.5f, -0.5f);
		sm.subMeshMax = XMFLOAT3(+0.5f, +0.5f, +0.5f);

		const UINT vertexBufferSize = sizeof(OcclusionVertex) * _countof(vertices);
		const UINT indexBufferSize = sizeof(UINT) * _countof(indices);

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
		sm.vbView.SizeInBytes = vertexBufferSize;
		sm.vbView.StrideInBytes = sizeof(OcclusionVertex);

		sm.ibView.BufferLocation = sm.ib->GetGPUVirtualAddress();
		sm.ibView.SizeInBytes = indexBufferSize;
		sm.ibView.Format = DXGI_FORMAT_R32_UINT;

		return mesh;
	}

	static bool TryBuildStaticOcclusionWorldBounds(
		CGameObject* obj,
		BoundingOrientedBox& outBounds)
	{
		if ( !obj )
			return false;

		if ( auto* collider = obj->GetComponent<CColliderComponent>() )
		{
			const std::vector<MeshOOBBSet>& meshSets = collider->GetMeshOOBBSets();

			bool hasAnyCorner = false;
			float minX = 0.0f;
			float maxX = 0.0f;
			float minY = 0.0f;
			float maxY = 0.0f;
			float minZ = 0.0f;
			float maxZ = 0.0f;

			for ( const MeshOOBBSet& set : meshSets )
			{
				for ( const BoundingOrientedBox& subOOBB : set.WorldSubOOBBs )
				{
					XMFLOAT3 corners[8] = {};
					subOOBB.GetCorners(corners);

					for ( int i = 0; i < 8; ++i )
					{
						const XMFLOAT3& c = corners[i];

						if ( !hasAnyCorner )
						{
							minX = maxX = c.x;
							minY = maxY = c.y;
							minZ = maxZ = c.z;
							hasAnyCorner = true;
						}
						else
						{
							if ( c.x < minX ) minX = c.x;
							if ( c.x > maxX ) maxX = c.x;
							if ( c.y < minY ) minY = c.y;
							if ( c.y > maxY ) maxY = c.y;
							if ( c.z < minZ ) minZ = c.z;
							if ( c.z > maxZ ) maxZ = c.z;
						}
					}
				}
			}

			if ( hasAnyCorner )
			{
				outBounds.Center = XMFLOAT3(
					( minX + maxX ) * 0.5f,
					( minY + maxY ) * 0.5f,
					( minZ + maxZ ) * 0.5f
				);

				outBounds.Extents = XMFLOAT3(
					( maxX - minX ) * 0.5f,
					( maxY - minY ) * 0.5f,
					( maxZ - minZ ) * 0.5f
				);

				outBounds.Orientation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
				return true;
			}
		}

		const XMFLOAT3 pos = obj->GetPosition();

		outBounds.Center = pos;
		outBounds.Extents = XMFLOAT3(0.5f, 0.5f, 0.5f);
		outBounds.Orientation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		return true;
	}

	static bool IsSkinnedOcclusionTargetAssetName(const std::string& assetName)
	{
		return
			( assetName == "Ghoul" ) ||
			( assetName == "SwordMan" ) ||
			( assetName == "BowMan" ) ||
			( assetName == "Mutant" ) ||
			( assetName == "Boss" );
	}

	static XMFLOAT3 GetSkinnedOcclusionExtentsByAssetName(const std::string& assetName)
	{
		if ( assetName == "Ghoul" )
			return XMFLOAT3(0.75f, 1.35f, 0.75f);

		if ( assetName == "SwordMan" )
			return XMFLOAT3(0.75f, 1.45f, 0.75f);

		if ( assetName == "BowMan" )
			return XMFLOAT3(0.75f, 1.45f, 0.75f);

		if ( assetName == "Mutant" )
			return XMFLOAT3(1.20f, 1.80f, 1.20f);

		if ( assetName == "Boss" )
			return XMFLOAT3(1.80f, 2.50f, 1.80f);

		return XMFLOAT3(0.80f, 1.50f, 0.80f);
	}

	static bool TryBuildSkinnedOcclusionWorldBounds(
		CGameObject* obj,
		const std::string& assetName,
		BoundingOrientedBox& outBounds)
	{
		if ( !obj )
			return false;

		const XMFLOAT3 position = obj->GetPosition();
		const XMFLOAT3 extents = GetSkinnedOcclusionExtentsByAssetName(assetName);

		outBounds.Center = XMFLOAT3(
			position.x,
			position.y + extents.y,
			position.z
		);

		outBounds.Extents = extents;
		outBounds.Orientation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		return true;
	}
}

void CGameScene::ResetStaticOcclusionEntries()
{
	m_staticOcclusionEntries.clear();
	m_staticOcclusionCullFlags.clear();
	m_staticOcclusionQuerySampleCounts.clear();
	m_staticOcclusionLastFrameIssuedFlags.clear();
	m_staticOcclusionCurrentFrameIssuedFlags.clear();
	m_staticOcclusionZeroSampleFrameCounts.clear();
	m_staticOcclusionQueryCapacity = 0;
	m_bStaticOcclusionQueryResultsValid = false;
}

void CGameScene::BuildStaticOcclusionEntries()
{
	ResetStaticOcclusionEntries();

	m_staticOcclusionCullFlags.assign(m_staticBatch.objectRefs.size(), 0);

	for ( const StaticWorldLodEntry& lodEntry : m_staticWorldLodEntries )
	{
		if ( !lodEntry.object )
			continue;

		if ( lodEntry.staticBatchObjectIndex == UINT_MAX )
			continue;

		if ( lodEntry.staticBatchObjectIndex >= ( UINT ) m_staticBatch.objectRefs.size() )
			continue;

		const std::string& assetName = lodEntry.assetName;

		const bool isOcclusionTarget =
			( assetName == "VillageWall" ) ||
			( assetName == "Castle" ) ||
			( assetName == "Tower" ) ||
			( assetName == "Building1" ) ||
			( assetName == "Building2" ) ||
			( assetName == "Building3" ) ||
			( assetName == "Building4" ) ||
			( assetName == "Building5" ) ||
			( assetName == "Building6" ) ||
			( assetName == "Building7" ) ||
			( assetName == "Building8" ) ||
			( assetName == "Building9" );

		if ( !isOcclusionTarget )
			continue;

		StaticOcclusionEntry entry{};
		entry.object = lodEntry.object;
		entry.staticBatchObjectIndex = lodEntry.staticBatchObjectIndex;
		entry.assetName = assetName;
		entry.enabled = true;
		entry.hasWorldBounds =
			TryBuildStaticOcclusionWorldBounds(entry.object, entry.worldBounds);

		m_staticOcclusionEntries.push_back(std::move(entry));
	}
}

void CGameScene::BuildStaticOcclusionUnitBoxMesh(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd)
{
	m_staticOcclusionUnitBoxMesh.reset();

	if ( !dev || !cmd )
		return;

	m_staticOcclusionUnitBoxMesh =
		CreateStaticOcclusionLocalUnitBoxMesh(dev, cmd);
}

void CGameScene::BuildStaticOcclusionGpuResources(ID3D12Device* dev)
{
	ReleaseStaticOcclusionGpuResources();

	if ( !dev )
		return;

	const UINT queryCount = ( UINT ) m_staticOcclusionEntries.size();
	m_staticOcclusionQueryCapacity = queryCount;
	m_staticOcclusionQuerySampleCounts.assign(queryCount, 1ull);
	m_staticOcclusionLastFrameIssuedFlags.assign(queryCount, 0);
	m_staticOcclusionCurrentFrameIssuedFlags.assign(queryCount, 0);
	m_staticOcclusionZeroSampleFrameCounts.assign(queryCount, 0);
	m_bStaticOcclusionQueryResultsValid = false;

	if ( queryCount == 0 )
		return;

	D3D12_QUERY_HEAP_DESC queryHeapDesc{};
	queryHeapDesc.Count = queryCount;
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	queryHeapDesc.NodeMask = 0;

	HRESULT hr = dev->CreateQueryHeap(
		&queryHeapDesc,
		IID_PPV_ARGS(m_pd3dStaticOcclusionQueryHeap.ReleaseAndGetAddressOf())
	);

	if ( FAILED(hr) )
	{
		OutputDebugStringA("[Occlusion] CreateQueryHeap failed.\n");
		return;
	}

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_READBACK;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC bufferDesc{};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Alignment = 0;
	bufferDesc.Width = sizeof(UINT64) * queryCount;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.SampleDesc.Quality = 0;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = dev->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(m_pd3dStaticOcclusionReadbackBuffer.ReleaseAndGetAddressOf())
	);

	if ( FAILED(hr) )
	{
		OutputDebugStringA("[Occlusion] Create readback buffer failed.\n");
		ReleaseStaticOcclusionGpuResources();
		return;
	}

	D3D12_RANGE readRange{};
	readRange.Begin = 0;
	readRange.End = 0;

	hr = m_pd3dStaticOcclusionReadbackBuffer->Map(
		0,
		&readRange,
		reinterpret_cast< void** >( &m_pMappedStaticOcclusionReadbackBuffer )
	);

	if ( FAILED(hr) || !m_pMappedStaticOcclusionReadbackBuffer )
	{
		OutputDebugStringA("[Occlusion] Map readback buffer failed.\n");
		ReleaseStaticOcclusionGpuResources();
		return;
	}

	D3D12_HEAP_PROPERTIES uploadHeapProps{};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHeapProps.CreationNodeMask = 1;
	uploadHeapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC instanceBufferDesc{};
	instanceBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	instanceBufferDesc.Alignment = 0;
	instanceBufferDesc.Width = sizeof(StaticInstanceVertex) * queryCount;
	instanceBufferDesc.Height = 1;
	instanceBufferDesc.DepthOrArraySize = 1;
	instanceBufferDesc.MipLevels = 1;
	instanceBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	instanceBufferDesc.SampleDesc.Count = 1;
	instanceBufferDesc.SampleDesc.Quality = 0;
	instanceBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	instanceBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = dev->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&instanceBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_pd3dStaticOcclusionInstanceBuffer.ReleaseAndGetAddressOf())
	);

	if ( FAILED(hr) )
	{
		OutputDebugStringA("[Occlusion] Create occlusion instance buffer failed.\n");
		ReleaseStaticOcclusionGpuResources();
		return;
	}

	hr = m_pd3dStaticOcclusionInstanceBuffer->Map(
		0,
		nullptr,
		reinterpret_cast< void** >( &m_pMappedStaticOcclusionInstanceBuffer )
	);

	if ( FAILED(hr) || !m_pMappedStaticOcclusionInstanceBuffer )
	{
		OutputDebugStringA("[Occlusion] Map occlusion instance buffer failed.\n");
		ReleaseStaticOcclusionGpuResources();
		return;
	}

	m_bStaticOcclusionQueryResourcesReady = true;
}

void CGameScene::ReleaseStaticOcclusionGpuResources()
{
	if ( m_pd3dStaticOcclusionInstanceBuffer )
	{
		if ( m_pMappedStaticOcclusionInstanceBuffer )
		{
			m_pd3dStaticOcclusionInstanceBuffer->Unmap(0, nullptr);
			m_pMappedStaticOcclusionInstanceBuffer = nullptr;
		}

		m_pd3dStaticOcclusionInstanceBuffer.Reset();
	}

	if ( m_pd3dStaticOcclusionReadbackBuffer )
	{
		if ( m_pMappedStaticOcclusionReadbackBuffer )
		{
			m_pd3dStaticOcclusionReadbackBuffer->Unmap(0, nullptr);
			m_pMappedStaticOcclusionReadbackBuffer = nullptr;
		}

		m_pd3dStaticOcclusionReadbackBuffer.Reset();
	}

	if ( m_pd3dStaticOcclusionQueryHeap )
		m_pd3dStaticOcclusionQueryHeap.Reset();

	m_staticOcclusionQueryCapacity = 0;
	m_bStaticOcclusionQueryResourcesReady = false;
	m_bStaticOcclusionQueryResultsValid = false;
	m_staticOcclusionQuerySampleCounts.clear();
	m_staticOcclusionLastFrameIssuedFlags.clear();
	m_staticOcclusionCurrentFrameIssuedFlags.clear();
	m_staticOcclusionZeroSampleFrameCounts.clear();
}

void CGameScene::BeginStaticOcclusionReadback()
{
	if ( !m_bStaticOcclusionQueryResourcesReady )
		return;

	if ( !m_bStaticOcclusionQueryResultsValid )
		return;

	if ( !m_pMappedStaticOcclusionReadbackBuffer )
		return;

	const UINT queryCount = ( UINT ) m_staticOcclusionEntries.size();
	if ( queryCount == 0 )
		return;

	if ( m_staticOcclusionQuerySampleCounts.size() != queryCount )
		m_staticOcclusionQuerySampleCounts.assign(queryCount, 1ull);

	for ( UINT i = 0; i < queryCount; ++i )
	{
		m_staticOcclusionQuerySampleCounts[i] =
			m_pMappedStaticOcclusionReadbackBuffer[i];
	}
}

void CGameScene::ResolveStaticOcclusionQueries(ID3D12GraphicsCommandList* cmd)
{
	if ( !cmd )
		return;

	if ( !m_bStaticOcclusionQueryResourcesReady )
		return;

	const UINT queryCount = ( UINT ) m_staticOcclusionEntries.size();
	if ( queryCount == 0 )
		return;

	cmd->ResolveQueryData(
		m_pd3dStaticOcclusionQueryHeap.Get(),
		D3D12_QUERY_TYPE_OCCLUSION,
		0,
		queryCount,
		m_pd3dStaticOcclusionReadbackBuffer.Get(),
		0
	);

	m_staticOcclusionLastFrameIssuedFlags = m_staticOcclusionCurrentFrameIssuedFlags;
	m_bStaticOcclusionQueryResultsValid = true;
}

void CGameScene::RenderStaticOcclusionPass(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd )
		return;

	if ( !camera )
		return;

	if ( !m_bStaticOcclusionCullingEnabled )
		return;

	if ( !m_bStaticOcclusionQueryResourcesReady )
		return;

	if ( m_staticOcclusionEntries.empty() )
		return;

	if ( !m_staticOcclusionUnitBoxMesh )
		return;

	if ( !m_occlusionStaticShader )
		return;

	if ( !m_bSceneRenderTargetsReady )
		return;

	if ( !m_pd3dStaticOcclusionInstanceBuffer )
		return;

	if ( !m_pMappedStaticOcclusionInstanceBuffer )
		return;

	if ( m_staticOcclusionUnitBoxMesh->m_SubMeshes.empty() )
		return;

	const SubMesh& sm = m_staticOcclusionUnitBoxMesh->m_SubMeshes[0];
	if ( sm.indices.empty() )
		return;

	cmd->SetGraphicsRootSignature(GetGraphicsRootSignature());

	if ( m_pDescriptorHeap && m_pDescriptorHeap->m_pd3dCbvSrvDescriptorHeap )
	{
		cmd->SetDescriptorHeaps(1, m_pDescriptorHeap->m_pd3dCbvSrvDescriptorHeap.GetAddressOf());
		cmd->SetGraphicsRootDescriptorTable(
			ROOT_PARAMETER_GLOBAL_SRV,
			m_pDescriptorHeap->GetGPUSrvDescriptorStartHandle()
		);
	}

	camera->SetViewportsAndScissorRects(cmd);

	cmd->OMSetRenderTargets(
		0,
		nullptr,
		FALSE,
		&m_sceneDsvHandle
	);

	m_occlusionStaticShader->Render(cmd, camera, &m_staticBatch);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_staticOcclusionCurrentFrameIssuedFlags.assign(m_staticOcclusionEntries.size(), 0);

	const XMFLOAT3 cameraPosition = camera->GetPosition();
	const float minTestDistanceSq =
		m_staticOcclusionMinTestDistance * m_staticOcclusionMinTestDistance;

	for ( UINT queryIndex = 0; queryIndex < ( UINT ) m_staticOcclusionEntries.size(); ++queryIndex )
	{
		const StaticOcclusionEntry& entry = m_staticOcclusionEntries[queryIndex];

		bool issueRealQuery = true;

		if ( !entry.enabled )
			issueRealQuery = false;

		if ( !entry.hasWorldBounds )
			issueRealQuery = false;

		if ( !entry.object )
			issueRealQuery = false;

		if ( entry.staticBatchObjectIndex == UINT_MAX )
			issueRealQuery = false;

		if ( entry.staticBatchObjectIndex >= ( UINT ) m_staticBatch.objectRefs.size() )
			issueRealQuery = false;

		if ( entry.staticBatchObjectIndex < ( UINT ) m_staticDistanceCullFlags.size() )
		{
			if ( m_staticDistanceCullFlags[entry.staticBatchObjectIndex] != 0 )
				issueRealQuery = false;
		}

		if ( issueRealQuery )
		{
			if ( !entry.object->IsVisible(camera) )
				issueRealQuery = false;
		}

		if ( issueRealQuery )
		{
			const float dx = cameraPosition.x - entry.worldBounds.Center.x;
			const float dy = cameraPosition.y - entry.worldBounds.Center.y;
			const float dz = cameraPosition.z - entry.worldBounds.Center.z;
			const float distSq = dx * dx + dy * dy + dz * dz;

			if ( distSq < minTestDistanceSq )
			{
				issueRealQuery = false;
			}
			else
			{
				const float dist = std::sqrt(distSq);
				float maxExtent = entry.worldBounds.Extents.x;
				if (entry.worldBounds.Extents.y > maxExtent) maxExtent = entry.worldBounds.Extents.y;
				if (entry.worldBounds.Extents.z > maxExtent) maxExtent = entry.worldBounds.Extents.z;

				if ( dist > 0.0001f )
				{
					const float extentDistanceRatio = maxExtent / dist;

					if ( extentDistanceRatio > m_staticOcclusionMaxCullExtentDistanceRatio )
						issueRealQuery = false;
				}
			}
		}

		cmd->BeginQuery(
			m_pd3dStaticOcclusionQueryHeap.Get(),
			D3D12_QUERY_TYPE_OCCLUSION,
			queryIndex
		);

		if ( issueRealQuery )
		{
			m_staticOcclusionCurrentFrameIssuedFlags[queryIndex] = 1;

			const XMFLOAT4X4 world = BuildOcclusionWorldMatrixFromOOBB(entry.worldBounds);

			StaticInstanceVertex& dst = m_pMappedStaticOcclusionInstanceBuffer[queryIndex];
			ZeroMemory(&dst, sizeof(dst));

			dst.world0 = XMFLOAT4(world._11, world._12, world._13, world._14);
			dst.world1 = XMFLOAT4(world._21, world._22, world._23, world._24);
			dst.world2 = XMFLOAT4(world._31, world._32, world._33, world._34);
			dst.world3 = XMFLOAT4(world._41, world._42, world._43, world._44);
			dst.objectId = 0;

			D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
			vbViews[0] = sm.vbView;
			vbViews[1].BufferLocation =
				m_pd3dStaticOcclusionInstanceBuffer->GetGPUVirtualAddress()
				+ sizeof(StaticInstanceVertex) * queryIndex;
			vbViews[1].SizeInBytes = sizeof(StaticInstanceVertex);
			vbViews[1].StrideInBytes = sizeof(StaticInstanceVertex);

			cmd->SetGraphicsRoot32BitConstant(ROOT_PARAMETER_MATERIAL_ID, 0u, 0);
			cmd->IASetVertexBuffers(0, 2, vbViews);
			cmd->IASetIndexBuffer(&sm.ibView);

			cmd->DrawIndexedInstanced(
				( UINT ) sm.indices.size(),
				1,
				0,
				0,
				0
			);
		}

		cmd->EndQuery(
			m_pd3dStaticOcclusionQueryHeap.Get(),
			D3D12_QUERY_TYPE_OCCLUSION,
			queryIndex
		);
	}

	ResolveStaticOcclusionQueries(cmd);
	RestoreSceneRenderTargets(cmd, camera);
}

void CGameScene::RenderSkinnedOcclusionPass(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd )
		return;

	if ( !camera )
		return;

	if ( !m_bSkinnedOcclusionCullingEnabled )
		return;

	if ( !m_bSkinnedOcclusionQueryResourcesReady )
		return;

	if ( m_skinnedOcclusionEntries.empty() )
		return;

	if ( !m_staticOcclusionUnitBoxMesh )
		return;

	if ( !m_occlusionStaticShader )
		return;

	if ( !m_bSceneRenderTargetsReady )
		return;

	if ( !m_pd3dSkinnedOcclusionInstanceBuffer )
		return;

	if ( !m_pMappedSkinnedOcclusionInstanceBuffer )
		return;

	if ( m_staticOcclusionUnitBoxMesh->m_SubMeshes.empty() )
		return;

	const SubMesh& sm = m_staticOcclusionUnitBoxMesh->m_SubMeshes[0];
	if ( sm.indices.empty() )
		return;

	cmd->SetGraphicsRootSignature(GetGraphicsRootSignature());

	if ( m_pDescriptorHeap && m_pDescriptorHeap->m_pd3dCbvSrvDescriptorHeap )
	{
		cmd->SetDescriptorHeaps(1, m_pDescriptorHeap->m_pd3dCbvSrvDescriptorHeap.GetAddressOf());
		cmd->SetGraphicsRootDescriptorTable(
			ROOT_PARAMETER_GLOBAL_SRV,
			m_pDescriptorHeap->GetGPUSrvDescriptorStartHandle()
		);
	}

	camera->SetViewportsAndScissorRects(cmd);

	cmd->OMSetRenderTargets(
		0,
		nullptr,
		FALSE,
		&m_sceneDsvHandle
	);

	m_occlusionStaticShader->Render(cmd, camera, &m_staticBatch);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_skinnedOcclusionCurrentFrameIssuedFlags.assign(m_skinnedOcclusionEntries.size(), 0);

	const XMFLOAT3 cameraPosition = camera->GetPosition();
	const float minTestDistanceSq =
		m_skinnedOcclusionMinTestDistance * m_skinnedOcclusionMinTestDistance;

	for ( UINT queryIndex = 0; queryIndex < ( UINT ) m_skinnedOcclusionEntries.size(); ++queryIndex )
	{
		SkinnedOcclusionEntry& entry = m_skinnedOcclusionEntries[queryIndex];

		bool issueRealQuery = true;

		if ( !entry.enabled )
			issueRealQuery = false;

		if ( !entry.object )
			issueRealQuery = false;

		if ( entry.skinnedBatchObjectIndex == UINT_MAX )
			issueRealQuery = false;

		if ( entry.skinnedBatchObjectIndex >= ( UINT ) m_skinnedBatch.objectRefs.size() )
			issueRealQuery = false;

		if ( issueRealQuery )
		{
			entry.hasWorldBounds =
				TryBuildSkinnedOcclusionWorldBounds(entry.object, entry.assetName, entry.worldBounds);

			if ( !entry.hasWorldBounds )
				issueRealQuery = false;
		}

		if ( issueRealQuery )
		{
			if ( entry.skinnedBatchObjectIndex < ( UINT ) m_skinnedDistanceCullFlags.size() )
			{
				if ( m_skinnedDistanceCullFlags[entry.skinnedBatchObjectIndex] != 0 )
					issueRealQuery = false;
			}
		}

		if ( issueRealQuery )
		{
			if ( !entry.object->IsVisible(camera) )
				issueRealQuery = false;
		}

		if ( issueRealQuery )
		{
			const float dx = cameraPosition.x - entry.worldBounds.Center.x;
			const float dy = cameraPosition.y - entry.worldBounds.Center.y;
			const float dz = cameraPosition.z - entry.worldBounds.Center.z;
			const float distSq = dx * dx + dy * dy + dz * dz;

			if ( distSq < minTestDistanceSq )
			{
				issueRealQuery = false;
			}
			else
			{
				const float dist = std::sqrt(distSq);
				float maxExtent = entry.worldBounds.Extents.x;
				if (entry.worldBounds.Extents.y > maxExtent) maxExtent = entry.worldBounds.Extents.y;
				if (entry.worldBounds.Extents.z > maxExtent) maxExtent = entry.worldBounds.Extents.z;

				if ( dist > 0.0001f )
				{
					const float extentDistanceRatio = maxExtent / dist;

					if ( extentDistanceRatio > m_skinnedOcclusionMaxCullExtentDistanceRatio )
						issueRealQuery = false;
				}
			}
		}

		cmd->BeginQuery(
			m_pd3dSkinnedOcclusionQueryHeap.Get(),
			D3D12_QUERY_TYPE_OCCLUSION,
			queryIndex
		);

		if ( issueRealQuery )
		{
			m_skinnedOcclusionCurrentFrameIssuedFlags[queryIndex] = 1;

			const XMFLOAT4X4 world = BuildOcclusionWorldMatrixFromOOBB(entry.worldBounds);

			StaticInstanceVertex& dst = m_pMappedSkinnedOcclusionInstanceBuffer[queryIndex];
			ZeroMemory(&dst, sizeof(dst));

			dst.world0 = XMFLOAT4(world._11, world._12, world._13, world._14);
			dst.world1 = XMFLOAT4(world._21, world._22, world._23, world._24);
			dst.world2 = XMFLOAT4(world._31, world._32, world._33, world._34);
			dst.world3 = XMFLOAT4(world._41, world._42, world._43, world._44);
			dst.objectId = 0;

			D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
			vbViews[0] = sm.vbView;
			vbViews[1].BufferLocation =
				m_pd3dSkinnedOcclusionInstanceBuffer->GetGPUVirtualAddress()
				+ sizeof(StaticInstanceVertex) * queryIndex;
			vbViews[1].SizeInBytes = sizeof(StaticInstanceVertex);
			vbViews[1].StrideInBytes = sizeof(StaticInstanceVertex);

			cmd->SetGraphicsRoot32BitConstant(ROOT_PARAMETER_MATERIAL_ID, 0u, 0);
			cmd->IASetVertexBuffers(0, 2, vbViews);
			cmd->IASetIndexBuffer(&sm.ibView);

			cmd->DrawIndexedInstanced(
				( UINT ) sm.indices.size(),
				1,
				0,
				0,
				0
			);
		}

		cmd->EndQuery(
			m_pd3dSkinnedOcclusionQueryHeap.Get(),
			D3D12_QUERY_TYPE_OCCLUSION,
			queryIndex
		);
	}

	ResolveSkinnedOcclusionQueries(cmd);
	RestoreSceneRenderTargets(cmd, camera);
}

void CGameScene::UpdateStaticOcclusionCullSelection(CCamera* camera)
{
	m_staticOcclusionCullFlags.assign(m_staticBatch.objectRefs.size(), 0);

	if ( !m_bStaticOcclusionCullingEnabled )
		return;

	if ( !camera )
		return;

	const XMFLOAT3 cameraPosition = camera->GetPosition();
	const float minTestDistanceSq =
		m_staticOcclusionMinTestDistance * m_staticOcclusionMinTestDistance;

	for ( size_t occlusionIndex = 0; occlusionIndex < m_staticOcclusionEntries.size(); ++occlusionIndex )
	{
		const StaticOcclusionEntry& entry = m_staticOcclusionEntries[occlusionIndex];

		if ( !entry.enabled )
			continue;

		if ( !entry.hasWorldBounds )
			continue;

		if ( !entry.object )
			continue;

		if ( entry.staticBatchObjectIndex == UINT_MAX )
			continue;

		if ( entry.staticBatchObjectIndex >= ( UINT ) m_staticOcclusionCullFlags.size() )
			continue;

		if ( occlusionIndex >= m_staticOcclusionZeroSampleFrameCounts.size() )
			continue;

		if ( entry.staticBatchObjectIndex < ( UINT ) m_staticDistanceCullFlags.size() )
		{
			if ( m_staticDistanceCullFlags[entry.staticBatchObjectIndex] != 0 )
			{
				m_staticOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
				continue;
			}
		}

		if ( !entry.object->IsVisible(camera) )
		{
			m_staticOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
			continue;
		}

		const float dx = cameraPosition.x - entry.worldBounds.Center.x;
		const float dy = cameraPosition.y - entry.worldBounds.Center.y;
		const float dz = cameraPosition.z - entry.worldBounds.Center.z;
		const float distSq = dx * dx + dy * dy + dz * dz;

		if ( distSq < minTestDistanceSq )
		{
			m_staticOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
			continue;
		}

		const float dist = std::sqrt(distSq);
		float maxExtent = entry.worldBounds.Extents.x;
		if (entry.worldBounds.Extents.y > maxExtent) maxExtent = entry.worldBounds.Extents.y;
		if (entry.worldBounds.Extents.z > maxExtent) maxExtent = entry.worldBounds.Extents.z;

		if ( dist > 0.0001f )
		{
			const float extentDistanceRatio = maxExtent / dist;

			if ( extentDistanceRatio > m_staticOcclusionMaxCullExtentDistanceRatio )
			{
				m_staticOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
				continue;
			}
		}

		if ( !m_bStaticOcclusionQueryResultsValid )
		{
			m_staticOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
			continue;
		}

		if ( occlusionIndex >= m_staticOcclusionQuerySampleCounts.size() )
		{
			m_staticOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
			continue;
		}

		if ( occlusionIndex >= m_staticOcclusionLastFrameIssuedFlags.size() )
		{
			m_staticOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
			continue;
		}

		if ( m_staticOcclusionLastFrameIssuedFlags[occlusionIndex] == 0 )
		{
			m_staticOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
			continue;
		}

		if ( m_staticOcclusionQuerySampleCounts[occlusionIndex] == 0ull )
		{
			uint8_t& zeroFrameCount = m_staticOcclusionZeroSampleFrameCounts[occlusionIndex];

			if ( zeroFrameCount < 255 )
				++zeroFrameCount;

			if ( zeroFrameCount >= m_staticOcclusionHideFrameThreshold )
				m_staticOcclusionCullFlags[entry.staticBatchObjectIndex] = 1;
		}
		else
		{
			m_staticOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
		}
	}
}

void CGameScene::ResetSkinnedOcclusionEntries()
{
	m_skinnedOcclusionEntries.clear();
	m_skinnedOcclusionCullFlags.clear();
	m_skinnedOcclusionQuerySampleCounts.clear();
	m_skinnedOcclusionLastFrameIssuedFlags.clear();
	m_skinnedOcclusionCurrentFrameIssuedFlags.clear();
	m_skinnedOcclusionZeroSampleFrameCounts.clear();
	m_skinnedOcclusionQueryCapacity = 0;
	m_bSkinnedOcclusionQueryResultsValid = false;
}

void CGameScene::BuildSkinnedOcclusionEntries()
{
	ResetSkinnedOcclusionEntries();

	m_skinnedOcclusionCullFlags.assign(m_skinnedBatch.objectRefs.size(), 0);

	for ( const SkinnedWorldLodEntry& lodEntry : m_skinnedWorldLodEntries )
	{
		if ( !lodEntry.object )
			continue;

		if ( lodEntry.skinnedBatchObjectIndex == UINT_MAX )
			continue;

		if ( lodEntry.skinnedBatchObjectIndex >= ( UINT ) m_skinnedBatch.objectRefs.size() )
			continue;

		const std::string& assetName = lodEntry.assetName;

		if ( !IsSkinnedOcclusionTargetAssetName(assetName) )
			continue;

		SkinnedOcclusionEntry entry{};
		entry.object = lodEntry.object;
		entry.skinnedBatchObjectIndex = lodEntry.skinnedBatchObjectIndex;
		entry.assetName = assetName;
		entry.enabled = true;
		entry.hasWorldBounds =
			TryBuildSkinnedOcclusionWorldBounds(entry.object, entry.assetName, entry.worldBounds);

		m_skinnedOcclusionEntries.push_back(std::move(entry));
	}
}

void CGameScene::BuildSkinnedOcclusionGpuResources(ID3D12Device* dev)
{
	ReleaseSkinnedOcclusionGpuResources();

	if ( !dev )
		return;

	const UINT queryCount = ( UINT ) m_skinnedOcclusionEntries.size();
	m_skinnedOcclusionQueryCapacity = queryCount;
	m_skinnedOcclusionQuerySampleCounts.assign(queryCount, 1ull);
	m_skinnedOcclusionLastFrameIssuedFlags.assign(queryCount, 0);
	m_skinnedOcclusionCurrentFrameIssuedFlags.assign(queryCount, 0);
	m_skinnedOcclusionZeroSampleFrameCounts.assign(queryCount, 0);
	m_bSkinnedOcclusionQueryResultsValid = false;

	if ( queryCount == 0 )
		return;

	D3D12_QUERY_HEAP_DESC queryHeapDesc{};
	queryHeapDesc.Count = queryCount;
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	queryHeapDesc.NodeMask = 0;

	HRESULT hr = dev->CreateQueryHeap(
		&queryHeapDesc,
		IID_PPV_ARGS(m_pd3dSkinnedOcclusionQueryHeap.ReleaseAndGetAddressOf())
	);

	if ( FAILED(hr) )
	{
		OutputDebugStringA("[SkinnedOcclusion] CreateQueryHeap failed.\n");
		return;
	}

	D3D12_HEAP_PROPERTIES readbackHeapProps{};
	readbackHeapProps.Type = D3D12_HEAP_TYPE_READBACK;
	readbackHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	readbackHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	readbackHeapProps.CreationNodeMask = 1;
	readbackHeapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC readbackDesc{};
	readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	readbackDesc.Alignment = 0;
	readbackDesc.Width = sizeof(UINT64) * queryCount;
	readbackDesc.Height = 1;
	readbackDesc.DepthOrArraySize = 1;
	readbackDesc.MipLevels = 1;
	readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
	readbackDesc.SampleDesc.Count = 1;
	readbackDesc.SampleDesc.Quality = 0;
	readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	readbackDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = dev->CreateCommittedResource(
		&readbackHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&readbackDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(m_pd3dSkinnedOcclusionReadbackBuffer.ReleaseAndGetAddressOf())
	);

	if ( FAILED(hr) )
	{
		OutputDebugStringA("[SkinnedOcclusion] Create readback buffer failed.\n");
		ReleaseSkinnedOcclusionGpuResources();
		return;
	}

	D3D12_RANGE readRange{};
	readRange.Begin = 0;
	readRange.End = 0;

	hr = m_pd3dSkinnedOcclusionReadbackBuffer->Map(
		0,
		&readRange,
		reinterpret_cast< void** >( &m_pMappedSkinnedOcclusionReadbackBuffer )
	);

	if ( FAILED(hr) || !m_pMappedSkinnedOcclusionReadbackBuffer )
	{
		OutputDebugStringA("[SkinnedOcclusion] Map readback buffer failed.\n");
		ReleaseSkinnedOcclusionGpuResources();
		return;
	}

	D3D12_HEAP_PROPERTIES uploadHeapProps{};
	uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHeapProps.CreationNodeMask = 1;
	uploadHeapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC instanceDesc{};
	instanceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	instanceDesc.Alignment = 0;
	instanceDesc.Width = sizeof(StaticInstanceVertex) * queryCount;
	instanceDesc.Height = 1;
	instanceDesc.DepthOrArraySize = 1;
	instanceDesc.MipLevels = 1;
	instanceDesc.Format = DXGI_FORMAT_UNKNOWN;
	instanceDesc.SampleDesc.Count = 1;
	instanceDesc.SampleDesc.Quality = 0;
	instanceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	instanceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = dev->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&instanceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_pd3dSkinnedOcclusionInstanceBuffer.ReleaseAndGetAddressOf())
	);

	if ( FAILED(hr) )
	{
		OutputDebugStringA("[SkinnedOcclusion] Create instance buffer failed.\n");
		ReleaseSkinnedOcclusionGpuResources();
		return;
	}

	hr = m_pd3dSkinnedOcclusionInstanceBuffer->Map(
		0,
		nullptr,
		reinterpret_cast< void** >( &m_pMappedSkinnedOcclusionInstanceBuffer )
	);

	if ( FAILED(hr) || !m_pMappedSkinnedOcclusionInstanceBuffer )
	{
		OutputDebugStringA("[SkinnedOcclusion] Map instance buffer failed.\n");
		ReleaseSkinnedOcclusionGpuResources();
		return;
	}

	m_bSkinnedOcclusionQueryResourcesReady = true;
}

void CGameScene::ReleaseSkinnedOcclusionGpuResources()
{
	if ( m_pd3dSkinnedOcclusionInstanceBuffer )
	{
		if ( m_pMappedSkinnedOcclusionInstanceBuffer )
		{
			m_pd3dSkinnedOcclusionInstanceBuffer->Unmap(0, nullptr);
			m_pMappedSkinnedOcclusionInstanceBuffer = nullptr;
		}

		m_pd3dSkinnedOcclusionInstanceBuffer.Reset();
	}

	if ( m_pd3dSkinnedOcclusionReadbackBuffer )
	{
		if ( m_pMappedSkinnedOcclusionReadbackBuffer )
		{
			m_pd3dSkinnedOcclusionReadbackBuffer->Unmap(0, nullptr);
			m_pMappedSkinnedOcclusionReadbackBuffer = nullptr;
		}

		m_pd3dSkinnedOcclusionReadbackBuffer.Reset();
	}

	if ( m_pd3dSkinnedOcclusionQueryHeap )
		m_pd3dSkinnedOcclusionQueryHeap.Reset();

	m_skinnedOcclusionQueryCapacity = 0;
	m_bSkinnedOcclusionQueryResourcesReady = false;
	m_bSkinnedOcclusionQueryResultsValid = false;
	m_skinnedOcclusionQuerySampleCounts.clear();
	m_skinnedOcclusionLastFrameIssuedFlags.clear();
	m_skinnedOcclusionCurrentFrameIssuedFlags.clear();
	m_skinnedOcclusionZeroSampleFrameCounts.clear();
}

void CGameScene::BeginSkinnedOcclusionReadback()
{
	if ( !m_bSkinnedOcclusionQueryResourcesReady )
		return;

	if ( !m_bSkinnedOcclusionQueryResultsValid )
		return;

	if ( !m_pMappedSkinnedOcclusionReadbackBuffer )
		return;

	const UINT queryCount = ( UINT ) m_skinnedOcclusionEntries.size();
	if ( queryCount == 0 )
		return;

	if ( m_skinnedOcclusionQuerySampleCounts.size() != queryCount )
		m_skinnedOcclusionQuerySampleCounts.assign(queryCount, 1ull);

	for ( UINT i = 0; i < queryCount; ++i )
	{
		m_skinnedOcclusionQuerySampleCounts[i] =
			m_pMappedSkinnedOcclusionReadbackBuffer[i];
	}
}

void CGameScene::ResolveSkinnedOcclusionQueries(ID3D12GraphicsCommandList* cmd)
{
	if ( !cmd )
		return;

	if ( !m_bSkinnedOcclusionQueryResourcesReady )
		return;

	const UINT queryCount = ( UINT ) m_skinnedOcclusionEntries.size();
	if ( queryCount == 0 )
		return;

	cmd->ResolveQueryData(
		m_pd3dSkinnedOcclusionQueryHeap.Get(),
		D3D12_QUERY_TYPE_OCCLUSION,
		0,
		queryCount,
		m_pd3dSkinnedOcclusionReadbackBuffer.Get(),
		0
	);

	m_skinnedOcclusionLastFrameIssuedFlags = m_skinnedOcclusionCurrentFrameIssuedFlags;
	m_bSkinnedOcclusionQueryResultsValid = true;
}

void CGameScene::UpdateSkinnedOcclusionCullSelection(CCamera* camera)
{
	m_skinnedOcclusionCullFlags.assign(m_skinnedBatch.objectRefs.size(), 0);

	if ( !m_bSkinnedOcclusionCullingEnabled )
		return;

	if ( !camera )
		return;

	const XMFLOAT3 cameraPosition = camera->GetPosition();
	const float minTestDistanceSq =
		m_skinnedOcclusionMinTestDistance * m_skinnedOcclusionMinTestDistance;

	for ( size_t occlusionIndex = 0; occlusionIndex < m_skinnedOcclusionEntries.size(); ++occlusionIndex )
	{
		SkinnedOcclusionEntry& entry = m_skinnedOcclusionEntries[occlusionIndex];

		if ( !entry.enabled )
			continue;

		if ( !entry.object )
			continue;

		if ( entry.skinnedBatchObjectIndex == UINT_MAX )
			continue;

		if ( entry.skinnedBatchObjectIndex >= ( UINT ) m_skinnedOcclusionCullFlags.size() )
			continue;

		if ( occlusionIndex >= m_skinnedOcclusionZeroSampleFrameCounts.size() )
			continue;

		entry.hasWorldBounds =
			TryBuildSkinnedOcclusionWorldBounds(entry.object, entry.assetName, entry.worldBounds);

		if ( !entry.hasWorldBounds )
		{
			m_skinnedOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
			continue;
		}

		if ( entry.skinnedBatchObjectIndex < ( UINT ) m_skinnedDistanceCullFlags.size() )
		{
			if ( m_skinnedDistanceCullFlags[entry.skinnedBatchObjectIndex] != 0 )
			{
				m_skinnedOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
				continue;
			}
		}

		if ( !entry.object->IsVisible(camera) )
		{
			m_skinnedOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
			continue;
		}

		const float dx = cameraPosition.x - entry.worldBounds.Center.x;
		const float dy = cameraPosition.y - entry.worldBounds.Center.y;
		const float dz = cameraPosition.z - entry.worldBounds.Center.z;
		const float distSq = dx * dx + dy * dy + dz * dz;

		if ( distSq < minTestDistanceSq )
		{
			m_skinnedOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
			continue;
		}

		const float dist = std::sqrt(distSq);
		float maxExtent = entry.worldBounds.Extents.x;
		if (entry.worldBounds.Extents.y > maxExtent) maxExtent = entry.worldBounds.Extents.y;
		if (entry.worldBounds.Extents.z > maxExtent) maxExtent = entry.worldBounds.Extents.z;

		if ( dist > 0.0001f )
		{
			const float extentDistanceRatio = maxExtent / dist;

			if ( extentDistanceRatio > m_skinnedOcclusionMaxCullExtentDistanceRatio )
			{
				m_skinnedOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
				continue;
			}
		}

		if ( !m_bSkinnedOcclusionQueryResultsValid )
		{
			m_skinnedOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
			continue;
		}

		if ( occlusionIndex >= m_skinnedOcclusionQuerySampleCounts.size() )
		{
			m_skinnedOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
			continue;
		}

		if ( occlusionIndex >= m_skinnedOcclusionLastFrameIssuedFlags.size() )
		{
			m_skinnedOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
			continue;
		}

		if ( m_skinnedOcclusionLastFrameIssuedFlags[occlusionIndex] == 0 )
		{
			m_skinnedOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
			continue;
		}

		if ( m_skinnedOcclusionQuerySampleCounts[occlusionIndex] == 0ull )
		{
			uint8_t& zeroFrameCount = m_skinnedOcclusionZeroSampleFrameCounts[occlusionIndex];

			if ( zeroFrameCount < 255 )
				++zeroFrameCount;

			if ( zeroFrameCount >= m_skinnedOcclusionHideFrameThreshold )
				m_skinnedOcclusionCullFlags[entry.skinnedBatchObjectIndex] = 1;
		}
		else
		{
			m_skinnedOcclusionZeroSampleFrameCounts[occlusionIndex] = 0;
		}
	}

	std::unordered_map<const CGameObject*, UINT> staticIndexByObject;
	staticIndexByObject.reserve(m_staticBatch.objectRefs.size());

	for ( UINT i = 0; i < ( UINT ) m_staticBatch.objectRefs.size(); ++i )
	{
		if ( m_staticBatch.objectRefs[i] )
			staticIndexByObject[m_staticBatch.objectRefs[i]] = i;
	}

	std::unordered_map<const CGameObject*, UINT> skinnedIndexByObject;
	skinnedIndexByObject.reserve(m_skinnedBatch.objectRefs.size());

	for ( UINT i = 0; i < ( UINT ) m_skinnedBatch.objectRefs.size(); ++i )
	{
		if ( m_skinnedBatch.objectRefs[i] )
			skinnedIndexByObject[m_skinnedBatch.objectRefs[i]] = i;
	}

	for ( const AttachmentBindSpec& spec : m_attachmentBinds )
	{
		if ( !spec.follower || !spec.target )
			continue;

		auto targetIt = skinnedIndexByObject.find(spec.target);
		if ( targetIt == skinnedIndexByObject.end() )
			continue;

		const UINT targetIndex = targetIt->second;
		if ( targetIndex >= ( UINT ) m_skinnedOcclusionCullFlags.size() )
			continue;

		if ( m_skinnedOcclusionCullFlags[targetIndex] == 0 )
			continue;

		auto followerStaticIt = staticIndexByObject.find(spec.follower);
		if ( followerStaticIt != staticIndexByObject.end() )
		{
			const UINT followerIndex = followerStaticIt->second;
			if ( followerIndex < ( UINT ) m_staticDistanceCullFlags.size() )
				m_staticDistanceCullFlags[followerIndex] = 1;
		}

		auto followerSkinnedIt = skinnedIndexByObject.find(spec.follower);
		if ( followerSkinnedIt != skinnedIndexByObject.end() )
		{
			const UINT followerIndex = followerSkinnedIt->second;
			if ( followerIndex < ( UINT ) m_skinnedDistanceCullFlags.size() )
				m_skinnedDistanceCullFlags[followerIndex] = 1;
		}
	}
}
