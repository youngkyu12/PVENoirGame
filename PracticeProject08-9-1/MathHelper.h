#pragma once

#include <algorithm>
#include <cmath>

#include <DirectXMath.h>
#include <DirectXCollision.h>

#include "BoundingCapsule.h"
using namespace DirectX;
using namespace std;

#define EPSILON							1.0e-10f

inline bool IsZero(float fValue) { return( ( fabsf(fValue) < EPSILON ) ); }
inline bool IsEqual(float fA, float fB) { return( ::IsZero(fA - fB) ); }
inline float InverseSqrt(float fValue) { return 1.0f / sqrtf(fValue); }
inline void Swap(float* pfS, float* pfT) { float fTemp = *pfS; *pfS = *pfT; *pfT = fTemp; }
// Returns random float in [0, 1).
inline float RandF()
{
	return (float)(rand()) / (float)RAND_MAX;
}

// Returns random float in [a, b).
inline float RandF(float a, float b)
{
	return a + RandF()*(b-a);
}

inline int Rand(int a, int b)
{
	return a + rand() % ((b - a) + 1);
}

namespace Vector3
{
	inline bool IsZero(const XMFLOAT3& xmf3Vector)
	{
		if ( ::IsZero(xmf3Vector.x) && ::IsZero(xmf3Vector.y) && ::IsZero(xmf3Vector.z) )return( true );
		return( false );
	}

	inline XMFLOAT3 XMVectorToFloat3(const XMVECTOR& xmvVector)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, xmvVector);
		return( xmf3Result );
	}

	inline XMFLOAT3 ScalarProduct(const XMFLOAT3& xmf3Vector, float fScalar, bool bNormalize = true)
	{
		XMFLOAT3 xmf3Result;
		if ( bNormalize )
			XMStoreFloat3(&xmf3Result, XMVector3Normalize(XMLoadFloat3(&xmf3Vector)) * fScalar);
		else
			XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector) * fScalar);
		return( xmf3Result );
	}

	inline XMFLOAT3 Add(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector1) + XMLoadFloat3(&xmf3Vector2));
		return( xmf3Result );
	}

	inline XMFLOAT3 Add(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2, float fScalar)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector1) + ( XMLoadFloat3(&xmf3Vector2) * fScalar ));
		return( xmf3Result );
	}

	inline XMFLOAT3 Subtract(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector1) - XMLoadFloat3(&xmf3Vector2));
		return( xmf3Result );
	}

	inline float DotProduct(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMVector3Dot(XMLoadFloat3(&xmf3Vector1), XMLoadFloat3(&xmf3Vector2)));
		return( xmf3Result.x );
	}

	inline XMFLOAT3 CrossProduct(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2, bool bNormalize = true)
	{
		XMFLOAT3 xmf3Result;
		if ( bNormalize )
			XMStoreFloat3(&xmf3Result, XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&xmf3Vector1), XMLoadFloat3(&xmf3Vector2))));
		else
			XMStoreFloat3(&xmf3Result, XMVector3Cross(XMLoadFloat3(&xmf3Vector1), XMLoadFloat3(&xmf3Vector2)));
		return( xmf3Result );
	}

	inline XMFLOAT3 Normalize(const XMFLOAT3& xmf3Vector)
	{
		XMFLOAT3 m_xmf3Normal;
		XMStoreFloat3(&m_xmf3Normal, XMVector3Normalize(XMLoadFloat3(&xmf3Vector)));
		return( m_xmf3Normal );
	}

	inline float Length(const XMFLOAT3& xmf3Vector)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMVector3Length(XMLoadFloat3(&xmf3Vector)));
		return( xmf3Result.x );
	}

	inline float Angle(const XMVECTOR& xmvVector1, const XMVECTOR& xmvVector2)
	{
		XMVECTOR xmvAngle = XMVector3AngleBetweenNormals(xmvVector1, xmvVector2);
		return( XMConvertToDegrees(acosf(XMVectorGetX(xmvAngle))) );
	}

	inline float Angle(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
	{
		XMVECTOR f3Vector1 = XMLoadFloat3(&xmf3Vector1);
		XMVECTOR f3Vector2 = XMLoadFloat3(&xmf3Vector2);
		return( Angle(f3Vector1, f3Vector2) );
	}

	inline XMFLOAT3 TransformNormal(const XMFLOAT3& xmf3Vector, const XMMATRIX& xmmtxTransform)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMVector3TransformNormal(XMLoadFloat3(&xmf3Vector), xmmtxTransform));
		return( xmf3Result );
	}

	inline XMFLOAT3 TransformCoord(const XMFLOAT3& xmf3Vector, const XMMATRIX& xmmtxTransform)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMVector3TransformCoord(XMLoadFloat3(&xmf3Vector), xmmtxTransform));
		return( xmf3Result );
	}

	inline XMFLOAT3 TransformCoord(const XMFLOAT3& xmf3Vector, const XMFLOAT4X4& xmmtx4x4Matrix)
	{
		XMMATRIX mtxTransform = XMLoadFloat4x4(&xmmtx4x4Matrix);
		return( TransformCoord(xmf3Vector, mtxTransform) );
	}
}

namespace Vector4
{
	inline XMFLOAT4 Add(const XMFLOAT4& xmf4Vector1, const XMFLOAT4& xmf4Vector2)
	{
		XMFLOAT4 xmf4Result;
		XMStoreFloat4(&xmf4Result, XMLoadFloat4(&xmf4Vector1) + XMLoadFloat4(&xmf4Vector2));
		return( xmf4Result );
	}

	inline XMFLOAT4 Multiply(const XMFLOAT4& xmf4Vector1, const XMFLOAT4& xmf4Vector2)
	{
		XMFLOAT4 xmf4Result;
		XMStoreFloat4(&xmf4Result, XMLoadFloat4(&xmf4Vector1) * XMLoadFloat4(&xmf4Vector2));
		return( xmf4Result );
	}

	inline XMFLOAT4 Multiply(float fScalar, const XMFLOAT4& xmf4Vector)
	{
		XMFLOAT4 xmf4Result;
		XMStoreFloat4(&xmf4Result, fScalar * XMLoadFloat4(&xmf4Vector));
		return( xmf4Result );
	}
}

namespace Matrix4x4
{
	inline XMFLOAT4X4 Identity()
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixIdentity());
		return( xmmtx4x4Result );
	}

	inline XMFLOAT4X4 Multiply(const XMFLOAT4X4& xmmtx4x4Matrix1, const XMFLOAT4X4& xmmtx4x4Matrix2)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMLoadFloat4x4(&xmmtx4x4Matrix1) * XMLoadFloat4x4(&xmmtx4x4Matrix2));
		return( xmmtx4x4Result );
	}

	inline XMFLOAT4X4 Multiply(const XMFLOAT4X4& xmmtx4x4Matrix1, const XMMATRIX& xmmtxMatrix2)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMLoadFloat4x4(&xmmtx4x4Matrix1) * xmmtxMatrix2);
		return( xmmtx4x4Result );
	}

	inline XMFLOAT4X4 Multiply(const XMMATRIX& xmmtxMatrix1, const XMFLOAT4X4& xmmtx4x4Matrix2)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, xmmtxMatrix1 * XMLoadFloat4x4(&xmmtx4x4Matrix2));
		return( xmmtx4x4Result );
	}

	inline XMFLOAT4X4 Inverse(const XMFLOAT4X4& xmmtx4x4Matrix)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixInverse(NULL, XMLoadFloat4x4(&xmmtx4x4Matrix)));
		return( xmmtx4x4Result );
	}

	inline XMFLOAT4X4 Transpose(const XMFLOAT4X4& xmmtx4x4Matrix)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixTranspose(XMLoadFloat4x4(&xmmtx4x4Matrix)));
		return( xmmtx4x4Result );
	}

	inline XMFLOAT4X4 PerspectiveFovLH(float FovAngleY, float AspectRatio, float NearZ, float FarZ)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixPerspectiveFovLH(FovAngleY, AspectRatio, NearZ, FarZ));
		return( xmmtx4x4Result );
	}

	inline XMFLOAT4X4 LookAtLH(const XMFLOAT3& xmf3EyePosition, const XMFLOAT3& xmf3LookAtPosition, const XMFLOAT3& xmf3UpDirection)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixLookAtLH(XMLoadFloat3(&xmf3EyePosition), XMLoadFloat3(&xmf3LookAtPosition), XMLoadFloat3(&xmf3UpDirection)));
		return( xmmtx4x4Result );
	}
}

namespace Triangle
{
	inline bool Intersect(const XMFLOAT3& xmf3RayPosition, const XMFLOAT3& xmf3RayDirection, const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2, float& fHitDistance)
	{
		return( TriangleTests::Intersects(XMLoadFloat3(&xmf3RayPosition), XMLoadFloat3(&xmf3RayDirection), XMLoadFloat3(&v0), XMLoadFloat3(&v1), XMLoadFloat3(&v2), fHitDistance) );
	}
}

namespace Plane
{
	inline XMFLOAT4 Normalize(XMFLOAT4& xmf4Plane)
	{
		XMFLOAT4 xmf4Result;
		XMStoreFloat4(&xmf4Result, XMPlaneNormalize(XMLoadFloat4(&xmf4Plane)));
		return( xmf4Result );
	}
}

namespace Collide
{
	// AB선분과 점 사이의 최근접점 Vector 반환
	inline XMVECTOR ClosestPointOnSegment(FXMVECTOR A, FXMVECTOR B, FXMVECTOR P)
	{
		XMVECTOR AB = B - A;	// A->B 방향과 길이를 가진 벡터
		XMVECTOR AP = P - A;	// A->P 방향과 길이를 가진 벡터

		float abLenSq = XMVectorGetX(XMVector3Dot(AB, AB));	 // AB를 AB로 내적한 것은 A->B 길이의 제곱과 같은 의미
		float t = 0.0f;

		if ( abLenSq > 0.0f )
		{
			t = XMVectorGetX(XMVector3Dot(AP, AB)) / abLenSq;
			t = std::clamp(t, 0.0f, 1.0f);
		}

		return XMVectorAdd(A, XMVectorScale(AB, t));
	}

	// AB선분에서 점까지의 최단거리 반환
	inline float distPointToSegment(FXMVECTOR A, FXMVECTOR B, FXMVECTOR P)
	{
		XMVECTOR AB = B - A;	// A->B 방향과 길이를 가진 벡터
		XMVECTOR AP = P - A;	// A->P 방향과 길이를 가진 벡터

		float abLenSq = XMVectorGetX(XMVector3Dot(AB, AB));	 // AB를 AB로 내적한 것은 선분AB 길이의 제곱
		float t = 0.0f;

		if ( abLenSq > 0.0f )
		{
			t = XMVectorGetX(XMVector3Dot(AP, AB)) / abLenSq;
			t = std::clamp(t, 0.0f, 1.0f);
		}
		XMVECTOR closest = XMVectorAdd(A, XMVectorScale(AB, t));

		return XMVectorGetX(XMVector3LengthSq(closest - P));
	}

	// AB선분 V0V1선분 사이의 최단 거리 제곱 반환
	inline float distSegmentToSegment(FXMVECTOR A, FXMVECTOR B, FXMVECTOR V0, FXMVECTOR V1)
	{
		XMVECTOR AB = B - A;
		XMVECTOR V0V1 = V1 - V0;
		XMVECTOR AV0 = V0 - A;
		XMVECTOR P;
		XMVECTOR Q;

		float ABLensq = XMVectorGetX(XMVector3Dot(AB, AB)); // |AB|^2 
		float ABProjV0V1 = XMVectorGetX(XMVector3Dot(V0V1, AB)); //V0V1 . AB = |V0V1||AB|cos(theta)
		float V0V1Lensq = XMVectorGetX(XMVector3Dot(V0V1, V0V1)); // |V0V1|^2 
		float ABProjAV0 = XMVectorGetX(XMVector3Dot(AV0, AB)); // AV0 . AB = |AV0||AB|cos(theta)
		float V0V1ProjAV0 = XMVectorGetX(XMVector3Dot(AV0, V0V1)); // AV0 . V0V1 = |AV0||V0V1|cos(theta)

		float denom = ABLensq * V0V1Lensq - ABProjV0V1 * ABProjV0V1;
		// |AB|^2 * |V0V1|^2 - |V0V1|^2 * |AB|^2 * cos^2(theta)
		// |AB|^2 |V0V1|^2 (1 - cos^2(theta))
		// |AB|^2 |V0V1|^2 sin^2(theta) = (|AB||V0V1|sin(theta))^2
		float s = 0.0f;
		float t = 0.0f;

		if ( ABLensq <= EPSILON && V0V1Lensq <= EPSILON ) // AB = 점, V0V1 = 점
		{
			P = A;
			Q = V0;
			return XMVectorGetX(XMVector3LengthSq(P - Q));	// 따라서 그 점과 점까지의 거리 반환
		}

		if ( ABLensq <= EPSILON ) // AB = 점 V0V1 = 선분
		{
			s = 0.0f;
			t = std::clamp(-V0V1ProjAV0 / V0V1Lensq, 0.0f, 1.0f);
		}
		else if ( V0V1Lensq <= EPSILON ) // V0V1 = 점 AB = 선분
		{
			t = 0.0f;
			s = std::clamp(ABProjAV0 / ABLensq, 0.0f, 1.0f);
		}
		else
		{
			if ( denom > EPSILON )
				s = std::clamp(( ABProjV0V1 * V0V1ProjAV0 - V0V1Lensq * ABProjAV0 ) / denom, 0.0f, 1.0f);
			// |V0V1||AB|cos(theta) * |AV0||V0V1|cos(alpha) - |V0V1|^2 * |AV0||AB|cos(beta)
			// |V0V1|^2 |AB| |AV0| cos(theta) * cos(alpha) - |V0V1|^2 |AB| |AV0| cos(beta)
			// |V0V1|^2 |AB| |AV0| (cos(theta) * cos(alpha) - cos(beta))
			// |V0V1|^2 |AB| |AV0|cos(theta) * cos(alpha) - cos(beta) / |V0V1|^2 |AB|^2  (1 - cos^2(theta))
			// |AV0| cos(theta) * cos(alpha) - cos(beta) / |AB|(1 - cos^2(theta))
			// cos(beta): AV0 가 AB 방향으로 얼마나 놓여 있나
			// cos(alpha): AV0 가 V0V1 방향으로 얼마나 놓여 있나
			// cos(theta): 두 선분 방향 AB, V0V1 가 얼마나 비슷한가(평행한가?)
			// 1 - cos^2(theta): 두 방향이 얼마나 평행하지 않은가
			// AB 위 최근접점
			else
				s = 0.0f;

			t = ( ABProjV0V1 * s + V0V1ProjAV0 ) / V0V1Lensq;
			// |AV0| (cos(alpha) - cos(theta)cos(beta))	/ (| V0V1 | (1 - cos ^ 2(theta)))
			// V0V1 위 최근접점
			if ( t < 0.0f ) // V0가 최근접점
			{
				t = 0.0f;
				s = std::clamp(ABProjAV0 / ABLensq, 0.0f, 1.0f);
			}
			else if ( t > 1.0f )	// V1이 최근접점
			{
				t = 1.0f;
				s = std::clamp(( ABProjV0V1 + ABProjAV0 ) / ABLensq, 0.0f, 1.0f);
			}
		}

		P = A + AB * s;
		Q = V0 + V0V1 * t;

		return XMVectorGetX(XMVector3LengthSq(P - Q));
	}

	// 삼각형 면과 점이 접하는지(삼각형 변과 꼭짓점 제외)
	inline bool PointInTriangle(FXMVECTOR P, FXMVECTOR V0, FXMVECTOR V1, FXMVECTOR V2)
	{
		XMVECTOR v0 = V2 - V0;
		XMVECTOR v1 = V1 - V0;
		XMVECTOR v2 = P - V0;

		float dot00 = XMVectorGetX(XMVector3Dot(v0, v0));
		float dot01 = XMVectorGetX(XMVector3Dot(v0, v1));
		float dot02 = XMVectorGetX(XMVector3Dot(v0, v2));
		float dot11 = XMVectorGetX(XMVector3Dot(v1, v1));
		float dot12 = XMVectorGetX(XMVector3Dot(v1, v2));

		float denom = dot00 * dot11 - dot01 * dot01;
		if ( fabsf(denom) <= EPSILON )
			return false;

		float invDenom = 1.0f / denom;
		float u = ( dot11 * dot02 - dot01 * dot12 ) * invDenom;
		float v = ( dot00 * dot12 - dot01 * dot02 ) * invDenom;

		return ( u >= 0.0f ) && ( v >= 0.0f ) && ( u + v <= 1.0f );
	}

	// 삼각형과 선분 사이의 최단거리
	inline float distSegmentToTriangle(FXMVECTOR A, FXMVECTOR B, FXMVECTOR V0, FXMVECTOR V1, FXMVECTOR V2)
	{
		XMVECTOR AB = B - A;
		XMVECTOR E0 = V1 - V0;
		XMVECTOR E1 = V2 - V0;
		XMVECTOR N = XMVector3Cross(E0, E1);	// 삼각형 평면 법선 구하기
		// |E0||E1|sin(theta)
		float nLenSq = XMVectorGetX(XMVector3Dot(N, N));

		if ( nLenSq <= EPSILON )
			return FLT_MAX; // degenerate triangle

		N = XMVector3Normalize(N);
		float f0 = XMVectorGetX(XMVector3Dot(A - V0, N));	// A가 평면에서 얼마나 멀어져 있는지 구하기
		// |A - V0|cos(alpha)
		float f1 = XMVectorGetX(XMVector3Dot(B - V0, N));	// B가 평면에서 얼마나 멀어져 있는지 구하기
		// |B - V0|cos(beta)

		float t = 0.0f;
		float denom = f0 - f1;
		if ( fabsf(denom) > EPSILON )
			t = std::clamp(f0 / denom, 0.0f, 1.0f);
		else
			t = ( fabsf(f0) < fabsf(f1) ) ? 0.0f : 1.0f;

		XMVECTOR closest = A + AB * t;
		float signedDist = XMVectorGetX(XMVector3Dot(closest - V0, N));	// 캡슐 선분 위 최근접점이 평면에서 얼마나 멀어져 있는지 구하기
		// |closest - V0|cos(gamma)
		XMVECTOR Q = closest - N * signedDist;	// 캡슐 선분 위 최근접점을 삼각형 평면에 수직으로 내린 점
		// closest - N * |closest - V0|cos(gamma)

		if ( !PointInTriangle(Q, V0, V1, V2) )
			return FLT_MAX; // 투영점이 면 내부가 아님

		return signedDist * signedDist; // 선분-삼각형 면 거리 제곱
	}

	inline XMVECTOR ClosestPointOnAABB(FXMVECTOR p, FXMVECTOR extents)
	{
		XMFLOAT3 pf, ef;
		XMStoreFloat3(&pf, p);
		XMStoreFloat3(&ef, extents);

		return XMVectorSet(
			std::clamp(pf.x, -ef.x, ef.x),
			std::clamp(pf.y, -ef.y, ef.y),
			std::clamp(pf.z, -ef.z, ef.z),
			0.0f);
	}

	inline float DistPointToAABBSq(FXMVECTOR p, FXMVECTOR extents)
	{
		XMVECTOR P = p;
		XMVECTOR Q = ClosestPointOnAABB(p, extents);
		return XMVectorGetX(XMVector3LengthSq(P - Q));
	}

	// 선분-선분 최단거리 제곱
	inline float DistSegmentToSegmentSq(FXMVECTOR p1, FXMVECTOR q1, FXMVECTOR p2, FXMVECTOR q2)
	{
		const float EPS = 1e-6f;

		XMVECTOR d1 = q1 - p1; // S1 방향
		XMVECTOR d2 = q2 - p2; // S2 방향
		XMVECTOR r = p1 - p2;

		float a = XMVectorGetX(XMVector3Dot(d1, d1));
		float e = XMVectorGetX(XMVector3Dot(d2, d2));
		float f = XMVectorGetX(XMVector3Dot(d2, r));

		float s, t;

		if ( a <= EPS && e <= EPS )
		{
			return XMVectorGetX(XMVector3LengthSq(p1 - p2));
		}

		if ( a <= EPS )
		{
			s = 0.0f;
			t = std::clamp(f / e, 0.0f, 1.0f);
		}
		else
		{
			float c = XMVectorGetX(XMVector3Dot(d1, r));

			if ( e <= EPS )
			{
				t = 0.0f;
				s = std::clamp(-c / a, 0.0f, 1.0f);
			}
			else
			{
				float b = XMVectorGetX(XMVector3Dot(d1, d2));
				float denom = a * e - b * b;

				if ( denom != 0.0f )
					s = std::clamp(( b * f - c * e ) / denom, 0.0f, 1.0f);
				else
					s = 0.0f;

				t = ( b * s + f ) / e;

				if ( t < 0.0f )
				{
					t = 0.0f;
					s = std::clamp(-c / a, 0.0f, 1.0f);
				}
				else if ( t > 1.0f )
				{
					t = 1.0f;
					s = std::clamp(( b - c ) / a, 0.0f, 1.0f);
				}
			}
		}

		XMVECTOR c1 = p1 + d1 * s;
		XMVECTOR c2 = p2 + d2 * t;
		return XMVectorGetX(XMVector3LengthSq(c1 - c2));
	}

	// slab 방식으로 선분이 AABB와 교차하는지 검사
	inline bool IntersectSegmentAABB(FXMVECTOR A, FXMVECTOR B, FXMVECTOR Extents)
	{
		float ax = XMVectorGetX(A);
		float ay = XMVectorGetY(A);
		float az = XMVectorGetZ(A);

		float bx = XMVectorGetX(B);
		float by = XMVectorGetY(B);
		float bz = XMVectorGetZ(B);

		float ex = XMVectorGetX(Extents);
		float ey = XMVectorGetY(Extents);
		float ez = XMVectorGetZ(Extents);

		float d[3] = { bx - ax, by - ay, bz - az };
		float p0[3] = { ax, ay, az };
		float minEx[3] = { -ex, -ey, -ez };
		float maxEx[3] = { ex, ey, ez };

		float tmin = 0.0f;
		float tmax = 1.0f;

		for ( int i = 0; i < 3; ++i ) {
			if ( fabsf(d[i]) <= EPSILON ) {	// 선분이 점이 될만큼 작은 경우
				if ( p0[i] < minEx[i] || p0[i] > maxEx[i] )
					return false;
			}
			else {
				float ood = 1.0f / d[i];
				float t1 = ( minEx[i] - p0[i] ) * ood;
				// P(t) = A + t(B - A), 
				// minEx[i] = p0[i] + t1 * d[i]
				// t1 =(minEx[i] - p0[i]) / d[i] 
				// 선분 P(t)=A+t(B-A)가 축 i의 최소 경계면(minEx[i])과 만나는 t1
				// 만약 AB 선분 위에 minEx[i]가 존재한다면 0.0~1.0 사이 값이 나온다.
				// 만약 AB 선분 위에 minEx[i]가 존재하지 않는다면 0.0보다 작거나 1.0보다 큰 값이 나온다.

				float t2 = ( maxEx[i] - p0[i] ) * ood;
				// P(t) = A + t(B - A), 
				// maxEx[i] = p0[i] + t2 * d[i]
				// t2 =(maxEx[i] - p0[i]) / d[i] 
				// 선분 P(t)=A+t(B-A)가 축 i의 최대 경계면(maxEx[i])과 만나는 t2
				// 만약 AB 선분 위에 maxEx[i]가 존재한다면 0.0~1.0 사이 값이 나온다.
				// 만약 AB 선분 위에 maxEx[i]가 존재하지 않는다면 0.0보다 작거나 1.0보다 큰 값이 나온다.

				// [t1, t2]은 각 축의 slab 공간([-ex, ex]) 사이를 만족하는 P(t)점의 허용 범위이다.
				// 다른 말로 t1은 각 축의 slab 공간 진입 시점(-ex) 중 P(t)점의 허용 범위이다.
				// t2는 각 축의 slab 공간 이탈 시점(ex) 중 P(t)점의 허용 범위이다.

				if ( t1 > t2 )
					std::swap(t1, t2); // 현재 축에서 slab 진입/이탈 시점 순서를 정리

				tmin = max(tmin, t1);	// 현재까지의 유효 구간 시작과, 이번 축의 진입 시점 중 더 큰 값
				tmax = min(tmax, t2);	// 현재까지의 유효 구간 끝과, 이번 축의 이탈 시점 중 더 작은 값

				if ( tmin > tmax )
					return false;
			}
		}

		return true;
	}

	// AABB 경계면과 점 사이의 거리 제곱
	inline float DistanceSqPointToAABB(FXMVECTOR P, FXMVECTOR Extents)
	{
		float px = XMVectorGetX(P);
		float py = XMVectorGetY(P);
		float pz = XMVectorGetZ(P);

		float ex = XMVectorGetX(Extents);
		float ey = XMVectorGetY(Extents);
		float ez = XMVectorGetZ(Extents);

		float dx = 0.0f;
		if ( px < -ex )      dx = -ex - px;
		else if ( px > ex )  dx = px - ex;

		float dy = 0.0f;
		if ( py < -ey )      dy = -ey - py;
		else if ( py > ey )  dy = py - ey;

		float dz = 0.0f;
		if ( pz < -ez )      dz = -ez - pz;
		else if ( pz > ez )  dz = pz - ez;

		return dx * dx + dy * dy + dz * dz;
	}

	inline float distSegmentToAABB(FXMVECTOR A, FXMVECTOR B, FXMVECTOR Extents)
	{
		// 1) 로컬공간에서의 캡슐 축 양끝점이 박스를 통과하면 거리 0 == 충돌
		if ( IntersectSegmentAABB(A, B, Extents) )
			return 0.0f;

		float best = DistanceSqPointToAABB(A, Extents);
		best = min(best, DistanceSqPointToAABB(B, Extents));

		XMVECTOR D = XMVectorSubtract(B, A);

		float ax = XMVectorGetX(A);
		float ay = XMVectorGetY(A);
		float az = XMVectorGetZ(A);

		float dx = XMVectorGetX(D);
		float dy = XMVectorGetY(D);
		float dz = XMVectorGetZ(D);

		float ex = XMVectorGetX(Extents);
		float ey = XMVectorGetY(Extents);
		float ez = XMVectorGetZ(Extents);

		auto testT = [ & ] (float t)
			{
				if ( t < 0.0f || t > 1.0f )
					return;

				XMVECTOR P = XMVectorAdd(A, XMVectorScale(D, t));
				best = min(best, DistanceSqPointToAABB(P, Extents));
			};

		if ( std::fabs(dx) > EPSILON )
		{
			testT(( -ex - ax ) / dx);
			testT(( ex - ax ) / dx);
		}

		if ( std::fabs(dy) > EPSILON )
		{
			testT(( -ey - ay ) / dy);
			testT(( ey - ay ) / dy);
		}

		if ( std::fabs(dz) > EPSILON )
		{
			testT(( -ez - az ) / dz);
			testT(( ez - az ) / dz);
		}

		return best;
	}

	inline bool Intersects(const BoundingFrustum& fr, const BoundingCapsule& ca) noexcept
	{
		return ca.Intersects(fr);
	}

}

enum class EColliderType : uint8_t
{
	None = 0,
	AABB,
	OOBB,
	BSphere,
	BCapsule
};

