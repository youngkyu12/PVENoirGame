#include "pch.h"
#include "BoundingCapsule.h"

namespace
{
	inline float Dot3(FXMVECTOR a, FXMVECTOR b)
	{
		return XMVectorGetX(XMVector3Dot(a, b));
	}

	inline XMVECTOR ClosestPointOnSegment(FXMVECTOR a, FXMVECTOR b, FXMVECTOR p)
	{
		const XMVECTOR ab = b - a;
		const float abLenSq = Dot3(ab, ab);
		if (abLenSq <= 1e-8f)
			return a;

		float t = Dot3(p - a, ab) / abLenSq;
		t = (std::max)(0.0f, (std::min)(1.0f, t));
		return a + ab * t;
	}

	inline float DistPointToSegmentSq(FXMVECTOR a, FXMVECTOR b, FXMVECTOR p)
	{
		const XMVECTOR c = ClosestPointOnSegment(a, b, p);
		return XMVectorGetX(XMVector3LengthSq(c - p));
	}

	inline float DistSegmentToSegmentSq(FXMVECTOR p1, FXMVECTOR q1, FXMVECTOR p2, FXMVECTOR q2)
	{
		const float EPS = 1e-6f;

		const XMVECTOR d1 = q1 - p1;
		const XMVECTOR d2 = q2 - p2;
		const XMVECTOR r = p1 - p2;

		const float a = Dot3(d1, d1);
		const float e = Dot3(d2, d2);
		const float f = Dot3(d2, r);

		float s = 0.0f;
		float t = 0.0f;

		if (a <= EPS && e <= EPS)
			return XMVectorGetX(XMVector3LengthSq(p1 - p2));

		if (a <= EPS)
		{
			t = (std::max)(0.0f, (std::min)(1.0f, f / e));
		}
		else
		{
			const float c = Dot3(d1, r);
			if (e <= EPS)
			{
				s = (std::max)(0.0f, (std::min)(1.0f, -c / a));
			}
			else
			{
				const float b = Dot3(d1, d2);
				const float denom = a * e - b * b;
				if (denom != 0.0f)
					s = (std::max)(0.0f, (std::min)(1.0f, (b * f - c * e) / denom));

				t = (b * s + f) / e;
				if (t < 0.0f)
				{
					t = 0.0f;
					s = (std::max)(0.0f, (std::min)(1.0f, -c / a));
				}
				else if (t > 1.0f)
				{
					t = 1.0f;
					s = (std::max)(0.0f, (std::min)(1.0f, (b - c) / a));
				}
			}
		}

		const XMVECTOR c1 = p1 + d1 * s;
		const XMVECTOR c2 = p2 + d2 * t;
		return XMVectorGetX(XMVector3LengthSq(c1 - c2));
	}

	inline bool IntersectSegmentAABB(FXMVECTOR A, FXMVECTOR B, FXMVECTOR E)
	{
		const float ax = XMVectorGetX(A), ay = XMVectorGetY(A), az = XMVectorGetZ(A);
		const float bx = XMVectorGetX(B), by = XMVectorGetY(B), bz = XMVectorGetZ(B);
		const float ex = XMVectorGetX(E), ey = XMVectorGetY(E), ez = XMVectorGetZ(E);

		const float d[3] = { bx - ax, by - ay, bz - az };
		const float p[3] = { ax, ay, az };
		const float mn[3] = { -ex, -ey, -ez };
		const float mx[3] = { ex, ey, ez };

		float tmin = 0.0f;
		float tmax = 1.0f;

		for (int i = 0; i < 3; ++i)
		{
			if (fabsf(d[i]) <= 1e-8f)
			{
				if (p[i] < mn[i] || p[i] > mx[i])
					return false;
			}
			else
			{
				float ood = 1.0f / d[i];
				float t1 = (mn[i] - p[i]) * ood;
				float t2 = (mx[i] - p[i]) * ood;
				if (t1 > t2) std::swap(t1, t2);
				tmin = (std::max)(tmin, t1);
				tmax = (std::min)(tmax, t2);
				if (tmin > tmax)
					return false;
			}
		}
		return true;
	}

	inline float DistPointToAABBSq(FXMVECTOR p, FXMVECTOR e)
	{
		XMFLOAT3 pf{}, ef{};
		XMStoreFloat3(&pf, p);
		XMStoreFloat3(&ef, e);

		const float cx = (std::max)(-ef.x, (std::min)(ef.x, pf.x));
		const float cy = (std::max)(-ef.y, (std::min)(ef.y, pf.y));
		const float cz = (std::max)(-ef.z, (std::min)(ef.z, pf.z));

		const float dx = pf.x - cx;
		const float dy = pf.y - cy;
		const float dz = pf.z - cz;
		return dx * dx + dy * dy + dz * dz;
	}

	inline float DistSegmentToAABBSq(FXMVECTOR A, FXMVECTOR B, FXMVECTOR E)
	{
		if (IntersectSegmentAABB(A, B, E))
			return 0.0f;

		float d = DistPointToAABBSq(A, E);
		d = (std::min)(d, DistPointToAABBSq(B, E));
		return d;
	}
}

void BoundingCapsule::Transform(BoundingCapsule& Out, FXMMATRIX M) const noexcept
{
	XMVECTOR P0 = XMVector3TransformCoord(XMLoadFloat3(&p0), M);
	XMVECTOR P1 = XMVector3TransformCoord(XMLoadFloat3(&p1), M);

	const float sx = sqrtf(XMVectorGetX(XMVector3Dot(M.r[0], M.r[0])));
	const float sy = sqrtf(XMVectorGetX(XMVector3Dot(M.r[1], M.r[1])));
	const float sz = sqrtf(XMVectorGetX(XMVector3Dot(M.r[2], M.r[2])));
	const float scale = (std::max)(sx, (std::max)(sy, sz));

	XMStoreFloat3(&Out.p0, P0);
	XMStoreFloat3(&Out.p1, P1);

	XMVECTOR C = 0.5f * (P0 + P1);
	XMStoreFloat3(&Out.Center, C);

	Out.Radius = Radius * scale;
	Out.Height = XMVectorGetX(XMVector3Length(P1 - P0)) + 2.0f * Out.Radius;
	Out.Direction = Direction;
}

void BoundingCapsule::Transform(BoundingCapsule& Out, float Scale, FXMVECTOR Rotation, FXMVECTOR Translation) const noexcept
{
	XMMATRIX M = XMMatrixScaling(Scale, Scale, Scale)
		* XMMatrixRotationQuaternion(Rotation)
		* XMMatrixTranslationFromVector(Translation);
	Transform(Out, M);
}

ContainmentType BoundingCapsule::Contains(FXMVECTOR Point) const noexcept
{
	const float d = DistPointToSegmentSq(XMLoadFloat3(&p0), XMLoadFloat3(&p1), Point);
	return (d <= Radius * Radius) ? CONTAINS : DISJOINT;
}

ContainmentType BoundingCapsule::Contains(FXMVECTOR V0, FXMVECTOR V1, FXMVECTOR V2) const noexcept
{
	const bool i0 = Contains(V0) != DISJOINT;
	const bool i1 = Contains(V1) != DISJOINT;
	const bool i2 = Contains(V2) != DISJOINT;
	if (i0 && i1 && i2) return CONTAINS;
	if (i0 || i1 || i2) return INTERSECTS;
	return DISJOINT;
}

ContainmentType BoundingCapsule::Contains(const BoundingSphere& sh) const noexcept
{
	if (!Intersects(sh)) return DISJOINT;
	return INTERSECTS;
}

ContainmentType BoundingCapsule::Contains(const BoundingBox& box) const noexcept
{
	if (!Intersects(box)) return DISJOINT;
	return INTERSECTS;
}

ContainmentType BoundingCapsule::Contains(const BoundingOrientedBox& box) const noexcept
{
	if (!Intersects(box)) return DISJOINT;
	return INTERSECTS;
}

ContainmentType BoundingCapsule::Contains(const BoundingCapsule& ca) const noexcept
{
	if (!Intersects(ca)) return DISJOINT;
	return INTERSECTS;
}

bool BoundingCapsule::Intersects(const BoundingSphere& sh) const noexcept
{
	const XMVECTOR C = XMLoadFloat3(&sh.Center);
	const float d = DistPointToSegmentSq(XMLoadFloat3(&p0), XMLoadFloat3(&p1), C);
	const float r = Radius + sh.Radius;
	return d <= r * r;
}

bool BoundingCapsule::Intersects(const BoundingBox& box) const noexcept
{
	BoundingOrientedBox obb;
	BoundingOrientedBox::CreateFromBoundingBox(obb, box);
	return Intersects(obb);
}

bool BoundingCapsule::Intersects(const BoundingOrientedBox& box) const noexcept
{
	const XMVECTOR A = XMLoadFloat3(&p0);
	const XMVECTOR B = XMLoadFloat3(&p1);

	const XMVECTOR center = XMLoadFloat3(&box.Center);
	const XMVECTOR extents = XMLoadFloat3(&box.Extents);
	const XMVECTOR q = XMLoadFloat4(&box.Orientation);

	const XMVECTOR localA = XMVector3InverseRotate(A - center, q);
	const XMVECTOR localB = XMVector3InverseRotate(B - center, q);

	const float d = DistSegmentToAABBSq(localA, localB, extents);
	return d <= Radius * Radius;
}

bool BoundingCapsule::Intersects(const BoundingCapsule& ca) const noexcept
{
	const float d = DistSegmentToSegmentSq(XMLoadFloat3(&p0), XMLoadFloat3(&p1), XMLoadFloat3(&ca.p0), XMLoadFloat3(&ca.p1));
	const float r = Radius + ca.Radius;
	return d <= r * r;
}

bool BoundingCapsule::Intersects(FXMVECTOR V0, FXMVECTOR V1, FXMVECTOR V2) const noexcept
{
	return Contains(V0, V1, V2) != DISJOINT;
}

PlaneIntersectionType BoundingCapsule::Intersects(FXMVECTOR Plane) const noexcept
{
	const XMVECTOR A = XMLoadFloat3(&p0);
	const XMVECTOR B = XMLoadFloat3(&p1);
	const float da = XMVectorGetX(XMPlaneDotCoord(Plane, A));
	const float db = XMVectorGetX(XMPlaneDotCoord(Plane, B));
	const float dmax = (std::max)(da, db);
	const float dmin = (std::min)(da, db);

	if (dmax < -Radius) return BACK;
	if (dmin > Radius) return FRONT;
	return INTERSECTING;
}

bool BoundingCapsule::Intersects(FXMVECTOR Origin, FXMVECTOR Direction, float& Dist) const noexcept
{
	const XMVECTOR C = XMLoadFloat3(&Center);
	const XMVECTOR m = Origin - C;
	const float r = Radius + 0.5f * Height;

	const float b = Dot3(m, Direction);
	const float c = Dot3(m, m) - r * r;
	if (c > 0.0f && b > 0.0f) return false;

	const float disc = b * b - c;
	if (disc < 0.0f) return false;

	Dist = -b - sqrtf(disc);
	if (Dist < 0.0f) Dist = 0.0f;
	return true;
}

