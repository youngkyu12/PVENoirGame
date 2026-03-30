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

    XMVECTOR closest = Collide::ClosestPointOnSegment(A, B, Point);
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

    float d0 = Collide::distPointToSegment(A, B, V0);
    float d1 = Collide::distPointToSegment(A, B, V1);
    float d2 = Collide::distPointToSegment(A, B, V2);

    XMVECTOR distSq = XMVectorSet(d0, d1, d2, 0.0f);
    XMVECTOR radiusSq = XMVectorReplicate(Radius * Radius);

    XMVECTOR inside = XMVectorLessOrEqual(distSq, radiusSq);

    return XMVector3EqualInt(inside, XMVectorTrueInt()) ? CONTAINS : INTERSECTS;
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

	XMVECTOR D = XMVectorSubtract(BLocal, ALocal);
	
	float ax = XMVectorGetX(ALocal);
	float ay = XMVectorGetY(ALocal);
	float az = XMVectorGetZ(ALocal);

	float bx = XMVectorGetX(BLocal);
	float by = XMVectorGetY(BLocal);
	float bz = XMVectorGetZ(BLocal);

	float ex = XMVectorGetX(extents);
	float ey = XMVectorGetY(extents);
	float ez = XMVectorGetZ(extents);

	float d[3] = { bx - ax, by - ay, bz - az };
	float p0[3] = { ax, ay, az };
	float ext[3] = { ex, ey, ez };
	float minEx[3] = { -ex, -ey, -ez };
	float maxEx[3] = { ex,  ey,  ez };

	bool segmentIntersectsBox = true;

	float tmin = 0.0f;
	float tmax = 1.0f;

	for ( int i = 0; i < 3; ++i )
	{
		if ( fabsf(d[i]) <= EPSILON )
		{
			if ( p0[i] < minEx[i] || p0[i] > maxEx[i] )
			{
				segmentIntersectsBox = false;
				break;
			}
		}
		else
		{
			float ood = 1.0f / d[i];
			float t1 = ( minEx[i] - p0[i] ) * ood;
			float t2 = ( maxEx[i] - p0[i] ) * ood;

			if ( t1 > t2 )
				std::swap(t1, t2);

			tmin = max(tmin, t1);
			tmax = min(tmax, t2);

			if ( tmin > tmax )
			{
				segmentIntersectsBox = false;
				break;
			}
		}
	}

	float distSq = 0.0f;

	if ( !segmentIntersectsBox )
	{
		// 선분 끝점 후보
		distSq = Collide::DistanceSqPointToAABB(ALocal, extents);
		distSq = min(distSq, Collide::DistanceSqPointToAABB(BLocal, extents));

		// 각 축의 경계 무한평면을 지나는 시점은
		// 점 - AABB 거리 함수의 형태가 바뀌는 분기점이라서
		//	최소 거리 후보가 될 수 있다
		for ( int i = 0; i < 3; ++i )
		{
			if ( fabsf(d[i]) > EPSILON )
			{
				float t1 = ( -ext[i] - p0[i] ) / d[i]; //  -extents = ALocal + t(BLocal - ALocal)
													// t = (-extens - ALocal)/(BLocal - ALocal)
				float t2 = ( ext[i] - p0[i] ) / d[i];

				if ( 0.0f <= t1 && t1 <= 1.0f )
				{
					XMVECTOR P1 = XMVectorAdd(ALocal, XMVectorScale(D, t1)); // P(t) = ALocal + t(BLocal - ALocal)
					distSq = min(distSq, Collide::DistanceSqPointToAABB(P1, extents));
				}

				if ( 0.0f <= t2 && t2 <= 1.0f )
				{
					XMVECTOR P2 = XMVectorAdd(ALocal, XMVectorScale(D, t2));
					distSq = min(distSq, Collide::DistanceSqPointToAABB(P2, extents));
				}
			}
		}
	}

	return distSq <= Radius * Radius;
}

bool BoundingCapsule::Intersects(const BoundingFrustum& fr) const noexcept
{
	// 충돌 연산 다시 작성
	float distSq = 0.0f;

	return distSq <= Radius * Radius;
}

bool BoundingCapsule::Intersects(const BoundingCapsule& ca) const noexcept
{
    XMVECTOR A0 = XMLoadFloat3(&p0);
    XMVECTOR B0 = XMLoadFloat3(&p1);

    XMVECTOR A1 = XMLoadFloat3(&ca.p0);
    XMVECTOR B1 = XMLoadFloat3(&ca.p1);

    XMVECTOR cp0, cp1;
    float distSq = Collide::distSegmentToSegment(A0, B0, A1, B1);

    float r = Radius + ca.Radius;
    return distSq <= r * r;
}

bool BoundingCapsule::Intersects(FXMVECTOR V0, FXMVECTOR V1, FXMVECTOR V2) const noexcept
{
    XMVECTOR A = XMLoadFloat3(&p0);
    XMVECTOR B = XMLoadFloat3(&p1);
    XMVECTOR RadiusSq = XMVectorReplicate(Radius * Radius);

    // 캡슐 선분 - 삼각형 꼭짓점 최단거리
    float d0 = Collide::distPointToSegment(A, B, V0);
    float d1 = Collide::distPointToSegment(A, B, V1);
    float d2 = Collide::distPointToSegment(A, B, V2);

    XMVECTOR vertexDistSq = XMVectorSet(d0, d1, d2, d2);
	
    XMVECTOR IntersectionP = XMVectorLessOrEqual(vertexDistSq, RadiusSq);
    XMVECTOR Intersection = IntersectionP;

    // 캡슐 선분 - 삼각형 변 최단거리

    float e0 = Collide::distSegmentToSegment(A, B, V0, V1);
    float e1 = Collide::distSegmentToSegment(A, B, V1, V2);
    float e2 = Collide::distSegmentToSegment(A, B, V2, V0);

    XMVECTOR edgeDistSq = XMVectorSet(e0, e1, e2, e2);

    XMVECTOR IntersectionE = XMVectorLessOrEqual(edgeDistSq, RadiusSq);
    Intersection = XMVectorOrInt(Intersection, IntersectionE);

    // 캡슐 선분 - 삼각형 면 최단거리
    float f0 = Collide::distSegmentToTriangle(A, B, V0, V1, V2);

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

        if (Collide::distPointToSegment(A, B, world) > radiusSq)
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

    float d0 = Collide::distPointToSegment(A, B, C0);
    float d1 = Collide::distPointToSegment(A, B, C1);

    return (d0 <= innerRSq && d1 <= innerRSq) ? CONTAINS : INTERSECTS;
}
