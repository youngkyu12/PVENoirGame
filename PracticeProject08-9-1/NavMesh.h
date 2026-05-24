//-----------------------------------------------------------------------------
// File: NavMesh.h
//-----------------------------------------------------------------------------

#pragma once

#include "stdafx.h"

#include <cstdint>
#include <string>
#include <vector>
#include <cfloat>

struct NAVMESH_FILE_HEADER_V1
{
	uint32_t magic = 0;        // 'NVMH' = 0x484D564E
	uint32_t version = 0;      // 1
	uint32_t flags = 0;        // bit0 = neighbor 정보 포함
	uint32_t vertexCount = 0;
	uint32_t triangleCount = 0;
	uint32_t reserved0 = 0;

	float boundsMinX = 0.0f;
	float boundsMinY = 0.0f;
	float boundsMinZ = 0.0f;

	float boundsMaxX = 0.0f;
	float boundsMaxY = 0.0f;
	float boundsMaxZ = 0.0f;
};

struct NAVMESH_VERTEX
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct NAVMESH_TRIANGLE
{
	uint32_t i0 = 0;
	uint32_t i1 = 0;
	uint32_t i2 = 0;

	int32_t n0 = -1;   // edge (i0, i1) shared neighbor, none = -1
	int32_t n1 = -1;   // edge (i1, i2)
	int32_t n2 = -1;   // edge (i2, i0)

	uint32_t area = 0;
	uint32_t reserved0 = 0;
};

class CNavMesh final
{
public:
	CNavMesh();
	~CNavMesh();

public:
	void Clear();
	bool LoadFromFile(const std::string& filePath);
	bool IsLoaded() const { return m_bLoaded; }

public:
	uint32_t GetVertexCount() const { return static_cast< uint32_t >( m_vertices.size() ); }
	uint32_t GetTriangleCount() const { return static_cast< uint32_t >( m_triangles.size() ); }

	const XMFLOAT3& GetBoundsMin() const { return m_boundsMin; }
	const XMFLOAT3& GetBoundsMax() const { return m_boundsMax; }

	const XMFLOAT3& GetVertex(uint32_t index) const;
	const NAVMESH_TRIANGLE& GetTriangle(uint32_t index) const;

public:
	void SetAreaCost(uint32_t areaId, float cost);
	float GetAreaCost(uint32_t areaId) const;

public:
	bool IsPointInsideBounds(const XMFLOAT3& worldPos) const;

	bool FindContainingTriangle(const XMFLOAT3& worldPos, int& outTriangleIndex) const;
	int FindNearestTriangle(const XMFLOAT3& worldPos, float maxSearchDistanceXZ = FLT_MAX) const;
	bool SamplePosition(
		const XMFLOAT3& worldPos,
		XMFLOAT3& outProjectedPos,
		int* outTriangleIndex = nullptr,
		float maxSearchDistanceXZ = FLT_MAX) const;

	bool HasLineOfSight(
		const XMFLOAT3& startPos,
		const XMFLOAT3& goalPos,
		float maxEndpointSearchDistanceXZ = 1.0f) const;

public:
	bool GetTriangleCentroid(int triangleIndex, XMFLOAT3& outCentroid) const;
	bool GetTrianglePortal(int fromTriangle, int toTriangle, XMFLOAT3& outA, XMFLOAT3& outB) const;

public:
	bool FindTrianglePath(
		const XMFLOAT3& startPos,
		const XMFLOAT3& goalPos,
		std::vector<int>& outTrianglePath) const;

	bool FindStraightPath(
		const XMFLOAT3& startPos,
		const XMFLOAT3& goalPos,
		std::vector<XMFLOAT3>& outStraightPath) const;

	bool FindPath(
		const XMFLOAT3& startPos,
		const XMFLOAT3& goalPos,
		std::vector<int>& outTrianglePath,
		std::vector<XMFLOAT3>& outStraightPath) const;

private:
	struct TriangleRuntimeData
	{
		float minX = 0.0f;
		float maxX = 0.0f;
		float minZ = 0.0f;
		float maxZ = 0.0f;
		XMFLOAT3 centroid = XMFLOAT3(0.0f, 0.0f, 0.0f);
	};

private:
	bool ValidateLoadedData() const;
	void BuildRuntimeData();
	void BuildNeighborsFromGeometry();

	bool ProjectPointToTriangleXZ(
		int triangleIndex,
		const XMFLOAT3& inputPos,
		XMFLOAT3& outProjectedPos,
		float* outDistSqXZ = nullptr,
		float* outVerticalAbs = nullptr) const;

private:
	bool m_bLoaded = false;

	NAVMESH_FILE_HEADER_V1 m_fileHeader = {};

	std::vector<XMFLOAT3> m_vertices;
	std::vector<NAVMESH_TRIANGLE> m_triangles;
	std::vector<TriangleRuntimeData> m_triangleRuntime;
	std::vector<float> m_areaCosts;

	XMFLOAT3 m_boundsMin = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 m_boundsMax = XMFLOAT3(0.0f, 0.0f, 0.0f);
};