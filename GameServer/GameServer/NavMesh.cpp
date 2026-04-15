#include "pch.h"
#include "NavMesh.h"

#include <fstream>
#include <queue>
#include <unordered_map>

namespace
{
	constexpr uint32 kNavMeshMagic = 0x484D564E;
	constexpr uint32 kNavMeshVersion = 1;
	constexpr float kInsideEps = 1e-4f;
	constexpr float kDegenerateEps = 1e-8f;
	constexpr float kGeometricEdgeEps = 1e-4f;
	constexpr float kGeometricEdgeEpsSq = kGeometricEdgeEps * kGeometricEdgeEps;

	struct QuantizedPointKey
	{
		int64 x = 0;
		int64 y = 0;
		int64 z = 0;

		bool operator==(const QuantizedPointKey& rhs) const
		{
			return x == rhs.x && y == rhs.y && z == rhs.z;
		}

		bool operator<(const QuantizedPointKey& rhs) const
		{
			if (x != rhs.x) return x < rhs.x;
			if (y != rhs.y) return y < rhs.y;
			return z < rhs.z;
		}
	};

	struct QuantizedPointKeyHasher
	{
		size_t operator()(const QuantizedPointKey& k) const noexcept
		{
			const size_t hx = std::hash<int64>{}(k.x);
			const size_t hy = std::hash<int64>{}(k.y);
			const size_t hz = std::hash<int64>{}(k.z);
			return hx ^ (hy + 0x9e3779b9ull + (hx << 6) + (hx >> 2))
				^ (hz + 0x9e3779b9ull + (hy << 6) + (hy >> 2));
		}
	};

	QuantizedPointKey MakeQuantizedPointKey(const GameMath::Vec3& p)
	{
		return QuantizedPointKey
		{
			static_cast<int64>(std::llround(static_cast<double>(p.x) / static_cast<double>(kGeometricEdgeEps))),
			static_cast<int64>(std::llround(static_cast<double>(p.y) / static_cast<double>(kGeometricEdgeEps))),
			static_cast<int64>(std::llround(static_cast<double>(p.z) / static_cast<double>(kGeometricEdgeEps)))
		};
	}

	struct GeometricEdgeKey
	{
		QuantizedPointKey a{};
		QuantizedPointKey b{};

		GeometricEdgeKey() = default;
		GeometricEdgeKey(const GameMath::Vec3& p0, const GameMath::Vec3& p1)
		{
			QuantizedPointKey k0 = MakeQuantizedPointKey(p0);
			QuantizedPointKey k1 = MakeQuantizedPointKey(p1);
			if (k1 < k0)
				std::swap(k0, k1);
			a = k0;
			b = k1;
		}

		bool operator==(const GeometricEdgeKey& rhs) const
		{
			return a == rhs.a && b == rhs.b;
		}
	};

	struct GeometricEdgeKeyHasher
	{
		size_t operator()(const GeometricEdgeKey& k) const noexcept
		{
			const size_t h0 = QuantizedPointKeyHasher{}(k.a);
			const size_t h1 = QuantizedPointKeyHasher{}(k.b);
			return h0 ^ (h1 + 0x9e3779b9ull + (h0 << 6) + (h0 >> 2));
		}
	};

	struct EdgeRef
	{
		int triangleIndex = -1;
		int edgeIndex = -1;
	};

	float DistSqXZ(const GameMath::Vec3& a, const GameMath::Vec3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;
		return dx * dx + dz * dz;
	}

	float DistXZ(const GameMath::Vec3& a, const GameMath::Vec3& b)
	{
		return sqrtf(DistSqXZ(a, b));
	}

	float Cross2D_XZ(const GameMath::Vec3& a, const GameMath::Vec3& b, const GameMath::Vec3& c)
	{
		return ((b.x - a.x) * (c.z - a.z)) - ((b.z - a.z) * (c.x - a.x));
	}

	bool NearlyEqualXZ(const GameMath::Vec3& a, const GameMath::Vec3& b)
	{
		return DistSqXZ(a, b) <= 1e-6f;
	}

	bool AppendPointIfNotDuplicate(std::vector<GameMath::Vec3>& points, const GameMath::Vec3& p)
	{
		if (!points.empty() && NearlyEqualXZ(points.back(), p))
			return false;
		points.push_back(p);
		return true;
	}

	float DistSq3D(const GameMath::Vec3& a, const GameMath::Vec3& b)
	{
		const float dx = a.x - b.x;
		const float dy = a.y - b.y;
		const float dz = a.z - b.z;
		return dx * dx + dy * dy + dz * dz;
	}

	bool NearlyEqual3D(const GameMath::Vec3& a, const GameMath::Vec3& b, float epsSq = kGeometricEdgeEpsSq)
	{
		return DistSq3D(a, b) <= epsSq;
	}

	bool IsSameUndirectedEdge3D(const GameMath::Vec3& a0, const GameMath::Vec3& a1, const GameMath::Vec3& b0, const GameMath::Vec3& b1)
	{
		const bool sameForward = NearlyEqual3D(a0, b0) && NearlyEqual3D(a1, b1);
		const bool sameReverse = NearlyEqual3D(a0, b1) && NearlyEqual3D(a1, b0);
		return sameForward || sameReverse;
	}

	void GetTriangleEdgeVertexIndices(const NAVMESH_TRIANGLE& tri, int edgeIndex, uint32& outA, uint32& outB)
	{
		switch (edgeIndex)
		{
		case 0: outA = tri.i0; outB = tri.i1; break;
		case 1: outA = tri.i1; outB = tri.i2; break;
		case 2: outA = tri.i2; outB = tri.i0; break;
		default: outA = tri.i0; outB = tri.i1; break;
		}
	}

	int* GetTriangleNeighborPtr(NAVMESH_TRIANGLE& tri, int edgeIndex)
	{
		switch (edgeIndex)
		{
		case 0: return &tri.n0;
		case 1: return &tri.n1;
		case 2: return &tri.n2;
		default: return nullptr;
		}
	}

	int GetTriangleNeighbor(const NAVMESH_TRIANGLE& tri, int edgeIndex)
	{
		switch (edgeIndex)
		{
		case 0: return tri.n0;
		case 1: return tri.n1;
		case 2: return tri.n2;
		default: return -1;
		}
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

	for (auto& tri : m_triangles)
	{
		auto sanitize = [&](int32& n)
			{
				if (n < 0 || n >= static_cast<int32>(m_triangles.size()))
					n = -1;
			};
		sanitize(tri.n0);
		sanitize(tri.n1);
		sanitize(tri.n2);
	}

	BuildNeighborsFromGeometry();

	m_loaded = !m_vertices.empty() && !m_triangles.empty();
	return m_loaded;
}

void CNavMesh::BuildNeighborsFromGeometry()
{
	for (NAVMESH_TRIANGLE& tri : m_triangles)
	{
		tri.n0 = -1;
		tri.n1 = -1;
		tri.n2 = -1;
	}

	std::unordered_map<GeometricEdgeKey, std::vector<EdgeRef>, GeometricEdgeKeyHasher> edgeBuckets;
	edgeBuckets.reserve(m_triangles.size() * 3);

	for (int triIndex = 0; triIndex < static_cast<int>(m_triangles.size()); ++triIndex)
	{
		const NAVMESH_TRIANGLE& tri = m_triangles[triIndex];
		for (int edgeIndex = 0; edgeIndex < 3; ++edgeIndex)
		{
			uint32 ia = 0;
			uint32 ib = 0;
			GetTriangleEdgeVertexIndices(tri, edgeIndex, ia, ib);

			const GameMath::Vec3& va = m_vertices[ia];
			const GameMath::Vec3& vb = m_vertices[ib];
			if (DistSq3D(va, vb) <= kDegenerateEps)
				continue;

			edgeBuckets[GeometricEdgeKey(va, vb)].push_back(EdgeRef{ triIndex, edgeIndex });
		}
	}

	for (auto& kv : edgeBuckets)
	{
		std::vector<EdgeRef>& refs = kv.second;
		if (refs.size() < 2)
			continue;

		for (size_t i = 0; i < refs.size(); ++i)
		{
			const EdgeRef lhsRef = refs[i];
			if (lhsRef.triangleIndex < 0)
				continue;
			if (GetTriangleNeighbor(m_triangles[lhsRef.triangleIndex], lhsRef.edgeIndex) >= 0)
				continue;

			uint32 lhsIa = 0;
			uint32 lhsIb = 0;
			GetTriangleEdgeVertexIndices(m_triangles[lhsRef.triangleIndex], lhsRef.edgeIndex, lhsIa, lhsIb);
			const GameMath::Vec3& lhsA = m_vertices[lhsIa];
			const GameMath::Vec3& lhsB = m_vertices[lhsIb];

			for (size_t j = i + 1; j < refs.size(); ++j)
			{
				const EdgeRef rhsRef = refs[j];
				if (rhsRef.triangleIndex < 0)
					continue;
				if (lhsRef.triangleIndex == rhsRef.triangleIndex)
					continue;
				if (GetTriangleNeighbor(m_triangles[rhsRef.triangleIndex], rhsRef.edgeIndex) >= 0)
					continue;

				uint32 rhsIa = 0;
				uint32 rhsIb = 0;
				GetTriangleEdgeVertexIndices(m_triangles[rhsRef.triangleIndex], rhsRef.edgeIndex, rhsIa, rhsIb);
				const GameMath::Vec3& rhsA = m_vertices[rhsIa];
				const GameMath::Vec3& rhsB = m_vertices[rhsIb];

				if (!IsSameUndirectedEdge3D(lhsA, lhsB, rhsA, rhsB))
					continue;

				if (int* pLhs = GetTriangleNeighborPtr(m_triangles[lhsRef.triangleIndex], lhsRef.edgeIndex))
					*pLhs = rhsRef.triangleIndex;
				if (int* pRhs = GetTriangleNeighborPtr(m_triangles[rhsRef.triangleIndex], rhsRef.edgeIndex))
					*pRhs = lhsRef.triangleIndex;

				break;
			}
		}
	}
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

bool CNavMesh::GetTrianglePortal(int fromTriangle, int toTriangle, GameMath::Vec3& outA, GameMath::Vec3& outB) const
{
	if (fromTriangle < 0 || fromTriangle >= static_cast<int>(m_triangles.size()))
		return false;
	if (toTriangle < 0 || toTriangle >= static_cast<int>(m_triangles.size()))
		return false;

	const NAVMESH_TRIANGLE& tri = m_triangles[fromTriangle];
	for (int edgeIndex = 0; edgeIndex < 3; ++edgeIndex)
	{
		if (GetTriangleNeighbor(tri, edgeIndex) != toTriangle)
			continue;

		uint32 ia = 0;
		uint32 ib = 0;
		GetTriangleEdgeVertexIndices(tri, edgeIndex, ia, ib);
		outA = m_vertices[ia];
		outB = m_vertices[ib];
		return true;
	}

	return false;
}

bool CNavMesh::FindStraightPath(const GameMath::Vec3& startProj, const GameMath::Vec3& goalProj, const std::vector<int>& trianglePath, std::vector<GameMath::Vec3>& outStraightPath) const
{
	outStraightPath.clear();
	if (trianglePath.empty())
		return false;

	if (trianglePath.size() == 1)
	{
		AppendPointIfNotDuplicate(outStraightPath, startProj);
		AppendPointIfNotDuplicate(outStraightPath, goalProj);
		return true;
	}

	struct Portal
	{
		GameMath::Vec3 left = GameMath::Vec3::Zero();
		GameMath::Vec3 right = GameMath::Vec3::Zero();
	};

	std::vector<Portal> portals;
	portals.reserve(trianglePath.size() + 1);
	portals.push_back(Portal{ startProj, startProj });

	for (size_t i = 0; i + 1 < trianglePath.size(); ++i)
	{
		GameMath::Vec3 a{};
		GameMath::Vec3 b{};
		if (!GetTrianglePortal(trianglePath[i], trianglePath[i + 1], a, b))
			return false;
		portals.push_back(Portal{ a, b });
	}

	portals.push_back(Portal{ goalProj, goalProj });

	GameMath::Vec3 portalApex = portals[0].left;
	GameMath::Vec3 portalLeft = portals[0].left;
	GameMath::Vec3 portalRight = portals[0].right;

	size_t apexIndex = 0;
	size_t leftIndex = 0;
	size_t rightIndex = 0;

	AppendPointIfNotDuplicate(outStraightPath, portalApex);

	for (size_t i = 1; i < portals.size(); ++i)
	{
		GameMath::Vec3 left = portals[i].left;
		GameMath::Vec3 right = portals[i].right;

		if (Cross2D_XZ(portalApex, left, right) < 0.0f)
			std::swap(left, right);

		if (NearlyEqualXZ(portalApex, portalRight) || Cross2D_XZ(portalApex, portalRight, right) <= 0.0f)
		{
			if (NearlyEqualXZ(portalApex, portalRight) || Cross2D_XZ(portalApex, portalLeft, right) > 0.0f)
			{
				portalRight = right;
				rightIndex = i;
			}
			else
			{
				AppendPointIfNotDuplicate(outStraightPath, portalLeft);

				portalApex = portalLeft;
				apexIndex = leftIndex;

				portalLeft = portalApex;
				portalRight = portalApex;
				leftIndex = apexIndex;
				rightIndex = apexIndex;

				i = apexIndex;
				continue;
			}
		}

		if (NearlyEqualXZ(portalApex, portalLeft) || Cross2D_XZ(portalApex, portalLeft, left) >= 0.0f)
		{
			if (NearlyEqualXZ(portalApex, portalLeft) || Cross2D_XZ(portalApex, portalRight, left) < 0.0f)
			{
				portalLeft = left;
				leftIndex = i;
			}
			else
			{
				AppendPointIfNotDuplicate(outStraightPath, portalRight);

				portalApex = portalRight;
				apexIndex = rightIndex;

				portalLeft = portalApex;
				portalRight = portalApex;
				leftIndex = apexIndex;
				rightIndex = apexIndex;

				i = apexIndex;
				continue;
			}
		}
	}

	AppendPointIfNotDuplicate(outStraightPath, goalProj);

	for (size_t i = 0; i < outStraightPath.size(); ++i)
	{
		GameMath::Vec3 corrected{};
		if (SamplePosition(outStraightPath[i], corrected))
			outStraightPath[i] = corrected;
	}

	return !outStraightPath.empty();
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

	return FindStraightPath(startProj, goalProj, outTrianglePath, outStraightPath);
}
