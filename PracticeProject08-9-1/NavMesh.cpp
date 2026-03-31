//-----------------------------------------------------------------------------
// File: NavMesh.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "NavMesh.h"

#include <fstream>
#include <queue>
#include <limits>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <unordered_map>

namespace
{
	static constexpr uint32_t kNavMeshMagic = 0x484D564E; // 'NVMH'
	static constexpr uint32_t kNavMeshVersion = 1;
	static constexpr uint32_t kNavMeshFlagHasNeighbors = ( 1u << 0 );

	static constexpr float kInsideEpsilon = 1e-4f;
	static constexpr float kDegenerateEpsilon = 1e-8f;
	static constexpr float kPointEqualEpsilonSq = 1e-6f;

	struct EdgeKey
	{
		int a = 0;
		int b = 0;

		EdgeKey() = default;
		EdgeKey(int i0, int i1)
		{
			if ( i0 < i1 )
			{
				a = i0;
				b = i1;
			}
			else
			{
				a = i1;
				b = i0;
			}
		}

		bool operator==(const EdgeKey& rhs) const
		{
			return ( a == rhs.a ) && ( b == rhs.b );
		}
	};

	struct EdgeKeyHasher
	{
		size_t operator()(const EdgeKey& k) const noexcept
		{
			size_t h0 = static_cast< size_t >( k.a ) * 73856093u;
			size_t h1 = static_cast< size_t >( k.b ) * 19349663u;
			return h0 ^ h1;
		}
	};

	struct EdgeRef
	{
		int triangleIndex = -1;
		int edgeIndex = -1;
	};

	struct ClosestPointResult
	{
		XMFLOAT3 point = XMFLOAT3(0.0f, 0.0f, 0.0f);
		float bary0 = 0.0f;
		float bary1 = 0.0f;
		float bary2 = 0.0f;
		float distSqXZ = FLT_MAX;
	};

	static float DistanceSqXZ(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;
		return ( dx * dx ) + ( dz * dz );
	}

	static float DistanceXZ(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		return std::sqrt(DistanceSqXZ(a, b));
	}

	static float Cross2D_XZ(const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c)
	{
		// (b - a) x (c - a) 의 y성분과 동일한 부호를 갖는 XZ 평면 외적
		return ( ( b.x - a.x ) * ( c.z - a.z ) ) - ( ( b.z - a.z ) * ( c.x - a.x ) );
	}

	static bool NearlyEqualXZ(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		return DistanceSqXZ(a, b) <= kPointEqualEpsilonSq;
	}

	static bool ComputeBarycentricXZ(
		const XMFLOAT3& p,
		const XMFLOAT3& a,
		const XMFLOAT3& b,
		const XMFLOAT3& c,
		float& outU,
		float& outV,
		float& outW)
	{
		const float v0x = b.x - a.x;
		const float v0z = b.z - a.z;
		const float v1x = c.x - a.x;
		const float v1z = c.z - a.z;
		const float v2x = p.x - a.x;
		const float v2z = p.z - a.z;

		const float d00 = ( v0x * v0x ) + ( v0z * v0z );
		const float d01 = ( v0x * v1x ) + ( v0z * v1z );
		const float d11 = ( v1x * v1x ) + ( v1z * v1z );
		const float d20 = ( v2x * v0x ) + ( v2z * v0z );
		const float d21 = ( v2x * v1x ) + ( v2z * v1z );

		const float denom = ( d00 * d11 ) - ( d01 * d01 );
		if ( std::fabs(denom) <= kDegenerateEpsilon )
			return false;

		outV = ( ( d11 * d20 ) - ( d01 * d21 ) ) / denom;
		outW = ( ( d00 * d21 ) - ( d01 * d20 ) ) / denom;
		outU = 1.0f - outV - outW;
		return true;
	}

	static XMFLOAT3 InterpolateBarycentric(
		const XMFLOAT3& a,
		const XMFLOAT3& b,
		const XMFLOAT3& c,
		float u,
		float v,
		float w)
	{
		return XMFLOAT3(
			( a.x * u ) + ( b.x * v ) + ( c.x * w ),
			( a.y * u ) + ( b.y * v ) + ( c.y * w ),
			( a.z * u ) + ( b.z * v ) + ( c.z * w )
		);
	}

	static XMFLOAT3 ClosestPointOnSegmentXZ(
		const XMFLOAT3& p,
		const XMFLOAT3& a,
		const XMFLOAT3& b,
		float& outT)
	{
		const float abx = b.x - a.x;
		const float abz = b.z - a.z;
		const float apx = p.x - a.x;
		const float apz = p.z - a.z;

		const float abLenSq = ( abx * abx ) + ( abz * abz );
		if ( abLenSq <= kDegenerateEpsilon )
		{
			outT = 0.0f;
			return a;
		}

		float t = ( ( apx * abx ) + ( apz * abz ) ) / abLenSq;
		if ( t < 0.0f ) t = 0.0f;
		else if ( t > 1.0f ) t = 1.0f;
		outT = t;

		return XMFLOAT3(
			a.x + ( abx * t ),
			0.0f,
			a.z + ( abz * t )
		);
	}

	static ClosestPointResult ClosestPointOnTriangleXZ(
		const XMFLOAT3& p,
		const XMFLOAT3& a,
		const XMFLOAT3& b,
		const XMFLOAT3& c)
	{
		ClosestPointResult best{};

		float u = 0.0f, v = 0.0f, w = 0.0f;
		if ( ComputeBarycentricXZ(p, a, b, c, u, v, w) )
		{
			if ( u >= -kInsideEpsilon && v >= -kInsideEpsilon && w >= -kInsideEpsilon )
			{
				u = ( u > 0.0f ) ? u : 0.0f;
				v = ( v > 0.0f ) ? v : 0.0f;
				w = ( w > 0.0f ) ? w : 0.0f;

				const float sum = u + v + w;
				if ( sum > kDegenerateEpsilon )
				{
					u /= sum;
					v /= sum;
					w /= sum;
				}

				best.bary0 = u;
				best.bary1 = v;
				best.bary2 = w;
				best.point = InterpolateBarycentric(a, b, c, u, v, w);
				best.distSqXZ = DistanceSqXZ(p, best.point);
				return best;
			}
		}

		// edge AB
		{
			float t = 0.0f;
			XMFLOAT3 cp = ClosestPointOnSegmentXZ(p, a, b, t);

			ClosestPointResult cand{};
			cand.bary0 = 1.0f - t;
			cand.bary1 = t;
			cand.bary2 = 0.0f;
			cand.point = InterpolateBarycentric(a, b, c, cand.bary0, cand.bary1, cand.bary2);
			cand.distSqXZ = DistanceSqXZ(p, cand.point);

			if ( cand.distSqXZ < best.distSqXZ )
				best = cand;
		}

		// edge BC
		{
			float t = 0.0f;
			XMFLOAT3 cp = ClosestPointOnSegmentXZ(p, b, c, t);

			ClosestPointResult cand{};
			cand.bary0 = 0.0f;
			cand.bary1 = 1.0f - t;
			cand.bary2 = t;
			cand.point = InterpolateBarycentric(a, b, c, cand.bary0, cand.bary1, cand.bary2);
			cand.distSqXZ = DistanceSqXZ(p, cand.point);

			if ( cand.distSqXZ < best.distSqXZ )
				best = cand;
		}

		// edge CA
		{
			float t = 0.0f;
			XMFLOAT3 cp = ClosestPointOnSegmentXZ(p, c, a, t);

			ClosestPointResult cand{};
			cand.bary0 = t;
			cand.bary1 = 0.0f;
			cand.bary2 = 1.0f - t;
			cand.point = InterpolateBarycentric(a, b, c, cand.bary0, cand.bary1, cand.bary2);
			cand.distSqXZ = DistanceSqXZ(p, cand.point);

			if ( cand.distSqXZ < best.distSqXZ )
				best = cand;
		}

		return best;
	}

	static bool AppendPointIfNotDuplicate(std::vector<XMFLOAT3>& points, const XMFLOAT3& p)
	{
		if ( !points.empty() )
		{
			if ( NearlyEqualXZ(points.back(), p) )
				return false;
		}

		points.push_back(p);
		return true;
	}

	static void GetTriangleEdgeVertexIndices(const NAVMESH_TRIANGLE& tri, int edgeIndex, uint32_t& outA, uint32_t& outB)
	{
		switch ( edgeIndex )
		{
		case 0:
			outA = tri.i0; outB = tri.i1;
			break;
		case 1:
			outA = tri.i1; outB = tri.i2;
			break;
		case 2:
			outA = tri.i2; outB = tri.i0;
			break;
		default:
			outA = tri.i0; outB = tri.i1;
			break;
		}
	}

	static int* GetTriangleNeighborPtr(NAVMESH_TRIANGLE& tri, int edgeIndex)
	{
		switch ( edgeIndex )
		{
		case 0: return &tri.n0;
		case 1: return &tri.n1;
		case 2: return &tri.n2;
		default: return nullptr;
		}
	}

	static int GetTriangleNeighbor(const NAVMESH_TRIANGLE& tri, int edgeIndex)
	{
		switch ( edgeIndex )
		{
		case 0: return tri.n0;
		case 1: return tri.n1;
		case 2: return tri.n2;
		default: return -1;
		}
	}
}

CNavMesh::CNavMesh()
{
	Clear();
}

CNavMesh::~CNavMesh()
{
	Clear();
}

void CNavMesh::Clear()
{
	m_bLoaded = false;
	m_fileHeader = {};

	m_vertices.clear();
	m_triangles.clear();
	m_triangleRuntime.clear();
	m_areaCosts.clear();

	m_boundsMin = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_boundsMax = XMFLOAT3(0.0f, 0.0f, 0.0f);
}

const XMFLOAT3& CNavMesh::GetVertex(uint32_t index) const
{
	static const XMFLOAT3 kDummy(0.0f, 0.0f, 0.0f);

	if ( index >= m_vertices.size() )
	{
		assert(false && "NavMesh vertex index out of range");
		return kDummy;
	}

	return m_vertices[index];
}

const NAVMESH_TRIANGLE& CNavMesh::GetTriangle(uint32_t index) const
{
	static const NAVMESH_TRIANGLE kDummy{};

	if ( index >= m_triangles.size() )
	{
		assert(false && "NavMesh triangle index out of range");
		return kDummy;
	}

	return m_triangles[index];
}

void CNavMesh::SetAreaCost(uint32_t areaId, float cost)
{
	if ( cost <= 0.0f )
		cost = 0.001f;

	if ( areaId >= m_areaCosts.size() )
		m_areaCosts.resize(areaId + 1, 1.0f);

	m_areaCosts[areaId] = cost;
}

float CNavMesh::GetAreaCost(uint32_t areaId) const
{
	if ( areaId >= m_areaCosts.size() )
		return 1.0f;

	const float c = m_areaCosts[areaId];
	return ( c > 0.0f ) ? c : 1.0f;
}

bool CNavMesh::LoadFromFile(const std::string& filePath)
{
	Clear();

	std::ifstream fin(filePath, std::ios::binary);
	if ( !fin.is_open() )
		return false;

	NAVMESH_FILE_HEADER_V1 header{};
	fin.read(reinterpret_cast< char* >( &header ), sizeof(header));
	if ( !fin.good() )
		return false;

	if ( header.magic != kNavMeshMagic )
		return false;

	if ( header.version != kNavMeshVersion )
		return false;

	if ( header.vertexCount == 0 || header.triangleCount == 0 )
		return false;

	std::vector<NAVMESH_VERTEX> fileVertices(header.vertexCount);
	std::vector<NAVMESH_TRIANGLE> fileTriangles(header.triangleCount);

	fin.read(reinterpret_cast< char* >( fileVertices.data() ), sizeof(NAVMESH_VERTEX) * fileVertices.size());
	if ( !fin.good() )
		return false;

	fin.read(reinterpret_cast< char* >( fileTriangles.data() ), sizeof(NAVMESH_TRIANGLE) * fileTriangles.size());
	if ( !fin.good() )
		return false;

	m_fileHeader = header;

	m_vertices.resize(fileVertices.size());
	for ( size_t i = 0; i < fileVertices.size(); ++i )
	{
		m_vertices[i] = XMFLOAT3(
			fileVertices[i].x,
			fileVertices[i].y,
			fileVertices[i].z
		);
	}

	m_triangles = std::move(fileTriangles);

	// neighbor 값이 파일에 있어도 기본적인 범위 보정은 한다.
	for ( size_t i = 0; i < m_triangles.size(); ++i )
	{
		NAVMESH_TRIANGLE& tri = m_triangles[i];

		auto SanitizeNeighbor = [ this ] (int32_t& n)
			{
				if ( n < 0 )
				{
					n = -1;
					return;
				}

				if ( n >= static_cast< int32_t >(m_triangles.size()) )
				{
					n = -1;
					return;
				}
			};

		SanitizeNeighbor(tri.n0);
		SanitizeNeighbor(tri.n1);
		SanitizeNeighbor(tri.n2);
	}

	if ( !ValidateLoadedData() )
	{
		Clear();
		return false;
	}

	// 파일에 neighbor가 없거나, geometry 기준으로 다시 보장하고 싶으면 재구축
	if ( ( m_fileHeader.flags & kNavMeshFlagHasNeighbors ) == 0u )
	{
		BuildNeighborsFromGeometry();
	}

	BuildRuntimeData();

	// area cost table 생성
	uint32_t maxArea = 0;
	for ( const NAVMESH_TRIANGLE& tri : m_triangles )
	{
		if ( tri.area > maxArea ) maxArea = tri.area;
	}
	m_areaCosts.assign(static_cast< size_t >( maxArea ) + 1u, 1.0f);

	m_bLoaded = true;
	return true;
}

bool CNavMesh::ValidateLoadedData() const
{
	if ( m_vertices.empty() ) return false;
	if ( m_triangles.empty() ) return false;

	for ( const NAVMESH_TRIANGLE& tri : m_triangles )
	{
		if ( tri.i0 >= m_vertices.size() ) return false;
		if ( tri.i1 >= m_vertices.size() ) return false;
		if ( tri.i2 >= m_vertices.size() ) return false;
	}

	return true;
}

void CNavMesh::BuildRuntimeData()
{
	m_triangleRuntime.clear();
	m_triangleRuntime.resize(m_triangles.size());

	if ( m_vertices.empty() )
	{
		m_boundsMin = XMFLOAT3(0.0f, 0.0f, 0.0f);
		m_boundsMax = XMFLOAT3(0.0f, 0.0f, 0.0f);
		return;
	}

	m_boundsMin = m_vertices[0];
	m_boundsMax = m_vertices[0];

	for ( const XMFLOAT3& v : m_vertices )
	{
		m_boundsMin.x = ( m_boundsMin.x < v.x ) ? m_boundsMin.x : v.x;
		m_boundsMin.y = ( m_boundsMin.y < v.y ) ? m_boundsMin.y : v.y;
		m_boundsMin.z = ( m_boundsMin.z < v.z ) ? m_boundsMin.z : v.z;

		m_boundsMax.x = ( m_boundsMax.x > v.x ) ? m_boundsMax.x : v.x;
		m_boundsMax.y = ( m_boundsMax.y > v.y ) ? m_boundsMax.y : v.y;
		m_boundsMax.z = ( m_boundsMax.z > v.z ) ? m_boundsMax.z : v.z;
	}

	for ( size_t i = 0; i < m_triangles.size(); ++i )
	{
		const NAVMESH_TRIANGLE& tri = m_triangles[i];
		const XMFLOAT3& a = m_vertices[tri.i0];
		const XMFLOAT3& b = m_vertices[tri.i1];
		const XMFLOAT3& c = m_vertices[tri.i2];

		TriangleRuntimeData rt{};

		// minX
		rt.minX = ( a.x < b.x ) ? a.x : b.x;
		rt.minX = ( rt.minX < c.x ) ? rt.minX : c.x;

		// maxX
		rt.maxX = ( a.x > b.x ) ? a.x : b.x;
		rt.maxX = ( rt.maxX > c.x ) ? rt.maxX : c.x;

		// minZ
		rt.minZ = ( a.z < b.z ) ? a.z : b.z;
		rt.minZ = ( rt.minZ < c.z ) ? rt.minZ : c.z;

		// maxZ
		rt.maxZ = ( a.z > b.z ) ? a.z : b.z;
		rt.maxZ = ( rt.maxZ > c.z ) ? rt.maxZ : c.z;


		rt.centroid = XMFLOAT3(
			( a.x + b.x + c.x ) / 3.0f,
			( a.y + b.y + c.y ) / 3.0f,
			( a.z + b.z + c.z ) / 3.0f
		);

		m_triangleRuntime[i] = rt;
	}
}

void CNavMesh::BuildNeighborsFromGeometry()
{
	std::unordered_map<EdgeKey, EdgeRef, EdgeKeyHasher> edgeMap;
	edgeMap.reserve(m_triangles.size() * 3);

	for ( NAVMESH_TRIANGLE& tri : m_triangles )
	{
		tri.n0 = -1;
		tri.n1 = -1;
		tri.n2 = -1;
	}

	for ( int triIndex = 0; triIndex < static_cast< int >(m_triangles.size()); ++triIndex )
	{
		NAVMESH_TRIANGLE& tri = m_triangles[triIndex];

		for ( int edgeIndex = 0; edgeIndex < 3; ++edgeIndex )
		{
			uint32_t ia = 0, ib = 0;
			GetTriangleEdgeVertexIndices(tri, edgeIndex, ia, ib);

			EdgeKey key(static_cast< int >(ia), static_cast< int >(ib));

			auto it = edgeMap.find(key);
			if ( it == edgeMap.end() )
			{
				edgeMap.emplace(key, EdgeRef{ triIndex, edgeIndex });
			}
			else
			{
				EdgeRef other = it->second;

				int* pThis = GetTriangleNeighborPtr(tri, edgeIndex);
				int* pOther = GetTriangleNeighborPtr(m_triangles[other.triangleIndex], other.edgeIndex);

				if ( pThis )  *pThis = other.triangleIndex;
				if ( pOther ) *pOther = triIndex;
			}
		}
	}
}

bool CNavMesh::IsPointInsideBounds(const XMFLOAT3& worldPos) const
{
	if ( !m_bLoaded ) return false;

	return
		( worldPos.x >= m_boundsMin.x && worldPos.x <= m_boundsMax.x ) &&
		( worldPos.y >= m_boundsMin.y && worldPos.y <= m_boundsMax.y ) &&
		( worldPos.z >= m_boundsMin.z && worldPos.z <= m_boundsMax.z );
}

bool CNavMesh::ProjectPointToTriangleXZ(
	int triangleIndex,
	const XMFLOAT3& inputPos,
	XMFLOAT3& outProjectedPos,
	float* outDistSqXZ,
	float* outVerticalAbs) const
{
	if ( triangleIndex < 0 || triangleIndex >= static_cast< int >(m_triangles.size()) )
		return false;

	const NAVMESH_TRIANGLE& tri = m_triangles[triangleIndex];
	const XMFLOAT3& a = m_vertices[tri.i0];
	const XMFLOAT3& b = m_vertices[tri.i1];
	const XMFLOAT3& c = m_vertices[tri.i2];

	ClosestPointResult cp = ClosestPointOnTriangleXZ(inputPos, a, b, c);
	outProjectedPos = cp.point;

	if ( outDistSqXZ )
		*outDistSqXZ = cp.distSqXZ;

	if ( outVerticalAbs )
		*outVerticalAbs = std::fabs(inputPos.y - cp.point.y);

	return true;
}

bool CNavMesh::FindContainingTriangle(const XMFLOAT3& worldPos, int& outTriangleIndex) const
{
	outTriangleIndex = -1;
	if ( !m_bLoaded ) return false;

	float bestVerticalAbs = FLT_MAX;

	for ( int i = 0; i < static_cast< int >(m_triangles.size()); ++i )
	{
		const TriangleRuntimeData& rt = m_triangleRuntime[i];

		if ( worldPos.x < ( rt.minX - kInsideEpsilon ) || worldPos.x >(rt.maxX + kInsideEpsilon) )
			continue;
		if ( worldPos.z < ( rt.minZ - kInsideEpsilon ) || worldPos.z >(rt.maxZ + kInsideEpsilon) )
			continue;

		const NAVMESH_TRIANGLE& tri = m_triangles[i];
		const XMFLOAT3& a = m_vertices[tri.i0];
		const XMFLOAT3& b = m_vertices[tri.i1];
		const XMFLOAT3& c = m_vertices[tri.i2];

		float u = 0.0f, v = 0.0f, w = 0.0f;
		if ( !ComputeBarycentricXZ(worldPos, a, b, c, u, v, w) )
			continue;

		if ( u >= -kInsideEpsilon && v >= -kInsideEpsilon && w >= -kInsideEpsilon )
		{
			const float y =
				( a.y * u ) +
				( b.y * v ) +
				( c.y * w );

			const float verticalAbs = std::fabs(worldPos.y - y);
			if ( verticalAbs < bestVerticalAbs )
			{
				bestVerticalAbs = verticalAbs;
				outTriangleIndex = i;
			}
		}
	}

	return ( outTriangleIndex >= 0 );
}

int CNavMesh::FindNearestTriangle(const XMFLOAT3& worldPos, float maxSearchDistanceXZ) const
{
	if ( !m_bLoaded ) return -1;

	int containing = -1;
	if ( FindContainingTriangle(worldPos, containing) )
		return containing;

	const float maxDistSq = ( maxSearchDistanceXZ >= FLT_MAX * 0.5f )
		? FLT_MAX
		: ( maxSearchDistanceXZ * maxSearchDistanceXZ );

	int bestTri = -1;
	float bestDistSq = FLT_MAX;
	float bestVerticalAbs = FLT_MAX;

	for ( int i = 0; i < static_cast< int >(m_triangles.size()); ++i )
	{
		XMFLOAT3 projected{};
		float distSq = FLT_MAX;
		float verticalAbs = FLT_MAX;

		if ( !ProjectPointToTriangleXZ(i, worldPos, projected, &distSq, &verticalAbs) )
			continue;

		if ( distSq > maxDistSq )
			continue;

		if ( distSq < bestDistSq )
		{
			bestDistSq = distSq;
			bestVerticalAbs = verticalAbs;
			bestTri = i;
			continue;
		}

		if ( std::fabs(distSq - bestDistSq) <= 1e-6f )
		{
			if ( verticalAbs < bestVerticalAbs )
			{
				bestVerticalAbs = verticalAbs;
				bestTri = i;
			}
		}
	}

	return bestTri;
}

bool CNavMesh::SamplePosition(
	const XMFLOAT3& worldPos,
	XMFLOAT3& outProjectedPos,
	int* outTriangleIndex,
	float maxSearchDistanceXZ) const
{
	if ( outTriangleIndex ) *outTriangleIndex = -1;
	if ( !m_bLoaded ) return false;

	int triIndex = -1;
	if ( !FindContainingTriangle(worldPos, triIndex) )
	{
		triIndex = FindNearestTriangle(worldPos, maxSearchDistanceXZ);
		if ( triIndex < 0 )
			return false;
	}

	if ( !ProjectPointToTriangleXZ(triIndex, worldPos, outProjectedPos, nullptr, nullptr) )
		return false;

	if ( outTriangleIndex )
		*outTriangleIndex = triIndex;

	return true;
}

bool CNavMesh::GetTriangleCentroid(int triangleIndex, XMFLOAT3& outCentroid) const
{
	if ( !m_bLoaded ) return false;
	if ( triangleIndex < 0 || triangleIndex >= static_cast< int >(m_triangleRuntime.size()) )
		return false;

	outCentroid = m_triangleRuntime[triangleIndex].centroid;
	return true;
}

bool CNavMesh::GetTrianglePortal(int fromTriangle, int toTriangle, XMFLOAT3& outA, XMFLOAT3& outB) const
{
	if ( !m_bLoaded ) return false;
	if ( fromTriangle < 0 || fromTriangle >= static_cast< int >(m_triangles.size()) ) return false;
	if ( toTriangle < 0 || toTriangle >= static_cast< int >(m_triangles.size()) ) return false;

	const NAVMESH_TRIANGLE& tri = m_triangles[fromTriangle];

	for ( int edgeIndex = 0; edgeIndex < 3; ++edgeIndex )
	{
		const int neighbor = GetTriangleNeighbor(tri, edgeIndex);
		if ( neighbor != toTriangle )
			continue;

		uint32_t ia = 0, ib = 0;
		GetTriangleEdgeVertexIndices(tri, edgeIndex, ia, ib);

		outA = m_vertices[ia];
		outB = m_vertices[ib];
		return true;
	}

	return false;
}

bool CNavMesh::FindTrianglePath(
	const XMFLOAT3& startPos,
	const XMFLOAT3& goalPos,
	std::vector<int>& outTrianglePath) const
{
	outTrianglePath.clear();
	if ( !m_bLoaded ) return false;

	XMFLOAT3 startProj{};
	XMFLOAT3 goalProj{};
	int startTri = -1;
	int goalTri = -1;

	if ( !SamplePosition(startPos, startProj, &startTri) )
		return false;

	if ( !SamplePosition(goalPos, goalProj, &goalTri) )
		return false;

	if ( startTri < 0 || goalTri < 0 )
		return false;

	if ( startTri == goalTri )
	{
		outTrianglePath.push_back(startTri);
		return true;
	}

	struct OpenNode
	{
		int triangleIndex = -1;
		float fScore = FLT_MAX;
	};

	struct OpenNodeGreater
	{
		bool operator()(const OpenNode& a, const OpenNode& b) const
		{
			return a.fScore > b.fScore;
		}
	};

	const int triCount = static_cast< int >( m_triangles.size() );

	std::vector<float> gScore(triCount, FLT_MAX);
	std::vector<float> fScore(triCount, FLT_MAX);
	std::vector<int> cameFrom(triCount, -1);
	std::vector<uint8_t> closed(triCount, 0);

	auto Heuristic = [ this, &goalProj ] (int triIndex) -> float
		{
			const XMFLOAT3& c = m_triangleRuntime[triIndex].centroid;
			return DistanceXZ(c, goalProj);
		};

	std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeGreater> openSet;

	gScore[startTri] = 0.0f;
	fScore[startTri] = Heuristic(startTri);
	openSet.push(OpenNode{ startTri, fScore[startTri] });

	while ( !openSet.empty() )
	{
		const OpenNode currentNode = openSet.top();
		openSet.pop();

		const int current = currentNode.triangleIndex;
		if ( current < 0 || current >= triCount )
			continue;

		if ( closed[current] )
			continue;

		if ( current == goalTri )
			break;

		closed[current] = 1;

		const NAVMESH_TRIANGLE& tri = m_triangles[current];
		const XMFLOAT3& currCenter = m_triangleRuntime[current].centroid;

		for ( int edgeIndex = 0; edgeIndex < 3; ++edgeIndex )
		{
			const int neighbor = GetTriangleNeighbor(tri, edgeIndex);
			if ( neighbor < 0 || neighbor >= triCount )
				continue;

			if ( closed[neighbor] )
				continue;

			const XMFLOAT3& nextCenter = m_triangleRuntime[neighbor].centroid;
			const float stepDist = DistanceXZ(currCenter, nextCenter);
			const float areaCost = GetAreaCost(m_triangles[neighbor].area);

			const float tentativeG = gScore[current] + ( stepDist * areaCost );
			if ( tentativeG >= gScore[neighbor] )
				continue;

			cameFrom[neighbor] = current;
			gScore[neighbor] = tentativeG;
			fScore[neighbor] = tentativeG + Heuristic(neighbor);

			openSet.push(OpenNode{ neighbor, fScore[neighbor] });
		}
	}

	if ( cameFrom[goalTri] == -1 )
		return false;

	std::vector<int> reversed;
	for ( int node = goalTri; node != -1; node = cameFrom[node] )
	{
		reversed.push_back(node);
		if ( node == startTri )
			break;
	}

	if ( reversed.empty() || reversed.back() != startTri )
		return false;

	outTrianglePath.assign(reversed.rbegin(), reversed.rend());
	return !outTrianglePath.empty();
}

bool CNavMesh::FindStraightPath(
	const XMFLOAT3& startPos,
	const XMFLOAT3& goalPos,
	std::vector<XMFLOAT3>& outStraightPath) const
{
	outStraightPath.clear();
	if ( !m_bLoaded ) return false;

	XMFLOAT3 startProj{};
	XMFLOAT3 goalProj{};
	int startTri = -1;
	int goalTri = -1;

	if ( !SamplePosition(startPos, startProj, &startTri) )
		return false;

	if ( !SamplePosition(goalPos, goalProj, &goalTri) )
		return false;

	std::vector<int> trianglePath;
	if ( !FindTrianglePath(startProj, goalProj, trianglePath) )
		return false;

	if ( trianglePath.empty() )
		return false;

	if ( trianglePath.size() == 1 )
	{
		AppendPointIfNotDuplicate(outStraightPath, startProj);
		AppendPointIfNotDuplicate(outStraightPath, goalProj);
		return true;
	}

	struct Portal
	{
		XMFLOAT3 left = XMFLOAT3(0.0f, 0.0f, 0.0f);
		XMFLOAT3 right = XMFLOAT3(0.0f, 0.0f, 0.0f);
	};

	std::vector<Portal> portals;
	portals.reserve(trianglePath.size() + 1);

	portals.push_back(Portal{ startProj, startProj });

	for ( size_t i = 0; i + 1 < trianglePath.size(); ++i )
	{
		XMFLOAT3 a{}, b{};
		if ( !GetTrianglePortal(trianglePath[i], trianglePath[i + 1], a, b) )
			return false;

		portals.push_back(Portal{ a, b });
	}

	portals.push_back(Portal{ goalProj, goalProj });

	XMFLOAT3 portalApex = portals[0].left;
	XMFLOAT3 portalLeft = portals[0].left;
	XMFLOAT3 portalRight = portals[0].right;

	size_t apexIndex = 0;
	size_t leftIndex = 0;
	size_t rightIndex = 0;

	AppendPointIfNotDuplicate(outStraightPath, portalApex);

	for ( size_t i = 1; i < portals.size(); ++i )
	{
		XMFLOAT3 left = portals[i].left;
		XMFLOAT3 right = portals[i].right;

		// 현재 apex 기준으로 portal 방향 정규화
		if ( Cross2D_XZ(portalApex, left, right) < 0.0f )
			std::swap(left, right);

		// right 갱신
		if ( NearlyEqualXZ(portalApex, portalRight) || Cross2D_XZ(portalApex, portalRight, right) <= 0.0f )
		{
			if ( NearlyEqualXZ(portalApex, portalRight) || Cross2D_XZ(portalApex, portalLeft, right) > 0.0f )
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

		// left 갱신
		if ( NearlyEqualXZ(portalApex, portalLeft) || Cross2D_XZ(portalApex, portalLeft, left) >= 0.0f )
		{
			if ( NearlyEqualXZ(portalApex, portalLeft) || Cross2D_XZ(portalApex, portalRight, left) < 0.0f )
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

	// y 보정: 각 포인트를 다시 navmesh 위로 샘플링
	for ( size_t i = 0; i < outStraightPath.size(); ++i )
	{
		XMFLOAT3 corrected{};
		if ( SamplePosition(outStraightPath[i], corrected) )
			outStraightPath[i] = corrected;
	}

	return !outStraightPath.empty();
}

bool CNavMesh::FindPath(
	const XMFLOAT3& startPos,
	const XMFLOAT3& goalPos,
	std::vector<int>& outTrianglePath,
	std::vector<XMFLOAT3>& outStraightPath) const
{
	outTrianglePath.clear();
	outStraightPath.clear();

	if ( !FindTrianglePath(startPos, goalPos, outTrianglePath) )
		return false;

	if ( !FindStraightPath(startPos, goalPos, outStraightPath) )
	{
		outTrianglePath.clear();
		return false;
	}

	return true;
}