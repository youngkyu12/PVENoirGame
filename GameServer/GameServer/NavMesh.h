#pragma once

#include "pch.h"
#include "GameMath.h"

#include <cfloat>

struct NAVMESH_FILE_HEADER_V1
{
	uint32 magic = 0;
	uint32 version = 0;
	uint32 flags = 0;
	uint32 vertexCount = 0;
	uint32 triangleCount = 0;
	uint32 reserved0 = 0;
	float boundsMinX = 0.f;
	float boundsMinY = 0.f;
	float boundsMinZ = 0.f;
	float boundsMaxX = 0.f;
	float boundsMaxY = 0.f;
	float boundsMaxZ = 0.f;
};

struct NAVMESH_VERTEX
{
	float x = 0.f;
	float y = 0.f;
	float z = 0.f;
};

struct NAVMESH_TRIANGLE
{
	uint32 i0 = 0;
	uint32 i1 = 0;
	uint32 i2 = 0;
	int32 n0 = -1;
	int32 n1 = -1;
	int32 n2 = -1;
	uint32 area = 0;
	uint32 reserved0 = 0;
};

class CNavMesh
{
public:
	bool LoadFromFile(const std::string& filePath);
	bool IsLoaded() const { return m_loaded; }

	bool SamplePosition(const GameMath::Vec3& worldPos, GameMath::Vec3& outProjectedPos, int* outTriangleIndex = nullptr, float maxSearchDistanceXZ = FLT_MAX) const;
	bool FindPath(const GameMath::Vec3& startPos, const GameMath::Vec3& goalPos, std::vector<int>& outTrianglePath, std::vector<GameMath::Vec3>& outStraightPath) const;

private:
	bool FindContainingTriangle(const GameMath::Vec3& worldPos, int& outTriangleIndex) const;
	int FindNearestTriangle(const GameMath::Vec3& worldPos, float maxSearchDistanceXZ) const;
	bool BuildTrianglePath(int startTri, int goalTri, std::vector<int>& outTrianglePath) const;
	bool FindStraightPath(const GameMath::Vec3& startProj, const GameMath::Vec3& goalProj, const std::vector<int>& trianglePath, std::vector<GameMath::Vec3>& outStraightPath) const;
	bool GetTrianglePortal(int fromTriangle, int toTriangle, GameMath::Vec3& outA, GameMath::Vec3& outB) const;
	GameMath::Vec3 TriangleCenter(int triIndex) const;
	bool ProjectPointToTriangleXZ(int triIndex, const GameMath::Vec3& input, GameMath::Vec3& outProjectedPos, float* outDistSq = nullptr) const;
	void BuildNeighborsFromGeometry();

private:
	bool m_loaded = false;
	std::vector<GameMath::Vec3> m_vertices;
	std::vector<NAVMESH_TRIANGLE> m_triangles;
};
