#include "stdafx.h"
#include "BoundingCapsule.h"
#include <sstream>

void BoundingCapsule::Transform(BoundingCapsule& Out, FXMMATRIX M) const noexcept
{
    XMVECTOR vP0 = XMLoadFloat3(&p0);
    XMVECTOR vP1 = XMLoadFloat3(&p1);

    // 점 변환
    XMVECTOR P0 = XMVector3TransformCoord(vP0, M);
    XMVECTOR P1 = XMVector3TransformCoord(vP1, M);

    // 행렬의 축 스케일 중 최대값 사용
    XMVECTOR dX = XMVector3Dot(M.r[0], M.r[0]);
    XMVECTOR dY = XMVector3Dot(M.r[1], M.r[1]);
    XMVECTOR dZ = XMVector3Dot(M.r[2], M.r[2]);
    XMVECTOR d = XMVectorMax(dX, XMVectorMax(dY, dZ));

    float scale = sqrtf(XMVectorGetX(d));

    XMStoreFloat3(&Out.p0, P0);
    XMStoreFloat3(&Out.p1, P1);

    // center는 변환 결과 축의 중점으로 다시 계산
    XMVECTOR C = 0.5f * (P0 + P1);
    XMStoreFloat3(&Out.Center, C);

    Out.Radius = Radius * scale;

    // 전체 높이 = 축 길이 + 양 끝 반구 반지름 2개
    float axisLen = XMVectorGetX(XMVector3Length(P1 - P0));
    Out.Height = axisLen + Out.Radius * 2.0f;

    // Direction enum을 유지해야 한다면 원본 값만 복사
    Out.Direction = Direction;
}



void BoundingCapsule::Transform(BoundingCapsule& Out, float Scale, FXMVECTOR Rotation, FXMVECTOR Translation) const noexcept
{
}

ContainmentType BoundingCapsule::Contains(FXMVECTOR Point) const noexcept
{
    XMVECTOR A = XMLoadFloat3(&p0);
    XMVECTOR B = XMLoadFloat3(&p1);
    XMVECTOR vRadius = XMVectorReplicatePtr(&Radius);

    XMVECTOR closest = Vector3::ClosestPointOnSegment(A, B, Point);
    XMVECTOR DistanceSquared = XMVector3LengthSq(Point - closest);
    XMVECTOR RadiusSquared = XMVectorMultiply(vRadius, vRadius);

    return XMVector3LessOrEqual(DistanceSquared, RadiusSquared) ? CONTAINS : DISJOINT;
}

ContainmentType BoundingCapsule::Contains(FXMVECTOR V0, FXMVECTOR V1, FXMVECTOR V2) const noexcept
{
    if (!Intersects(V0, V1, V2))
        return DISJOINT;

    XMVECTOR A = XMLoadFloat3(&p0);
    XMVECTOR B = XMLoadFloat3(&p1);

    float d0 = Vector3::distPointToSegment(A, B, V0);
    float d1 = Vector3::distPointToSegment(A, B, V1);
    float d2 = Vector3::distPointToSegment(A, B, V2);

    XMVECTOR distSq = XMVectorSet(d0, d1, d2, 0.0f);
    XMVECTOR radiusSq = XMVectorReplicate(Radius * Radius);

    XMVECTOR inside = XMVectorLessOrEqual(distSq, radiusSq);

    return XMVector3EqualInt(inside, XMVectorTrueInt()) ? CONTAINS : INTERSECTS;
}

bool BoundingCapsule::Intersects(const BoundingSphere& sh) const noexcept
{
	XMVECTOR A = XMLoadFloat3(&p0);
	XMVECTOR B = XMLoadFloat3(&p1);
	XMVECTOR C = XMLoadFloat3(&sh.Center);

	const float distSq = Collide::distPointToSegment(A, B, C);
	const float r = Radius + sh.Radius;

	return distSq <= ( r * r );
}

bool BoundingCapsule::Intersects(const BoundingBox& box) const noexcept
{
	XMVECTOR A = XMLoadFloat3(&p0);
	XMVECTOR B = XMLoadFloat3(&p1);

	XMVECTOR center = XMLoadFloat3(&box.Center);
	XMVECTOR extents = XMLoadFloat3(&box.Extents);

	// AABB 중심 기준 로컬 공간으로 선분 이동
	XMVECTOR ALocal = XMVectorSubtract(A, center);
	XMVECTOR BLocal = XMVectorSubtract(B, center);

	const float distSq = Collide::distSegmentToAABB(ALocal, BLocal, extents);
	return distSq <= ( Radius * Radius );
}

bool BoundingCapsule::Intersects(const BoundingOrientedBox& box) const noexcept
{
    XMVECTOR A = XMLoadFloat3(&p0);
    XMVECTOR B = XMLoadFloat3(&p1);

    XMVECTOR center = XMLoadFloat3(&box.Center);
    XMVECTOR extents = XMLoadFloat3(&box.Extents);
    XMVECTOR q = XMLoadFloat4(&box.Orientation);

    XMVECTOR ALocal = XMVector3InverseRotate(XMVectorSubtract(A, center), q);
    XMVECTOR BLocal = XMVector3InverseRotate(XMVectorSubtract(B, center), q);

    float distSq = Vector3::distSegmentToAABB(ALocal, BLocal, extents);
    return distSq <= Radius * Radius;
}

bool BoundingCapsule::Intersects(const BoundingFrustum& fr) const noexcept
{
	XMVECTOR A = XMLoadFloat3(&p0);
	XMVECTOR B = XMLoadFloat3(&p1);

	XMVECTOR nearPlane, farPlane, rightPlane, leftPlane, topPlane, bottomPlane;
	fr.GetPlanes(&nearPlane, &farPlane, &rightPlane, &leftPlane, &topPlane, &bottomPlane);

	XMVECTOR planes[6] =
	{
		nearPlane, farPlane, rightPlane,
		leftPlane, topPlane, bottomPlane
	};

	for ( int i = 0; i < 6; ++i )
	{
		float da = XMVectorGetX(XMPlaneDotCoord(planes[i], A));
		float db = XMVectorGetX(XMPlaneDotCoord(planes[i], B));

		float dmax = max(da, db);

		
		// 두 끝점이 모두 반지름 보정 후에도 평면 바깥이면 캡슐은 프러스텀과 비충돌
		if ( fA > 0.0f && fB > 0.0f )
			return false;
		
		// 선분 두 끝점 이외에 안쪽 점들 확인

		// |N||D|cos(theta) = dot(N, D)	// 평면 벡터를 정규화했기 때문에 법선벡터 1로 수식에서 N으로 표기
		// 선분 방향이 현재 평면의 법선 방향으로 얼마나 향하는지 알 수 있다.
		// 이 값이 0에 가까우면 선분은 평면과 거의 평행하다.
		XMVECTOR N = XMVectorSetW(P, 0.0f);
		float denom = XMVectorGetX(XMVector3Dot(N, D));

		// 선분이 현재 평면과 거의 평행
		if ( fabsf(denom) <= EPSILON )
		{
			if ( fA < 0.0f )
				return false;

			continue;
		}

		// X(t) = fA + t(fB - fA), 선분 위의 위치 벡터 구하는 식
		// dot(N, X) + d = 0, 평면 방정식
		// dot(N, X(t)) + d = 0, 선분 위의 위치 벡터를 평면 방정식에 대입한 식
		// 이 식을 만족하는 t를 구하면, 무한 평면 위에 놓이는 선분 위의 점을 찾을 수 있다.
		// dot(N, fA + t(fB - fA)) + d = 0
		// dot(N, fA) + d + t * dot(N, fB - fA) = 0
		// fA + t * denom = 0 
		// t = -fA / denom
		
		float t = -fA / denom;

		// t < 0		: A보다 앞쪽 연장선
		// t == 0		: 시작점 A
		// 0 < t < 1	: 선분 AB 사이
		// t == 1		: 끝점 B
		// t > 1		: B보다 뒤쪽 연장선
		// 즉, t < 0 이거나 t > 1 이면 선분 바깥쪽 t시점에서 평면과 만나므로 선분 위에 있는 점이 아님

		// denom > 0 : 선분 A->B가 현재 평면 법선 방향으로 진행
		// denom < 0 : 선분 A->B가 현재 평면 법선 반대 방향으로 진행
		// denom == 0에 가까움 : 선분이 현재 평면과 거의 평행
		// 현재 프러스텀 평면의 바깥 -> 안쪽 진입
		if ( denom < 0.0f )
			tEnter = max(tEnter, t); // 0 < t
		// 현재 프러스텀 평면의 안쪽 -> 바깥 이탈
		else
			tExit = min(tExit, t);	// t < 1

		// t < 0 or t > 1
		if ( tEnter > tExit )
			return false;
	}

	return true;
}

bool BoundingCapsule::Intersects(const BoundingCapsule& ca) const noexcept
{
    XMVECTOR A0 = XMLoadFloat3(&p0);
    XMVECTOR B0 = XMLoadFloat3(&p1);

    XMVECTOR A1 = XMLoadFloat3(&ca.p0);
    XMVECTOR B1 = XMLoadFloat3(&ca.p1);

    XMVECTOR cp0, cp1;
    float distSq = Vector3::distSegmentToSegment(A0, B0, A1, B1);

    float r = Radius + ca.Radius;
    return distSq <= r * r;
}

bool BoundingCapsule::Intersects(FXMVECTOR V0, FXMVECTOR V1, FXMVECTOR V2) const noexcept
{
    XMVECTOR A = XMLoadFloat3(&p0);
    XMVECTOR B = XMLoadFloat3(&p1);
    XMVECTOR RadiusSq = XMVectorReplicate(Radius * Radius);

    // 캡슐 선분 - 삼각형 꼭짓점 최단거리
    float d0 = Vector3::distPointToSegment(A, B, V0);
    float d1 = Vector3::distPointToSegment(A, B, V1);
    float d2 = Vector3::distPointToSegment(A, B, V2);

    XMVECTOR vertexDistSq = XMVectorSet(d0, d1, d2, d2);

    XMVECTOR IntersectionP = XMVectorLessOrEqual(vertexDistSq, RadiusSq);
    XMVECTOR Intersection = IntersectionP;

    // 캡슐 선분 - 삼각형 변 최단거리

    float e0 = Vector3::distSegmentToSegment(A, B, V0, V1);
    float e1 = Vector3::distSegmentToSegment(A, B, V1, V2);
    float e2 = Vector3::distSegmentToSegment(A, B, V2, V0);

    XMVECTOR edgeDistSq = XMVectorSet(e0, e1, e2, e2);

    XMVECTOR IntersectionE = XMVectorLessOrEqual(edgeDistSq, RadiusSq);
    Intersection = XMVectorOrInt(Intersection, IntersectionE);

    // 캡슐 선분 - 삼각형 면 최단거리
    float f0 = Vector3::distSegmentToTriangle(A, B, V0, V1, V2);

    XMVECTOR faceDistSq = XMVectorReplicate(f0);

    XMVECTOR IntersectionF = XMVectorLessOrEqual(faceDistSq, RadiusSq);
    Intersection = XMVectorOrInt(Intersection, IntersectionF);

    return XMVector4NotEqualInt(Intersection, XMVectorZero());
 }

ContainmentType BoundingCapsule::Contains(const BoundingOrientedBox& box) const noexcept
{
    if (!Intersects(box))
        return DISJOINT;

    XMVECTOR A = XMLoadFloat3(&p0);
    XMVECTOR B = XMLoadFloat3(&p1);
    float radiusSq = Radius * Radius;

    XMVECTOR center = XMLoadFloat3(&box.Center);
    XMVECTOR extents = XMLoadFloat3(&box.Extents);
    XMVECTOR q = XMLoadFloat4(&box.Orientation);
    XMMATRIX R = XMMatrixRotationQuaternion(q);

    static const XMFLOAT3 signs[8] =
    {
        { -1.f, -1.f, -1.f }, { -1.f, -1.f,  1.f },
        { -1.f,  1.f, -1.f }, { -1.f,  1.f,  1.f },
        {  1.f, -1.f, -1.f }, {  1.f, -1.f,  1.f },
        {  1.f,  1.f, -1.f }, {  1.f,  1.f,  1.f }
    };

    for (int i = 0; i < 8; ++i)
    {
        XMVECTOR local = XMVectorSet(
            signs[i].x * XMVectorGetX(extents),
            signs[i].y * XMVectorGetY(extents),
            signs[i].z * XMVectorGetZ(extents),
            0.0f);

        XMVECTOR world = center + XMVector3Transform(local, R);

        if (Vector3::distPointToSegment(A, B, world) > radiusSq)
            return INTERSECTS;
    }

    return CONTAINS;
}
ContainmentType BoundingCapsule::Contains(const BoundingFrustum& fr) const noexcept
{
	XMVECTOR A = XMLoadFloat3(&p0);
	XMVECTOR B = XMLoadFloat3(&p1);

	XMVECTOR nearPlane, farPlane, rightPlane, leftPlane, topPlane, bottomPlane;
	fr.GetPlanes(&nearPlane, &farPlane, &rightPlane, &leftPlane, &topPlane, &bottomPlane);

	XMVECTOR planes[6] =
	{
		nearPlane, farPlane, rightPlane,
		leftPlane, topPlane, bottomPlane
	};

	bool allInside = true;

	for ( int i = 0; i < 6; ++i )
	{
		float da = XMVectorGetX(XMPlaneDotCoord(planes[i], A));
		float db = XMVectorGetX(XMPlaneDotCoord(planes[i], B));

		float dmin = min(da, db);
		float dmax = max(da, db);

		if ( dmax < -Radius )
			return DISJOINT;

		if ( dmin < Radius )
			allInside = false;
	}

	return allInside ? CONTAINS : INTERSECTS;
}

ContainmentType BoundingCapsule::Contains(const BoundingCapsule& ca) const noexcept
{
    if (!Intersects(ca))
        return DISJOINT;

    if (Radius < ca.Radius)
        return INTERSECTS;

    XMVECTOR A = XMLoadFloat3(&p0);
    XMVECTOR B = XMLoadFloat3(&p1);

    XMVECTOR C0 = XMLoadFloat3(&ca.p0);
    XMVECTOR C1 = XMLoadFloat3(&ca.p1);

    float innerR = Radius - ca.Radius;
    float innerRSq = innerR * innerR;

    float d0 = Vector3::distPointToSegment(A, B, C0);
    float d1 = Vector3::distPointToSegment(A, B, C1);

    return (d0 <= innerRSq && d1 <= innerRSq) ? CONTAINS : INTERSECTS;
}
