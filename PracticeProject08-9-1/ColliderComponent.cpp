//-----------------------------------------------------------------------------
// File: ColliderComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "ColliderComponent.h"
#include "Object.h"
#include "AnimatorComponent.h"

#include <fstream>
#include <cctype>
#include <cstring>

BoundingOrientedBox CColliderComponent::MakeLocalOOBB(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
	BoundingOrientedBox box{};

	box.Center = XMFLOAT3(
		( Min.x + Max.x ) * 0.5f,
		( Min.y + Max.y ) * 0.5f,
		( Min.z + Max.z ) * 0.5f
	);

	box.Extents = XMFLOAT3(
		( Max.x - Min.x ) * 0.5f,
		( Max.y - Min.y ) * 0.5f,
		( Max.z - Min.z ) * 0.5f
	);

	box.Orientation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	return box;
}

static BoundingCapsule MakeCapsuleFromSegment(
	const XMFLOAT3& p0,
	const XMFLOAT3& p1,
	float radius)
{
	BoundingCapsule capsule{};
	capsule.p0 = p0;
	capsule.p1 = p1;

	capsule.Center = XMFLOAT3(
		( p0.x + p1.x ) * 0.5f,
		( p0.y + p1.y ) * 0.5f,
		( p0.z + p1.z ) * 0.5f
	);

	capsule.Radius = radius;

	const XMVECTOR A = XMLoadFloat3(&p0);
	const XMVECTOR B = XMLoadFloat3(&p1);
	const XMVECTOR D = B - A;

	const float axisLen = XMVectorGetX(XMVector3Length(D));
	capsule.Height = axisLen + radius * 2.0f;

	XMFLOAT3 absD{};
	XMStoreFloat3(&absD, XMVectorAbs(D));

	if ( absD.x >= absD.y && absD.x >= absD.z ) capsule.Direction = EDirection::X;
	else if ( absD.y >= absD.x && absD.y >= absD.z ) capsule.Direction = EDirection::Y;
	else capsule.Direction = EDirection::Z;

	return capsule;
}

namespace
{
	struct StaticRefinedVariant
	{
		std::vector<BoundingOrientedBox> boxes;
		XMFLOAT3 unionMin = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
		XMFLOAT3 unionMax = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	};

	using RefinedNameMap = std::unordered_map<std::string, std::vector<StaticRefinedVariant>>;
	static std::unordered_map<std::string, RefinedNameMap> gStaticRefinedOOBBReport;

	static std::string TrimCopy(const std::string& s)
	{
		size_t begin = 0;
		while ( begin < s.size() && std::isspace(static_cast< unsigned char >(s[begin])) )
			++begin;

		size_t end = s.size();
		while ( end > begin && std::isspace(static_cast< unsigned char >( s[end - 1] )) )
			--end;

		return s.substr(begin, end - begin);
	}

	static bool ParseFloat3Field(const std::string& line, const char* label, XMFLOAT3& out)
	{
		if ( line.rfind(label, 0) != 0 )
			return false;

		float x = 0.0f, y = 0.0f, z = 0.0f;
		if ( sscanf_s(line.c_str() + std::strlen(label), " (%f, %f, %f)", &x, &y, &z) != 3 )
			return false;

		out = XMFLOAT3(x, y, z);
		return true;
	}

	static BoundingOrientedBox MakeLocalOOBBFromCenterSizeEuler(
		const XMFLOAT3& center,
		const XMFLOAT3& size,
		const XMFLOAT3& eulerDeg)
	{
		BoundingOrientedBox box{};
		box.Center = center;
		box.Extents = XMFLOAT3(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f);

		const XMVECTOR q = XMQuaternionRotationRollPitchYaw(
			XMConvertToRadians(eulerDeg.x),
			XMConvertToRadians(eulerDeg.y),
			XMConvertToRadians(eulerDeg.z)
		);
		XMStoreFloat4(&box.Orientation, q);
		return box;
	}

	static void ExpandMinMax(XMFLOAT3& minV, XMFLOAT3& maxV, const XMFLOAT3& p)
	{
		minV.x = min(minV.x, p.x);
		minV.y = min(minV.y, p.y);
		minV.z = min(minV.z, p.z);

		maxV.x = max(maxV.x, p.x);
		maxV.y = max(maxV.y, p.y);
		maxV.z = max(maxV.z, p.z);
	}

	static void ComputeUnionBounds(
		const std::vector<BoundingOrientedBox>& boxes,
		XMFLOAT3& outMin,
		XMFLOAT3& outMax)
	{
		outMin = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
		outMax = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for ( const BoundingOrientedBox& box : boxes )
		{
			XMFLOAT3 corners[8];
			box.GetCorners(corners);

			for ( int i = 0; i < 8; ++i )
				ExpandMinMax(outMin, outMax, corners[i]);
		}
	}

	static const StaticRefinedVariant* FindBestRefinedVariant(
		const std::string& assetKey,
		const std::string& subMeshName,
		const XMFLOAT3& subMeshMin,
		const XMFLOAT3& subMeshMax)
	{
		auto itAsset = gStaticRefinedOOBBReport.find(assetKey);
		if ( itAsset == gStaticRefinedOOBBReport.end() )
			return nullptr;

		auto itName = itAsset->second.find(subMeshName);
		if ( itName == itAsset->second.end() )
			return nullptr;

		const std::vector<StaticRefinedVariant>& variants = itName->second;
		if ( variants.empty() )
			return nullptr;

		if ( variants.size() == 1 )
			return &variants[0];

		const XMFLOAT3 subCenter(
			( subMeshMin.x + subMeshMax.x ) * 0.5f,
			( subMeshMin.y + subMeshMax.y ) * 0.5f,
			( subMeshMin.z + subMeshMax.z ) * 0.5f
		);

		const XMFLOAT3 subSize(
			subMeshMax.x - subMeshMin.x,
			subMeshMax.y - subMeshMin.y,
			subMeshMax.z - subMeshMin.z
		);

		const StaticRefinedVariant* best = nullptr;
		float bestScore = FLT_MAX;

		for ( const StaticRefinedVariant& variant : variants )
		{
			const XMFLOAT3 varCenter(
				( variant.unionMin.x + variant.unionMax.x ) * 0.5f,
				( variant.unionMin.y + variant.unionMax.y ) * 0.5f,
				( variant.unionMin.z + variant.unionMax.z ) * 0.5f
			);

			const XMFLOAT3 varSize(
				variant.unionMax.x - variant.unionMin.x,
				variant.unionMax.y - variant.unionMin.y,
				variant.unionMax.z - variant.unionMin.z
			);

			const float sizeScore =
				fabsf(subSize.x - varSize.x) +
				fabsf(subSize.y - varSize.y) +
				fabsf(subSize.z - varSize.z);

			const float centerScore =
				fabsf(subCenter.x - varCenter.x) +
				fabsf(subCenter.y - varCenter.y) +
				fabsf(subCenter.z - varCenter.z);

			const float score = sizeScore * 10.0f + centerScore;

			if ( score < bestScore )
			{
				bestScore = score;
				best = &variant;
			}
		}

		return best;
	}
}

bool CColliderComponent::LoadStaticRefinedOOBBReport(const std::string& txtPath)
{
	gStaticRefinedOOBBReport.clear();

	std::ifstream fin(txtPath);
	if ( !fin.is_open() )
		return false;

	std::string currentAssetName;
	std::string currentAName;
	std::vector<BoundingOrientedBox> currentBoxes;

	struct PendingCubeState
	{
		bool hasCenter = false;
		bool hasRotation = false;
		XMFLOAT3 center = XMFLOAT3(0, 0, 0);
		XMFLOAT3 rotation = XMFLOAT3(0, 0, 0);

		void Reset()
		{
			hasCenter = false;
			hasRotation = false;
			center = XMFLOAT3(0, 0, 0);
			rotation = XMFLOAT3(0, 0, 0);
		}
	} pendingCube;

	auto FlushCurrentA = [ & ] ()
		{
			if ( currentAssetName.empty() || currentAName.empty() || currentBoxes.empty() )
			{
				currentBoxes.clear();
				return;
			}

			StaticRefinedVariant variant{};
			variant.boxes = currentBoxes;
			ComputeUnionBounds(variant.boxes, variant.unionMin, variant.unionMax);

			gStaticRefinedOOBBReport[currentAssetName][currentAName].push_back(std::move(variant));
			currentBoxes.clear();
		};

	std::string line;
	while ( std::getline(fin, line) )
	{
		const std::string t = TrimCopy(line);
		if ( t.empty() )
			continue;

		if ( t.rfind("[TopRootGroup", 0) == 0 )
		{
			FlushCurrentA();
			currentAName.clear();
			pendingCube.Reset();
			continue;
		}

		if ( t.rfind("TopRootName:", 0) == 0 )
		{
			FlushCurrentA();
			currentAName.clear();
			pendingCube.Reset();
			currentAssetName = TrimCopy(t.substr(std::strlen("TopRootName:")));
			continue;
		}

		if ( t.rfind("[A #", 0) == 0 )
		{
			FlushCurrentA();
			currentAName.clear();
			pendingCube.Reset();
			continue;
		}

		if ( t.rfind("AName:", 0) == 0 )
		{
			FlushCurrentA();
			currentBoxes.clear();
			pendingCube.Reset();
			currentAName = TrimCopy(t.substr(std::strlen("AName:")));
			continue;
		}

		XMFLOAT3 value{};
		if ( ParseFloat3Field(t, "CenterInA_Local:", value) )
		{
			pendingCube.center = value;
			pendingCube.hasCenter = true;
			continue;
		}

		if ( ParseFloat3Field(t, "RotationInA_LocalEuler:", value) )
		{
			pendingCube.rotation = value;
			pendingCube.hasRotation = true;
			continue;
		}

		if ( ParseFloat3Field(t, "SizeInA_Local:", value) )
		{
			if ( pendingCube.hasCenter && pendingCube.hasRotation )
			{
				currentBoxes.push_back(
					MakeLocalOOBBFromCenterSizeEuler(
						pendingCube.center,
						value,
						pendingCube.rotation
					)
				);
			}

			pendingCube.Reset();
			continue;
		}
	}

	FlushCurrentA();
	return !gStaticRefinedOOBBReport.empty();
}

void CColliderComponent::ClearStaticRefinedOOBBReport()
{
	gStaticRefinedOOBBReport.clear();
}

void CColliderComponent::BuildHierarchicalOOBBs(const vector<shared_ptr<CMesh>>& meshes)
{
	mMeshOOBBSets.clear();
	mMeshOOBBSets.reserve(meshes.size());

	for ( const shared_ptr<CMesh>& mesh : meshes )
	{
		if ( !mesh ) continue;

		MeshOOBBSet set{};
		set.LocalMeshOOBB = MakeLocalOOBB(mesh->GetMeshMin(), mesh->GetMeshMax());
		set.WorldMeshOOBB = set.LocalMeshOOBB;

		set.SubMeshes.reserve(mesh->m_SubMeshes.size());

		for ( const auto& submesh : mesh->m_SubMeshes )
		{
			SubMeshOOBBSet subSet{};
			subSet.subMeshName = submesh.meshName;
			subSet.LocalCoarseOOBB = MakeLocalOOBB(submesh.subMeshMin, submesh.subMeshMax);
			subSet.WorldCoarseOOBB = subSet.LocalCoarseOOBB;
			subSet.useCoarseOOBB = true;

			if ( !mStaticColliderAssetKey.empty() && !subSet.subMeshName.empty() )
			{
				const StaticRefinedVariant* variant = FindBestRefinedVariant(
					mStaticColliderAssetKey,
					subSet.subMeshName,
					submesh.subMeshMin,
					submesh.subMeshMax
				);

				if ( variant && !variant->boxes.empty() )
				{
					subSet.useCoarseOOBB = false;
					subSet.LocalRefinedOOBBs = variant->boxes;
					subSet.WorldRefinedOOBBs.resize(subSet.LocalRefinedOOBBs.size());
				}
			}

			set.SubMeshes.push_back(std::move(subSet));
		}

		mMeshOOBBSets.push_back(std::move(set));
	}
}

CColliderComponent::CColliderComponent(CGameObject* owner, EColliderType Type)
    : CComponentT<CColliderComponent>(owner)
{
    mColliderType = Type;
}

void CColliderComponent::OnCreate(ID3D12Device*, ID3D12GraphicsCommandList*)
{
    mTransform = GetOwner()->GetComponent<CTransformComponent>();
    assert(mTransform && "CColliderComponent requires CTransformComponent");

    mModel = GetOwner()->GetComponent<CModelComponent>();
    assert(mModel && "CColliderComponent requires CModelComponent");
	
	mRender = GetOwner()->GetRenderer();
	assert(mRender && "CColliderComponent requires CRendererComponent");

    const vector<shared_ptr<CMesh>>& meshes = mModel->GetMeshes();

    if (meshes.empty())
        return;
    XMFLOAT3 objMin = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
    XMFLOAT3 objMax = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    switch (mColliderType)
    {
    case EColliderType::AABB:
        for (const shared_ptr<CMesh>& mesh : meshes)
        {
            if (!mesh) continue;

            for (const vector<SubMesh>::iterator::value_type& submesh : mesh->m_SubMeshes)
            {
                objMin.x = min(objMin.x, submesh.subMeshMin.x);
                objMin.y = min(objMin.y, submesh.subMeshMin.y);
                objMin.z = min(objMin.z, submesh.subMeshMin.z);

                objMax.x = max(objMax.x, submesh.subMeshMax.x);
                objMax.y = max(objMax.y, submesh.subMeshMax.y);
                objMax.z = max(objMax.z, submesh.subMeshMax.z);
            }
            SetAABB(objMin, objMax);
        }
        break;
	case EColliderType::OOBB:
	{
		BuildHierarchicalOOBBs(meshes);

		for ( const shared_ptr<CMesh>& mesh : meshes )
		{
			if ( !mesh ) continue;

			const XMFLOAT3 meshMin = mesh->GetMeshMin();
			const XMFLOAT3 meshMax = mesh->GetMeshMax();

			objMin.x = min(objMin.x, meshMin.x);
			objMin.y = min(objMin.y, meshMin.y);
			objMin.z = min(objMin.z, meshMin.z);

			objMax.x = max(objMax.x, meshMax.x);
			objMax.y = max(objMax.y, meshMax.y);
			objMax.z = max(objMax.z, meshMax.z);
		}

		if ( objMin.x <= objMax.x &&
			objMin.y <= objMax.y &&
			objMin.z <= objMax.z )
		{
			SetOOBB(objMin, objMax);
		}

		break;
	}
    case EColliderType::BSphere:

        break;
    case EColliderType::BCapsule:
        for (const shared_ptr<CMesh>& mesh : meshes)
        {
            if (!mesh) continue;

            for (const auto& submesh : mesh->m_SubMeshes)
            {
                objMin.x = min(objMin.x, submesh.subMeshMin.x);
                objMin.y = min(objMin.y, submesh.subMeshMin.y);
                objMin.z = min(objMin.z, submesh.subMeshMin.z);

                objMax.x = max(objMax.x, submesh.subMeshMax.x);
                objMax.y = max(objMax.y, submesh.subMeshMax.y);
                objMax.z = max(objMax.z, submesh.subMeshMax.z);
                SetSubBCapsule(submesh.subMeshMin, submesh.subMeshMax);
            }
            SetBCapsule(objMin, objMax);
        }
        
        break;
    default:
        break;
    }

	bool isSkinned = false;
	for ( const auto& mesh : meshes )
	{
		if ( mesh && mesh->IsSkinnedMesh() )
		{
			isSkinned = true;
			break;
		}
	}

	if ( mColliderType == EColliderType::BCapsule && isSkinned )
	{
		BuildBoneCapsulesFromSkeleton();
	}
    
    UpdateWorldBounds();
}

void CColliderComponent::SetAABB(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
    LocalAABB.Center = XMFLOAT3(
        (Min.x + Max.x) * 0.5f,
        (Min.y + Max.y) * 0.5f,
        (Min.z + Max.z) * 0.5f);

    LocalAABB.Extents = XMFLOAT3(
        (Max.x - Min.x) * 0.5f,
        (Max.y - Min.y) * 0.5f,
        (Max.z - Min.z) * 0.5f);
}

void CColliderComponent::SetOOBB(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
	LocalOOBB.Center = XMFLOAT3(
		( Min.x + Max.x ) * 0.5f,
		( Min.y + Max.y ) * 0.5f,
		( Min.z + Max.z ) * 0.5f);

	LocalOOBB.Extents = XMFLOAT3(
		( Max.x - Min.x ) * 0.5f,
		( Max.y - Min.y ) * 0.5f,
		( Max.z - Min.z ) * 0.5f);

	LocalOOBB.Orientation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
}

void CColliderComponent::SetBSphere(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
    LocalBSphere.Center = XMFLOAT3(
        (Min.x + Max.x) * 0.5f,
        (Min.y + Max.y) * 0.5f,
        (Min.z + Max.z) * 0.5f);

    XMFLOAT3 Extents = XMFLOAT3(
        (Max.x - Min.x) * 0.5f,
        (Max.y - Min.y) * 0.5f,
        (Max.z - Min.z) * 0.5f);

    float Radius = sqrt(Extents.x * Extents.x + Extents.y * Extents.y + Extents.z * Extents.z);
    LocalBSphere.Radius = Radius;
}

void CColliderComponent::SetBCapsule(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
    LocalBCapsule.Center = XMFLOAT3(
        (Min.x + Max.x) * 0.5f,
        (Min.y + Max.y) * 0.5f,
        (Min.z + Max.z) * 0.5f);

    const float dx = Max.x - Min.x;
    const float dy = Max.y - Min.y;
    const float dz = Max.z - Min.z;

    if (dx >= dy && dx >= dz) {
        LocalBCapsule.Height = dx;
        LocalBCapsule.Radius = max(dy, dz) * 0.5f;

        const float halfSegment = max(0.0f, dx * 0.5f - LocalBCapsule.Radius);

        LocalBCapsule.p0 = XMFLOAT3(
            LocalBCapsule.Center.x - halfSegment,
            LocalBCapsule.Center.y,
            LocalBCapsule.Center.z);

        LocalBCapsule.p1 = XMFLOAT3(
            LocalBCapsule.Center.x + halfSegment,
            LocalBCapsule.Center.y,
            LocalBCapsule.Center.z);
        LocalBCapsule.Direction = EDirection::X;
    }
    else if (dy >= dx && dy >= dz) {
        LocalBCapsule.Height = dy;
        LocalBCapsule.Radius = max(dx, dz) * 0.5f;

        const float halfSegment = max(0.0f, dy * 0.5f - LocalBCapsule.Radius);

        LocalBCapsule.p0 = XMFLOAT3(
            LocalBCapsule.Center.x,
            LocalBCapsule.Center.y - halfSegment,
            LocalBCapsule.Center.z);

        LocalBCapsule.p1 = XMFLOAT3(
            LocalBCapsule.Center.x,
            LocalBCapsule.Center.y + halfSegment,
            LocalBCapsule.Center.z);
        LocalBCapsule.Direction = EDirection::Y;
    }
	else {
		LocalBCapsule.Height = dz;
		LocalBCapsule.Radius = max(dx, dy) * 0.5f;

		const float halfSegment = max(0.0f, dz * 0.5f - LocalBCapsule.Radius);

		LocalBCapsule.p0 = XMFLOAT3(
			LocalBCapsule.Center.x,
			LocalBCapsule.Center.y,
			LocalBCapsule.Center.z - halfSegment);

		LocalBCapsule.p1 = XMFLOAT3(
			LocalBCapsule.Center.x,
			LocalBCapsule.Center.y,
			LocalBCapsule.Center.z + halfSegment);

		LocalBCapsule.Direction = EDirection::Z;
	}
   
}

void CColliderComponent::SetSubBCapsule(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
    BoundingCapsule Capsule;
    Capsule.Center = XMFLOAT3(
        (Min.x + Max.x) * 0.5f,
        (Min.y + Max.y) * 0.5f,
        (Min.z + Max.z) * 0.5f);

    const float dx = Max.x - Min.x;
    const float dy = Max.y - Min.y;
    const float dz = Max.z - Min.z;

    if (dx >= dy && dx >= dz) {
        Capsule.Height = dx;
        Capsule.Radius = max(dy, dz) * 0.5f;

        const float halfSegment = max(0.0f, dx * 0.5f - Capsule.Radius);

        Capsule.p0 = XMFLOAT3(
            Capsule.Center.x - halfSegment,
            Capsule.Center.y,
            Capsule.Center.z);

        Capsule.p1 = XMFLOAT3(
            Capsule.Center.x + halfSegment,
            Capsule.Center.y,
            Capsule.Center.z);
        Capsule.Direction = EDirection::X;
    }
    else if (dy >= dx && dy >= dz) {
        Capsule.Height = dy;
        Capsule.Radius = max(dx, dz) * 0.5f;

        const float halfSegment = max(0.0f, dy * 0.5f - Capsule.Radius);

        Capsule.p0 = XMFLOAT3(
            Capsule.Center.x,
            Capsule.Center.y - halfSegment,
            Capsule.Center.z);

        Capsule.p1 = XMFLOAT3(
            Capsule.Center.x,
            Capsule.Center.y + halfSegment,
            Capsule.Center.z);
        Capsule.Direction = EDirection::Y;
    }
	else {
		Capsule.Height = dz;
		Capsule.Radius = max(dx, dy) * 0.5f;

		const float halfSegment = max(0.0f, dz * 0.5f - Capsule.Radius);

		Capsule.p0 = XMFLOAT3(
			Capsule.Center.x,
			Capsule.Center.y,
			Capsule.Center.z - halfSegment);

		Capsule.p1 = XMFLOAT3(
			Capsule.Center.x,
			Capsule.Center.y,
			Capsule.Center.z + halfSegment);

		Capsule.Direction = EDirection::Z;
	}

    LocalSubBCapsules.push_back(Capsule);
}

void CColliderComponent::DisabledRender()
{
	if ( mRender )
		mRender->SetEnabled(false);
}

void CColliderComponent::UpdateWorldBounds()
{
    if (!mTransform) return;

    XMMATRIX S = XMMatrixScaling(mTransform->scale.x, mTransform->scale.y, mTransform->scale.z);
    XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&mTransform->rotation));
    XMMATRIX T = XMMatrixTranslation(mTransform->position.x, mTransform->position.y, mTransform->position.z);

    XMMATRIX W = S * R * T;

    switch (mColliderType)
    {
    case EColliderType::AABB:
    {
        LocalAABB.Transform(WorldAABB, W);
        break;
    }
	case EColliderType::OOBB:
	{
		LocalOOBB.Transform(WorldOOBB, W);

		for ( MeshOOBBSet& set : mMeshOOBBSets )
		{
			set.LocalMeshOOBB.Transform(set.WorldMeshOOBB, W);

			for ( SubMeshOOBBSet& subSet : set.SubMeshes )
			{
				subSet.LocalCoarseOOBB.Transform(subSet.WorldCoarseOOBB, W);

				subSet.WorldRefinedOOBBs.resize(subSet.LocalRefinedOOBBs.size());
				for ( size_t i = 0; i < subSet.LocalRefinedOOBBs.size(); ++i )
				{
					subSet.LocalRefinedOOBBs[i].Transform(subSet.WorldRefinedOOBBs[i], W);
				}
			}
		}

		break;
	}
    case EColliderType::BSphere:
    {
        LocalBSphere.Transform(WorldBSphere, W);
        break;
    }
	case EColliderType::BCapsule:
	{
		LocalBCapsule.Transform(WorldBCapsule, W);

		WorldSubBCapsules.resize(LocalSubBCapsules.size());
		for ( size_t i = 0; i < LocalSubBCapsules.size(); ++i )
		{
			LocalSubBCapsules[i].Transform(WorldSubBCapsules[i], W);
		}

		if ( !mBoneCapsuleLinks.empty() )
		{
			UpdateBoneCapsulesFromCurrentPose();
		}

		break;
	}
    default:
        // None이면 캐시만 리셋하거나 무시
        break;
    }
}

bool CColliderComponent::IntersectsCapsuleHierarchical(const BoundingCapsule& capsule) const
{
	if ( mColliderType != EColliderType::OOBB )
		return false;

	// 1차: 오브젝트 전체 OOBB
	if ( !capsule.Intersects(WorldOOBB) )
		return false;

	// 세부 계층이 없으면 여기서 종료
	if ( mMeshOOBBSets.empty() )
		return true;

	// 2차: mesh 단위 OOBB
	for ( const MeshOOBBSet& set : mMeshOOBBSets )
	{
		if ( !capsule.Intersects(set.WorldMeshOOBB) )
			continue;

		// 3차: submesh 단위
		for ( const SubMeshOOBBSet& subSet : set.SubMeshes )
		{
			// 리포트가 없으면 기존 coarse submesh OOBB 사용
			if ( subSet.useCoarseOOBB )
			{
				if ( capsule.Intersects(subSet.WorldCoarseOOBB) )
					return true;

				continue;
			}

			// 리포트가 있으면 refined OOBB들만 사용
			for ( const BoundingOrientedBox& refinedBox : subSet.WorldRefinedOOBBs )
			{
				if ( capsule.Intersects(refinedBox) )
					return true;
			}
		}
	}

	return false;
}

bool CColliderComponent::IntersectsBoneCapsulesHierarchical(const BoundingOrientedBox& box) const
{
	if ( mColliderType != EColliderType::BCapsule )
		return false;

	if ( !WorldBCapsule.Intersects(box) )
		return false;

	if ( mWorldBoneCapsules.empty() )
		return true;

	for ( const BoundingCapsule& boneCapsule : mWorldBoneCapsules )
	{
		if ( boneCapsule.Intersects(box) )
			return true;
	}

	return false;
}

bool CColliderComponent::IntersectsBoneCapsulesHierarchical(const BoundingCapsule& capsule) const
{
	if ( mColliderType != EColliderType::BCapsule )
		return false;

	if ( !WorldBCapsule.Intersects(capsule) )
		return false;

	if ( mWorldBoneCapsules.empty() )
		return true;

	for ( const BoundingCapsule& boneCapsule : mWorldBoneCapsules )
	{
		if ( boneCapsule.Intersects(capsule) )
			return true;
	}

	return false;
}

void CColliderComponent::BuildBoneCapsulesFromSkeleton()
{
	mBoneCapsuleLinks.clear();
	mWorldBoneCapsules.clear();

	if ( !mModel ) return;

	const std::vector<Bone>& bones = mModel->GetBones();
	if ( bones.empty() ) return;

	std::vector<XMFLOAT4X4> bindGlobal(bones.size());

	for ( size_t i = 0; i < bones.size(); ++i )
	{
		XMMATRIX local = XMLoadFloat4x4(&bones[i].bindLocal);

		if ( bones[i].parentIndex >= 0 )
		{
			XMMATRIX parentGlobal = XMLoadFloat4x4(&bindGlobal[bones[i].parentIndex]);
			XMStoreFloat4x4(&bindGlobal[i], local * parentGlobal);
		}
		else
		{
			XMStoreFloat4x4(&bindGlobal[i], local);
		}
	}

	std::vector<XMFLOAT3> jointBindPositions(bones.size());

	for ( size_t i = 0; i < bones.size(); ++i )
	{
		XMVECTOR pos = XMVector3TransformCoord(
			XMVectorZero(),
			XMLoadFloat4x4(&bindGlobal[i])
		);
		XMStoreFloat3(&jointBindPositions[i], pos);
	}

	for ( size_t child = 0; child < bones.size(); ++child )
	{
		const int parent = bones[child].parentIndex;
		if ( parent < 0 ) continue;

		const XMVECTOR A = XMLoadFloat3(&jointBindPositions[parent]);
		const XMVECTOR B = XMLoadFloat3(&jointBindPositions[child]);

		const float boneLen = XMVectorGetX(XMVector3Length(B - A));
		if ( boneLen <= 1e-4f ) continue;

		float maxRadius = 0.0f;
		int sampleCount = 0;

		for ( const auto& mesh : mModel->GetMeshes() )
		{
			if ( !mesh ) continue;

			for ( const auto& sm : mesh->m_SubMeshes )
			{
				const size_t vcount = sm.positions.size();
				for ( size_t v = 0; v < vcount; ++v )
				{
					if ( v >= sm.boneIndices.size() ) continue;
					if ( v >= sm.boneWeights.size() ) continue;

					const XMUINT4& bi = sm.boneIndices[v];
					const XMFLOAT4& bw = sm.boneWeights[v];

					int dominantBone = ( int ) bi.x;
					float dominantWeight = bw.x;

					if ( bw.y > dominantWeight ) { dominantWeight = bw.y; dominantBone = ( int ) bi.y; }
					if ( bw.z > dominantWeight ) { dominantWeight = bw.z; dominantBone = ( int ) bi.z; }
					if ( bw.w > dominantWeight ) { dominantWeight = bw.w; dominantBone = ( int ) bi.w; }

					if ( dominantBone != parent && dominantBone != ( int ) child )
						continue;

					const XMVECTOR P = XMLoadFloat3(&sm.positions[v]);
					const float distSq = Vector3::distPointToSegment(A, B, P);
					const float dist = sqrtf(distSq);

					if ( dist > maxRadius )
						maxRadius = dist;

					++sampleCount;
				}
			}
		}

		if ( sampleCount == 0 )
			maxRadius = boneLen * 0.15f;

		if ( maxRadius < 0.02f ) maxRadius = 0.02f;
		if ( maxRadius > boneLen * 0.75f ) maxRadius = boneLen * 0.75f;

		BoneCapsuleLink link{};
		link.parentBoneIndex = parent;
		link.childBoneIndex = ( int ) child;
		link.radius = maxRadius;

		mBoneCapsuleLinks.push_back(link);
	}

	mWorldBoneCapsules.resize(mBoneCapsuleLinks.size());
	RebuildWeaponBoneCapsuleSelection();
}

void CColliderComponent::UpdateBoneCapsulesFromCurrentPose()
{
	if ( mBoneCapsuleLinks.empty() ) return;

	CAnimatorComponent* animComp = GetOwner()->GetComponent<CAnimatorComponent>();
	if ( !animComp ) return;

	const std::vector<XMFLOAT4X4>* globalPose = animComp->GetCurrentGlobalPose();
	if ( !globalPose ) return;
	if ( globalPose->empty() ) return;

	const XMMATRIX objectWorld = XMLoadFloat4x4(&mTransform->GetWorldMatrix());

	for ( size_t i = 0; i < mBoneCapsuleLinks.size(); ++i )
	{
		const BoneCapsuleLink& link = mBoneCapsuleLinks[i];

		if ( link.parentBoneIndex < 0 ) continue;
		if ( link.childBoneIndex < 0 ) continue;
		if ( link.parentBoneIndex >= ( int ) globalPose->size() ) continue;
		if ( link.childBoneIndex >= ( int ) globalPose->size() ) continue;

		const XMMATRIX parentGlobal = XMLoadFloat4x4(&( *globalPose )[link.parentBoneIndex]);
		const XMMATRIX childGlobal = XMLoadFloat4x4(&( *globalPose )[link.childBoneIndex]);

		const XMVECTOR P0 = XMVector3TransformCoord(XMVectorZero(), parentGlobal * objectWorld);
		const XMVECTOR P1 = XMVector3TransformCoord(XMVectorZero(), childGlobal * objectWorld);

		XMFLOAT3 p0{};
		XMFLOAT3 p1{};
		XMStoreFloat3(&p0, P0);
		XMStoreFloat3(&p1, P1);

		mWorldBoneCapsules[i] = MakeCapsuleFromSegment(p0, p1, link.radius);
	}

	mWorldWeaponBoneCapsules.resize(mWeaponBoneCapsuleLinkIndices.size());

	for ( size_t i = 0; i < mWeaponBoneCapsuleLinkIndices.size(); ++i )
	{
		const int srcIndex = mWeaponBoneCapsuleLinkIndices[i];
		if ( srcIndex < 0 ) continue;
		if ( srcIndex >= ( int ) mWorldBoneCapsules.size() ) continue;

		mWorldWeaponBoneCapsules[i] = mWorldBoneCapsules[srcIndex];
	}
}

void CColliderComponent::SetWeaponBoneCapsuleRoots(const std::vector<std::string>& rootBoneNames)
{
	mWeaponBoneRootNames = rootBoneNames;
	RebuildWeaponBoneCapsuleSelection();

	if ( !mBoneCapsuleLinks.empty() )
		UpdateBoneCapsulesFromCurrentPose();
}

void CColliderComponent::ClearWeaponBoneCapsuleRoots()
{
	mWeaponBoneRootNames.clear();
	mWeaponBoneCapsuleLinkIndices.clear();
	mWorldWeaponBoneCapsules.clear();
}

void CColliderComponent::RebuildWeaponBoneCapsuleSelection()
{
	mWeaponBoneCapsuleLinkIndices.clear();
	mWorldWeaponBoneCapsules.clear();

	if ( !mModel ) return;

	const std::vector<Bone>& bones = mModel->GetBones();
	if ( bones.empty() ) return;
	if ( mBoneCapsuleLinks.empty() ) return;
	if ( mWeaponBoneRootNames.empty() ) return;

	std::vector<uint8_t> includedBones(bones.size(), 0u);

	auto FindBoneIndexByName = [ &bones ] (const std::string& boneName) -> int
		{
			for ( int i = 0; i < ( int ) bones.size(); ++i )
			{
				if ( bones[i].name == boneName )
					return i;
			}
			return -1;
		};

	for ( const std::string& rootName : mWeaponBoneRootNames )
	{
		const int rootBoneIndex = FindBoneIndexByName(rootName);
		if ( rootBoneIndex < 0 ) continue;

		std::vector<int> stack;
		stack.push_back(rootBoneIndex);

		while ( !stack.empty() )
		{
			const int boneIndex = stack.back();
			stack.pop_back();

			if ( boneIndex < 0 ) continue;
			if ( boneIndex >= ( int ) bones.size() ) continue;
			if ( includedBones[boneIndex] ) continue;

			includedBones[boneIndex] = 1u;

			for ( int i = 0; i < ( int ) bones.size(); ++i )
			{
				if ( bones[i].parentIndex == boneIndex )
					stack.push_back(i);
			}
		}
	}

	for ( size_t i = 0; i < mBoneCapsuleLinks.size(); ++i )
	{
		const BoneCapsuleLink& link = mBoneCapsuleLinks[i];

		if ( link.parentBoneIndex < 0 ) continue;
		if ( link.parentBoneIndex >= ( int ) includedBones.size() ) continue;

		if ( includedBones[link.parentBoneIndex] )
			mWeaponBoneCapsuleLinkIndices.push_back(( int ) i);
	}

	mWorldWeaponBoneCapsules.resize(mWeaponBoneCapsuleLinkIndices.size());
}

bool CColliderComponent::IntersectsActiveWeaponBoneCapsulesAgainstBody(const CColliderComponent& targetBody) const
{
	if ( !mWeaponBoneCapsulesActive )
		return false;

	if ( mWorldWeaponBoneCapsules.empty() )
		return false;

	if ( !targetBody.IsCollisionEnabled() )
		return false;

	for ( const BoundingCapsule& weaponCapsule : mWorldWeaponBoneCapsules )
	{
		switch ( targetBody.GetType() )
		{
		case EColliderType::BCapsule:
			if ( targetBody.IntersectsBoneCapsulesHierarchical(weaponCapsule) )
				return true;
			break;

		case EColliderType::OOBB:
			if ( weaponCapsule.Intersects(targetBody.GetOOBB()) )
				return true;
			break;

		case EColliderType::AABB:
			if ( weaponCapsule.Intersects(targetBody.GetAABB()) )
				return true;
			break;

		case EColliderType::BSphere:
			if ( weaponCapsule.Intersects(targetBody.GetBSphere()) )
				return true;
			break;

		default:
			break;
		}
	}

	return false;
}

void CColliderComponent::OnUpdate(float dt)
{
	UNREFERENCED_PARAMETER(dt);
}

void CColliderComponent::OnLateUpdate(float dt)
{
	UNREFERENCED_PARAMETER(dt);
	UpdateWorldBounds();
}