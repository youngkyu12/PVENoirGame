#pragma once
#include "MathHelper.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <cstdint>
using namespace DirectX;

enum class EDirection : uint8_t
{
	X = 0,
	Y,
	Z
};

struct BoundingCapsule
{
    XMFLOAT3 p0;
    XMFLOAT3 p1;
    XMFLOAT3 Center;
    float Radius;
    float Height;
    EDirection Direction;

    // Creators
    BoundingCapsule() noexcept : p0(0.0f, 0.0f, 0.0f), p1(0.0f, 0.0f, 0.0f), Center(0.0f, 0.0f, 0.0f), Radius(0.1f), Height(0.0f) {}

    BoundingCapsule(const BoundingCapsule&) = default;
    BoundingCapsule& operator=(const BoundingCapsule&) = default;

    BoundingCapsule(BoundingCapsule&&) = default;
    BoundingCapsule& operator=(BoundingCapsule&&) = default;

    constexpr BoundingCapsule(const XMFLOAT3& q0, const XMFLOAT3& q1, const XMFLOAT3& center, const float& radius, const float& height, const EDirection& direction) noexcept
        : p0(q0), p1(q1), Center(center), Radius(radius), Height(height), Direction(direction) {
    }

    // Methods
    void Transform(BoundingCapsule& Out, FXMMATRIX M) const noexcept;
    void Transform(BoundingCapsule& Out, float Scale, FXMVECTOR Rotation, FXMVECTOR Translation) const noexcept;

    ContainmentType Contains(FXMVECTOR Point) const noexcept;
    ContainmentType Contains(FXMVECTOR V0, FXMVECTOR V1, FXMVECTOR V2) const noexcept;
    //ContainmentType Contains(const BoundingSphere& sh) const noexcept;
    //ContainmentType Contains(const BoundingBox& box) const noexcept;
    ContainmentType Contains(const BoundingOrientedBox& box) const noexcept;
    ContainmentType Contains(const BoundingFrustum& fr) const noexcept;
    ContainmentType Contains(const BoundingCapsule& ca) const noexcept;

    bool Intersects(const BoundingSphere& sh) const noexcept;
    bool Intersects(const BoundingBox& box) const noexcept;
    bool Intersects(const BoundingOrientedBox& box) const noexcept;
    bool Intersects(const BoundingFrustum& fr) const noexcept;
    bool Intersects(const BoundingCapsule& ca) const noexcept;

    bool Intersects( FXMVECTOR V0,  FXMVECTOR V1,  FXMVECTOR V2) const noexcept;

    //PlaneIntersectionType Intersects( FXMVECTOR Plane) const noexcept;

   // bool Intersects( FXMVECTOR Origin,  FXMVECTOR Direction,  float& Dist) const noexcept;
    // Ray-sphere test

    //ContainmentType ContainedBy( FXMVECTOR Plane0,  FXMVECTOR Plane1,  FXMVECTOR Plane2,
    //     GXMVECTOR Plane3,  HXMVECTOR Plane4,  HXMVECTOR Plane5) const noexcept;
};