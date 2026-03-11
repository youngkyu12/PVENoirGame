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

    XMVECTOR AB = B - A;
    XMVECTOR AP = Point - A;

    float abLenSq = XMVectorGetX(XMVector3Dot(AB, AB));
    float Projection = 0.0f;

    if (abLenSq > 0.0f)
    {
        Projection = XMVectorGetX(XMVector3Dot(AP, AB)) / abLenSq;
        Projection = std::clamp(Projection, 0.0f, 1.0f);
    }

    XMVECTOR closest = A + AB * Projection;
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
    return false;
}
