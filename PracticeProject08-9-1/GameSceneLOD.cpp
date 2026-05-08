//-----------------------------------------------------------------------------
// File: GameSceneLOD.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScene.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "Camera.h"
#include "Mesh.h"
#include "Object.h"

namespace
{
	static int ClampStaticWorldLodLevel(int lodLevel)
	{
		if ( lodLevel < 0 ) return 0;
		if ( lodLevel > 2 ) return 2;
		return lodLevel;
	}

	static bool BuildStaticLodMeshBinPath(
		const std::string& baseMeshBinPath,
		int lodLevel,
		std::string& outMeshBinPath)
	{
		const size_t dotPos = baseMeshBinPath.find_last_of('.');
		if ( dotPos == std::string::npos )
			return false;

		const int clampedLodLevel = ClampStaticWorldLodLevel(lodLevel);

		outMeshBinPath = baseMeshBinPath.substr(0, dotPos);
		outMeshBinPath += "_LOD";
		outMeshBinPath += std::to_string(clampedLodLevel);
		outMeshBinPath += baseMeshBinPath.substr(dotPos);

		return true;
	}

	static int ClampSkinnedWorldLodLevel(int lodLevel)
	{
		if ( lodLevel < 0 ) return 0;
		if ( lodLevel > 2 ) return 2;
		return lodLevel;
	}

	static bool BuildSkinnedLodMeshBinPath(
		const std::string& baseMeshBinPath,
		int lodLevel,
		std::string& outMeshBinPath)
	{
		const size_t dotPos = baseMeshBinPath.find_last_of('.');
		if ( dotPos == std::string::npos )
			return false;

		const int clampedLodLevel = ClampSkinnedWorldLodLevel(lodLevel);

		outMeshBinPath = baseMeshBinPath.substr(0, dotPos);
		outMeshBinPath += "_LOD";
		outMeshBinPath += std::to_string(clampedLodLevel);
		outMeshBinPath += baseMeshBinPath.substr(dotPos);
		return true;
	}
}

void CGameScene::ResetStaticWorldLodEntries()
{
	m_staticWorldLodEntries.clear();
	m_staticDistanceCullFlags.clear();
	m_staticTreeGridCullFlags.clear();
	m_staticWorldLodDirty = false;
}

int CGameScene::ComputeStaticWorldLodLevel(
	const XMFLOAT3& cameraPosition,
	const StaticWorldLodEntry& entry) const
{
	if ( !entry.lodEnabled )
		return 0;

	const float dx = cameraPosition.x - entry.lodReferencePosition.x;
	const float dy = cameraPosition.y - entry.lodReferencePosition.y;
	const float dz = cameraPosition.z - entry.lodReferencePosition.z;

	const float distSq = dx * dx + dy * dy + dz * dz;
	const float dist = std::sqrt(distSq);

	float lodDistance01 = entry.lodDistance01;
	if ( lodDistance01 < 0.0f ) lodDistance01 = 0.0f;

	float lodDistance12 = entry.lodDistance12;
	if ( lodDistance12 < lodDistance01 ) lodDistance12 = lodDistance01;

	const float lod01Enter = lodDistance01 + m_staticLodHysteresis;
	const float lod01Exit = lodDistance01 - m_staticLodHysteresis;
	const float lod12Enter = lodDistance12 + m_staticLodHysteresis;
	const float lod12Exit = lodDistance12 - m_staticLodHysteresis;

	switch ( entry.currentLod )
	{
	case 0:
		if ( dist >= lod01Enter ) return 1;
		return 0;

	case 1:
		if ( dist < lod01Exit ) return 0;
		if ( dist >= lod12Enter ) return 2;
		return 1;

	case 2:
		if ( dist < lod12Exit ) return 1;
		return 2;

	default:
		break;
	}

	if ( dist < lodDistance01 ) return 0;
	if ( dist < lodDistance12 ) return 1;
	return 2;
}

bool CGameScene::ComputeStaticWorldDistanceCulled(
	const XMFLOAT3& cameraPosition,
	const StaticWorldLodEntry& entry) const
{
	if ( !entry.distanceCullEnabled )
		return false;

	const float dx = cameraPosition.x - entry.lodReferencePosition.x;
	const float dy = cameraPosition.y - entry.lodReferencePosition.y;
	const float dz = cameraPosition.z - entry.lodReferencePosition.z;

	const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

	float cullDistance = entry.cullDistance;
	if ( cullDistance < 0.0f ) cullDistance = 0.0f;

	const float cullEnter = cullDistance + m_staticCullHysteresis;
	float cullExit = cullDistance - m_staticCullHysteresis;
	if ( cullExit < 0.0f ) cullExit = 0.0f;

	if ( !entry.distanceCulled )
	{
		if ( dist >= cullEnter ) return true;
		return false;
	}

	if ( dist < cullExit ) return false;
	return true;
}

void CGameScene::UpdateStaticWorldLodSelection(CCamera* camera)
{
	if ( !camera )
	{
		m_staticDistanceCullFlags.clear();
		return;
	}

	m_staticDistanceCullFlags.assign(m_staticBatch.objectRefs.size(), 0);

	if ( m_staticWorldLodEntries.empty() )
	{
		m_staticWorldLodDirty = false;
		return;
	}

	const XMFLOAT3 cameraPosition = camera->GetPosition();

	for ( StaticWorldLodEntry& entry : m_staticWorldLodEntries )
	{
		if ( !entry.object ) continue;
		if ( entry.staticBatchObjectIndex == UINT_MAX ) continue;
		if ( entry.staticBatchObjectIndex >= ( UINT ) m_staticDistanceCullFlags.size() ) continue;

		const bool distanceCulled =
			ComputeStaticWorldDistanceCulled(cameraPosition, entry);

		entry.distanceCulled = distanceCulled;

		if ( distanceCulled )
			m_staticDistanceCullFlags[entry.staticBatchObjectIndex] = 1;
	}

	m_staticWorldLodDirty = false;
}

void CGameScene::ResetSkinnedWorldLodEntries()
{
	m_skinnedWorldLodEntries.clear();
	m_skinnedDistanceCullFlags.clear();
	m_skinnedWorldLodDirty = false;
}

int CGameScene::ComputeSkinnedWorldLodLevel(
	const XMFLOAT3& cameraPosition,
	const SkinnedWorldLodEntry& entry) const
{
	if ( !entry.lodEnabled )
		return 0;

	const float dx = cameraPosition.x - entry.lodReferencePosition.x;
	const float dy = cameraPosition.y - entry.lodReferencePosition.y;
	const float dz = cameraPosition.z - entry.lodReferencePosition.z;

	const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

	float lodDistance01 = entry.lodDistance01;
	if ( lodDistance01 < 0.0f ) lodDistance01 = 0.0f;

	float lodDistance12 = entry.lodDistance12;
	if ( lodDistance12 < lodDistance01 ) lodDistance12 = lodDistance01;

	const float lod01Enter = lodDistance01 + m_skinnedLodHysteresis;
	const float lod01Exit = lodDistance01 - m_skinnedLodHysteresis;
	const float lod12Enter = lodDistance12 + m_skinnedLodHysteresis;
	const float lod12Exit = lodDistance12 - m_skinnedLodHysteresis;

	switch ( entry.currentLod )
	{
	case 0:
		if ( dist >= lod01Enter ) return 1;
		return 0;

	case 1:
		if ( dist < lod01Exit ) return 0;
		if ( dist >= lod12Enter ) return 2;
		return 1;

	case 2:
		if ( dist < lod12Exit ) return 1;
		return 2;
	}

	if ( dist < lodDistance01 ) return 0;
	if ( dist < lodDistance12 ) return 1;
	return 2;
}

bool CGameScene::ComputeSkinnedWorldDistanceCulled(
	const XMFLOAT3& cameraPosition,
	const SkinnedWorldLodEntry& entry) const
{
	if ( !entry.distanceCullEnabled )
		return false;

	const float dx = cameraPosition.x - entry.lodReferencePosition.x;
	const float dy = cameraPosition.y - entry.lodReferencePosition.y;
	const float dz = cameraPosition.z - entry.lodReferencePosition.z;

	const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

	// replace std::max usag
	float cullDistance = entry.cullDistance;
	if ( cullDistance < 0.0f ) cullDistance = 0.0f;

	const float cullEnter = cullDistance + m_skinnedCullHysteresis;
	float cullExit = cullDistance - m_skinnedCullHysteresis;
	if ( cullExit < 0.0f ) cullExit = 0.0f;

	if ( !entry.distanceCulled )
	{
		if ( dist >= cullEnter ) return true;
		return false;
	}

	if ( dist < cullExit ) return false;
	return true;
}

void CGameScene::UpdateSkinnedWorldLodSelection(CCamera* camera)
{
	if ( !camera )
	{
		m_skinnedDistanceCullFlags.clear();
		return;
	}

	m_skinnedDistanceCullFlags.assign(m_skinnedBatch.objectRefs.size(), 0);

	if ( m_skinnedWorldLodEntries.empty() )
	{
		m_skinnedWorldLodDirty = false;
		return;
	}

	const XMFLOAT3 cameraPosition = camera->GetPosition();
	bool anyLodChanged = false;

	for ( SkinnedWorldLodEntry& entry : m_skinnedWorldLodEntries )
	{
		if ( !entry.object ) continue;
		if ( entry.skinnedBatchObjectIndex == UINT_MAX ) continue;
		if ( entry.skinnedBatchObjectIndex >= ( UINT ) m_skinnedDistanceCullFlags.size() ) continue;

		const bool distanceCulled =
			ComputeSkinnedWorldDistanceCulled(cameraPosition, entry);

		entry.distanceCulled = distanceCulled;

		if ( distanceCulled )
		{
			m_skinnedDistanceCullFlags[entry.skinnedBatchObjectIndex] = 1;
			continue;
		}

		int desiredLod = ComputeSkinnedWorldLodLevel(cameraPosition, entry);
		desiredLod = ClampSkinnedWorldLodLevel(desiredLod);

		int resolvedLod = desiredLod;
		while ( resolvedLod > 0 && !entry.lodMeshes[( size_t ) resolvedLod] )
			--resolvedLod;

		std::shared_ptr<CMesh> targetMesh = entry.lodMeshes[( size_t ) resolvedLod];
		if ( !targetMesh ) continue;

		std::shared_ptr<CMesh> currentMesh = entry.object->GetMeshShared(0);
		if ( entry.currentLod == resolvedLod && currentMesh.get() == targetMesh.get() )
			continue;
	}

	// --------------------------------------------------------------------
	// body가 culled 되면 attachment follower도 같이 culled 처리
	// - static follower : sword, helmet 등
	// - skinned follower: bow 등
	// --------------------------------------------------------------------
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
		if ( targetIndex >= ( UINT ) m_skinnedDistanceCullFlags.size() )
			continue;

		if ( m_skinnedDistanceCullFlags[targetIndex] == 0 )
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

	if ( anyLodChanged )
	{
		BuildSkinnedInstanceGroups();
		m_skinnedWorldLodDirty = true;
	}
	else
	{
		m_skinnedWorldLodDirty = false;
	}
}

// spawn

void CGameScene::ResetSpawnWorldLodEntries()
{
	m_spawnWorldLodEntries.clear();
	m_spawnDistanceCullFlags.clear();
	m_spawnWorldLodDirty = false;
}