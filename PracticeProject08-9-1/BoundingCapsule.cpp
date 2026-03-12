#include "stdafx.h"
#include "BoundingCapsule.h"

void BoundingCapsule::Transform(BoundingCapsule& Out, FXMMATRIX M) const noexcept
{
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
    return ContainmentType();
}

bool BoundingCapsule::Intersects(FXMVECTOR V0, FXMVECTOR V1, FXMVECTOR V2) const noexcept
{
    XMVECTOR A = XMLoadFloat3(&p0);
    XMVECTOR B = XMLoadFloat3(&p1);
    XMVECTOR vRadius = XMVectorReplicatePtr(&Radius);
    // 
    XMVECTOR closestV0 = Vector3::ClosestPointOnSegment(A, B, V0);
    XMVECTOR closestV1 = Vector3::ClosestPointOnSegment(A, B, V1);
    XMVECTOR closestV2 = Vector3::ClosestPointOnSegment(A, B, V2);

    XMVECTOR d0 = XMVector3LengthSq(V0 - closestV0);
    XMVECTOR d1 = XMVector3LengthSq(V1 - closestV1);
    XMVECTOR d2 = XMVector3LengthSq(V2 - closestV2);

    XMVECTOR RadiusSq = XMVectorMultiply(vRadius, vRadius);

    XMVECTOR IntersectionV0 = XMVectorLessOrEqual(d0, RadiusSq);
    XMVECTOR IntersectionV1 = XMVectorLessOrEqual(d1, RadiusSq);
    XMVECTOR IntersectionV2 = XMVectorLessOrEqual(d2, RadiusSq);

    XMVECTOR IntersectionP = XMVectorOrInt(IntersectionV0, IntersectionV1);
    XMVECTOR Intersection = XMVectorOrInt(IntersectionP, IntersectionV2);

    XMVECTOR cpCapsule, cpEdge;

    float e0 = Vector3::ClosestSegmentSegment(A, B, V0, V1, cpCapsule, cpEdge);
    float e1 = Vector3::ClosestSegmentSegment(A, B, V1, V2, cpCapsule, cpEdge);
    float e2 = Vector3::ClosestSegmentSegment(A, B, V2, V0, cpCapsule, cpEdge);

    XMVECTOR edgeDistSq = XMVectorSet(e0, e1, e2, e2);

    XMVECTOR IntersectionE = XMVectorLessOrEqual(edgeDistSq, RadiusSq);

    Intersection = XMVectorOrInt(Intersection, IntersectionE);

    XMVECTOR N = XMVector3Normalize(XMVector3Cross(XMVectorSubtract(V1, V0), XMVectorSubtract(V2, V0)));
    float distA = XMVectorGetX(XMVector3Dot(A - V0, N));
    float distB = XMVectorGetX(XMVector3Dot(B - V0, N));

 }
