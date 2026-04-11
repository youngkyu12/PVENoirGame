#include "pch.h"
#include "NavMesh.h"

#include <fstream>
#include <queue>

namespace
{
	constexpr uint32 kNavMeshMagic = 0x484D564E;
	constexpr uint32 kNavMeshVersion = 1;
	constexpr float kInsideEps = 1e-4f;
	constexpr float kDegenerateEps = 1e-8f;

	float DistSqXZ(const GameMath::Vec3& a, const GameMath::Vec3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;
		return dx * dx + dz * dz;
	}

	bool ComputeBarycentricXZ(const GameMath::Vec3& p, const GameMath::Vec3& a, const GameMath::Vec3& b, const GameMath::Vec3& c, float& u, float& v, float& w)
	{
		const float v0x = b.x - a.x;
		const float v0z = b.z - a.z;
		const float v1x = c.x - a.x;
		const float v1z = c.z - a.z;
		const float v2x = p.x - a.x;
		const float v2z = p.z - a.z;

		const float d00 = v0x * v0x + v0z * v0z;
		const float d01 = v0x * v1x + v0z * v1z;
		const float d11 = v1x * v1x + v1z * v1z;
		const float d20 = v2x * v0x + v2z * v0z;
		const float d21 = v2x * v1x + v2z * v1z;

		const float denom = d00 * d11 - d01 * d01;
		if (fabsf(denom) <= kDegenerateEps)
			return false;

		v = (d11 * d20 - d01 * d21) / denom;
		w = (d00 * d21 - d01 * d20) / denom;
		u = 1.0f - v - w;
		return true;
	}
}

bool CNavMesh::LoadFromFile(const std::string& filePath)
{
	m_loaded = false;
	m_vertices.clear();
	m_triangles.clear();

	std::ifstream fin(filePath, std::ios::binary);
	if (!fin.is_open())
		return false;

	NAVMESH_FILE_HEADER_V1 header{};
	fin.read(reinterpret_cast<char*>(&header), sizeof(header));
	if (!fin.good())
		return false;

	if (header.magic != kNavMeshMagic || header.version != kNavMeshVersion)
		return false;

	std::vector<NAVMESH_VERTEX> fileVertices(header.vertexCount);
	std::vector<NAVMESH_TRIANGLE> fileTriangles(header.triangleCount);

	fin.read(reinterpret_cast<char*>(fileVertices.data()), sizeof(NAVMESH_VERTEX) * fileVertices.size());
	fin.read(reinterpret_cast<char*>(fileTriangles.data()), sizeof(NAVMESH_TRIANGLE) * fileTriangles.size());
	if (!fin.good())
		return false;

	m_vertices.reserve(fileVertices.size());
	for (const auto& v : fileVertices)
		m_vertices.push_back(GameMath::Vec3(v.x, v.y, v.z));

	m_triangles = std::move(fileTriangles);
	m_loaded = !m_vertices.empty() && !m_triangles.empty();
	return m_loaded;
}

bool CNavMesh::ProjectPointToTriangleXZ(int triIndex, const GameMath::Vec3& input, GameMath::Vec3& outProjectedPos, float* outDistSq) const
{
	if (triIndex < 0 || triIndex >= static_cast<int>(m_triangles.size()))
		return false;

	const auto& tri = m_triangles[triIndex];
	const GameMath::Vec3& a = m_vertices[tri.i0];
	const GameMath::Vec3& b = m_vertices[tri.i1];
	const GameMath::Vec3& c = m_vertices[tri.i2];

	float u = 0.f, v = 0.f, w = 0.f;
	if (!ComputeBarycentricXZ(input, a, b, c, u, v, w))
		return false;

	u = GameMath::Clamp(u, 0.f, 1.f);
	v = GameMath::Clamp(v, 0.f, 1.f);
	w = GameMath::Clamp(w, 0.f, 1.f);
	const float sum = u + v + w;
	if (sum <= kDegenerateEps)
		return false;

	u /= sum;
	v /= sum;
	w /= sum;

	outProjectedPos = GameMath::Vec3(
		a.x * u + b.x * v + c.x * w,
		a.y * u + b.y * v + c.y * w,
		a.z * u + b.z * v + c.z * w);

	if (outDistSq)
		*outDistSq = DistSqXZ(input, outProjectedPos);

	return true;
}

bool CNavMesh::FindContainingTriangle(const GameMath::Vec3& worldPos, int& outTriangleIndex) const
{
	outTriangleIndex = -1;
	for (int i = 0; i < static_cast<int>(m_triangles.size()); ++i)
	{
		const auto& tri = m_triangles[i];
		const GameMath::Vec3& a = m_vertices[tri.i0];
		const GameMath::Vec3& b = m_vertices[tri.i1];
		const GameMath::Vec3& c = m_vertices[tri.i2];

		float u = 0.f, v = 0.f, w = 0.f;
		if (!ComputeBarycentricXZ(worldPos, a, b, c, u, v, w))
			continue;

		if (u >= -kInsideEps && v >= -kInsideEps && w >= -kInsideEps)
		{
			outTriangleIndex = i;
			return true;
		}
	}
	return false;
}

int CNavMesh::FindNearestTriangle(const GameMath::Vec3& worldPos, float maxSearchDistanceXZ) const
{
	int best = -1;
	float bestDist = FLT_MAX;
	const float maxDistSq = maxSearchDistanceXZ >= FLT_MAX * 0.5f ? FLT_MAX : maxSearchDistanceXZ * maxSearchDistanceXZ;

	for (int i = 0; i < static_cast<int>(m_triangles.size()); ++i)
	{
		GameMath::Vec3 projected{};
		float distSq = FLT_MAX;
		if (!ProjectPointToTriangleXZ(i, worldPos, projected, &distSq))
			continue;
		if (distSq > maxDistSq)
			continue;
		if (distSq < bestDist)
		{
			bestDist = distSq;
			best = i;
		}
	}

	return best;
}

bool CNavMesh::SamplePosition(const GameMath::Vec3& worldPos, GameMath::Vec3& outProjectedPos, int* outTriangleIndex, float maxSearchDistanceXZ) const
{
	if (outTriangleIndex)
		*outTriangleIndex = -1;

	if (!m_loaded)
		return false;

	int tri = -1;
	if (!FindContainingTriangle(worldPos, tri))
		tri = FindNearestTriangle(worldPos, maxSearchDistanceXZ);

	if (tri < 0)
		return false;

	if (!ProjectPointToTriangleXZ(tri, worldPos, outProjectedPos))
		return false;

	if (outTriangleIndex)
		*outTriangleIndex = tri;

	return true;
}

GameMath::Vec3 CNavMesh::TriangleCenter(int triIndex) const
{
	const auto& tri = m_triangles[triIndex];
	const auto& a = m_vertices[tri.i0];
	const auto& b = m_vertices[tri.i1];
	const auto& c = m_vertices[tri.i2];
	return GameMath::Vec3((a.x + b.x + c.x) / 3.f, (a.y + b.y + c.y) / 3.f, (a.z + b.z + c.z) / 3.f);
}

bool CNavMesh::BuildTrianglePath(int startTri, int goalTri, std::vector<int>& outTrianglePath) const
{
	outTrianglePath.clear();
	if (startTri == goalTri)
	{
		outTrianglePath.push_back(startTri);
		return true;
	}

	struct Node { int tri = -1; float f = FLT_MAX; };
	struct Greater { bool operator()(const Node& a, const Node& b) const { return a.f > b.f; } };

	const int triCount = static_cast<int>(m_triangles.size());
	std::vector<float> g(triCount, FLT_MAX);
	std::vector<float> f(triCount, FLT_MAX);
	std::vector<int> prev(triCount, -1);
	std::vector<uint8> closed(triCount, 0);
	std::priority_queue<Node, std::vector<Node>, Greater> open;

	auto heuristic = [&](int idx)
		{
			return sqrtf(DistSqXZ(TriangleCenter(idx), TriangleCenter(goalTri)));
		};

	g[startTri] = 0.f;
	f[startTri] = heuristic(startTri);
	open.push({ startTri, f[startTri] });

	while (!open.empty())
	{
		const int cur = open.top().tri;
		open.pop();

		if (closed[cur])
			continue;
		closed[cur] = 1;

		if (cur == goalTri)
			break;

		const auto& tri = m_triangles[cur];
		const int neighbors[3] = { tri.n0, tri.n1, tri.n2 };
		for (int n : neighbors)
		{
			if (n < 0 || n >= triCount)
				continue;
			if (closed[n])
				continue;

			const float stepCost = sqrtf(DistSqXZ(TriangleCenter(cur), TriangleCenter(n)));
			const float candidate = g[cur] + stepCost;
			if (candidate >= g[n])
				continue;

			prev[n] = cur;
			g[n] = candidate;
			f[n] = candidate + heuristic(n);
			open.push({ n, f[n] });
		}
	}

	if (prev[goalTri] == -1)
		return false;

	for (int t = goalTri; t != -1; t = prev[t])
		outTrianglePath.push_back(t);
	std::reverse(outTrianglePath.begin(), outTrianglePath.end());
	return !outTrianglePath.empty();
}

bool CNavMesh::FindPath(const GameMath::Vec3& startPos, const GameMath::Vec3& goalPos, std::vector<int>& outTrianglePath, std::vector<GameMath::Vec3>& outStraightPath) const
{
	outTrianglePath.clear();
	outStraightPath.clear();

	if (!m_loaded)
		return false;

	GameMath::Vec3 startProj{};
	GameMath::Vec3 goalProj{};
	int startTri = -1;
	int goalTri = -1;
	if (!SamplePosition(startPos, startProj, &startTri))
		return false;
	if (!SamplePosition(goalPos, goalProj, &goalTri))
		return false;

	if (!BuildTrianglePath(startTri, goalTri, outTrianglePath))
		return false;

	outStraightPath.push_back(startProj);
	for (size_t i = 1; i + 1 < outTrianglePath.size(); ++i)
		outStraightPath.push_back(TriangleCenter(outTrianglePath[i]));
	outStraightPath.push_back(goalProj);

	return true;
}
