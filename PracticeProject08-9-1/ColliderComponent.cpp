//-----------------------------------------------------------------------------
// File: ColliderComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "ColliderComponent.h"
#include "Object.h"
#include "AnimatorComponent.h"

#include <unordered_set>
#include <sstream>
#include <iomanip>

namespace
{
	static std::string DebugFloat3(const XMFLOAT3& v)
	{
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(4)
			<< "(" << v.x << ", " << v.y << ", " << v.z << ")";
		return oss.str();
	}

	static std::string DebugFloat4(const XMFLOAT4& v)
	{
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(6)
			<< "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
		return oss.str();
	}

	static void DebugPrintLine(const std::string& text)
	{
		OutputDebugStringA(text.c_str());
		OutputDebugStringA("\n");
	}

	static void DebugPrintOOBBLine(
		const char* tag,
		const std::string& assetName,
		const std::string& objectName,
		const std::string& meshName,
		const std::string& authoringPath,
		size_t meshSetIndex,
		size_t subIndex,
		const BoundingOrientedBox& box)
	{
		std::ostringstream oss;
		oss << "[ColliderBuild][" << tag << "] "
			<< "asset=\"" << assetName << "\" "
			<< "object=\"" << objectName << "\" "
			<< "meshSet=" << meshSetIndex << " "
			<< "sub=" << subIndex << " "
			<< "mesh=\"" << meshName << "\" "
			<< "authoringPath=\"" << authoringPath << "\" "
			<< "center=" << DebugFloat3(box.Center) << " "
			<< "extents=" << DebugFloat3(box.Extents) << " "
			<< "size=" << DebugFloat3(XMFLOAT3(
				box.Extents.x * 2.0f,
				box.Extents.y * 2.0f,
				box.Extents.z * 2.0f)) << " "
			<< "rot=" << DebugFloat4(box.Orientation);

		DebugPrintLine(oss.str());
	}

	static void DebugPrintMessage(
		const char* tag,
		const std::string& assetName,
		const std::string& objectName,
		const std::string& message)
	{
		std::ostringstream oss;
		oss << "[ColliderBuild][" << tag << "] "
			<< "asset=\"" << assetName << "\" "
			<< "object=\"" << objectName << "\" "
			<< message;

		DebugPrintLine(oss.str());
	}
}

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

void CColliderComponent::SetColliderBuildLogEnabled(
	bool enabled,
	const std::string& assetName,
	const std::string& objectName)
{
	mDebugColliderBuildLogEnabled = enabled;
	mDebugColliderAssetName = assetName;
	mDebugColliderObjectName = objectName;
}

BoundingOrientedBox CColliderComponent::MakeAuthoredLocalOOBB(
	const XMFLOAT3& Center,
	const XMFLOAT4& RotationQuat,
	const XMFLOAT3& Size)
{
	BoundingOrientedBox box{};

	box.Center = Center;
	box.Extents = XMFLOAT3(
		Size.x * 0.5f,
		Size.y * 0.5f,
		Size.z * 0.5f
	);

	const XMVECTOR q = XMQuaternionNormalize(XMLoadFloat4(&RotationQuat));
	XMStoreFloat4(&box.Orientation, q);
	return box;
}

BoundingOrientedBox CColliderComponent::MakeLocalOOBBFromMatrix(const XMFLOAT4X4& unitBoxToLocal)
{
	BoundingOrientedBox box{};

	box.Center = XMFLOAT3(
		unitBoxToLocal._41,
		unitBoxToLocal._42,
		unitBoxToLocal._43
	);

	const XMVECTOR rawX = XMVectorSet(unitBoxToLocal._11, unitBoxToLocal._12, unitBoxToLocal._13, 0.0f);
	const XMVECTOR rawY = XMVectorSet(unitBoxToLocal._21, unitBoxToLocal._22, unitBoxToLocal._23, 0.0f);
	const XMVECTOR rawZ = XMVectorSet(unitBoxToLocal._31, unitBoxToLocal._32, unitBoxToLocal._33, 0.0f);

	const float sizeX = XMVectorGetX(XMVector3Length(rawX));
	const float sizeY = XMVectorGetX(XMVector3Length(rawY));
	const float sizeZ = XMVectorGetX(XMVector3Length(rawZ));

	box.Extents = XMFLOAT3(
		sizeX * 0.5f,
		sizeY * 0.5f,
		sizeZ * 0.5f
	);

	const float eps = 1e-6f;

	auto NormalizeOr = [ ] (FXMVECTOR v, FXMVECTOR fallback) -> XMVECTOR
		{
			const float lenSq = XMVectorGetX(XMVector3LengthSq(v));
			if ( lenSq > 1e-12f )
				return XMVector3Normalize(v);
			return fallback;
		};

	const bool hasX = ( sizeX > eps );
	const bool hasY = ( sizeY > eps );
	const bool hasZ = ( sizeZ > eps );

	XMVECTOR axisX = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR axisY = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR axisZ = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	// 1) 일반적인 입체 서브메시
	if ( hasX && hasY && hasZ )
	{
		axisX = NormalizeOr(rawX, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));

		XMVECTOR yOrtho = rawY - XMVectorScale(axisX, XMVectorGetX(XMVector3Dot(rawY, axisX)));
		axisY = NormalizeOr(yOrtho, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

		axisZ = NormalizeOr(XMVector3Cross(axisX, axisY), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));

		// rawZ 방향과 반대면 뒤집기
		if ( XMVectorGetX(XMVector3Dot(axisZ, rawZ)) < 0.0f )
		{
			axisY = XMVectorNegate(axisY);
			axisZ = XMVectorNegate(axisZ);
		}

		axisY = NormalizeOr(XMVector3Cross(axisZ, axisX), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	}
	// 2) XY 평면류: Z 두께만 0
	else if ( hasX && hasY )
	{
		axisX = NormalizeOr(rawX, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));

		XMVECTOR yOrtho = rawY - XMVectorScale(axisX, XMVectorGetX(XMVector3Dot(rawY, axisX)));
		axisY = NormalizeOr(yOrtho, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

		axisZ = NormalizeOr(XMVector3Cross(axisX, axisY), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
		axisY = NormalizeOr(XMVector3Cross(axisZ, axisX), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	}
	// 3) YZ 평면류: X 두께만 0
	else if ( hasY && hasZ )
	{
		axisY = NormalizeOr(rawY, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

		XMVECTOR zOrtho = rawZ - XMVectorScale(axisY, XMVectorGetX(XMVector3Dot(rawZ, axisY)));
		axisZ = NormalizeOr(zOrtho, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));

		axisX = NormalizeOr(XMVector3Cross(axisY, axisZ), XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
		axisZ = NormalizeOr(XMVector3Cross(axisX, axisY), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
	}
	// 4) XZ 평면류: Y 두께만 0
	else if ( hasX && hasZ )
	{
		axisX = NormalizeOr(rawX, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));

		XMVECTOR zOrtho = rawZ - XMVectorScale(axisX, XMVectorGetX(XMVector3Dot(rawZ, axisX)));
		axisZ = NormalizeOr(zOrtho, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));

		axisY = NormalizeOr(XMVector3Cross(axisZ, axisX), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		axisZ = NormalizeOr(XMVector3Cross(axisX, axisY), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
	}
	// 5) 선분/점 수준의 완전 퇴화 케이스 fallback
	else if ( hasX )
	{
		axisX = NormalizeOr(rawX, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));

		XMVECTOR tmp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		if ( fabsf(XMVectorGetX(XMVector3Dot(axisX, tmp))) > 0.99f )
			tmp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

		axisZ = NormalizeOr(XMVector3Cross(axisX, tmp), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
		axisY = NormalizeOr(XMVector3Cross(axisZ, axisX), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	}
	else if ( hasY )
	{
		axisY = NormalizeOr(rawY, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

		XMVECTOR tmp = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		if ( fabsf(XMVectorGetX(XMVector3Dot(axisY, tmp))) > 0.99f )
			tmp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

		axisX = NormalizeOr(XMVector3Cross(axisY, tmp), XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
		axisZ = NormalizeOr(XMVector3Cross(axisX, axisY), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
	}
	else if ( hasZ )
	{
		axisZ = NormalizeOr(rawZ, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));

		XMVECTOR tmp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		if ( fabsf(XMVectorGetX(XMVector3Dot(axisZ, tmp))) > 0.99f )
			tmp = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

		axisX = NormalizeOr(XMVector3Cross(tmp, axisZ), XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
		axisY = NormalizeOr(XMVector3Cross(axisZ, axisX), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	}
	else
	{
		box.Orientation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		return box;
	}

	const XMMATRIX rotM(
		axisX,
		axisY,
		axisZ,
		XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)
	);

	const XMVECTOR q = XMQuaternionNormalize(XMQuaternionRotationMatrix(rotM));
	XMStoreFloat4(&box.Orientation, q);

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

static void ExpandMinMaxByPoint(
	XMFLOAT3& minPt,
	XMFLOAT3& maxPt,
	const XMFLOAT3& p)
{
	minPt.x = min(minPt.x, p.x);
	minPt.y = min(minPt.y, p.y);
	minPt.z = min(minPt.z, p.z);

	maxPt.x = max(maxPt.x, p.x);
	maxPt.y = max(maxPt.y, p.y);
	maxPt.z = max(maxPt.z, p.z);
}

static void ExpandMinMaxByOOBB(
	XMFLOAT3& minPt,
	XMFLOAT3& maxPt,
	const BoundingOrientedBox& box)
{
	XMFLOAT3 corners[8]{};
	box.GetCorners(corners);

	for ( int i = 0; i < 8; ++i )
		ExpandMinMaxByPoint(minPt, maxPt, corners[i]);
}

static BoundingOrientedBox MakeEnclosingLocalOOBB(
	const std::vector<BoundingOrientedBox>& boxes)
{
	BoundingOrientedBox result{};

	if ( boxes.empty() )
	{
		result.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
		result.Extents = XMFLOAT3(0.0f, 0.0f, 0.0f);
		result.Orientation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		return result;
	}

	XMFLOAT3 minPt(FLT_MAX, FLT_MAX, FLT_MAX);
	XMFLOAT3 maxPt(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for ( const BoundingOrientedBox& box : boxes )
		ExpandMinMaxByOOBB(minPt, maxPt, box);

	result.Center = XMFLOAT3(
		( minPt.x + maxPt.x ) * 0.5f,
		( minPt.y + maxPt.y ) * 0.5f,
		( minPt.z + maxPt.z ) * 0.5f
	);

	result.Extents = XMFLOAT3(
		( maxPt.x - minPt.x ) * 0.5f,
		( maxPt.y - minPt.y ) * 0.5f,
		( maxPt.z - minPt.z ) * 0.5f
	);

	result.Orientation = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	return result;
}

void CColliderComponent::BuildHierarchicalOOBBs(const vector<shared_ptr<CMesh>>& meshes)
{
	mMeshOOBBSets.clear();
	mMeshOOBBSets.reserve(meshes.size());

	const bool logEnabled = mDebugColliderBuildLogEnabled;

	if ( logEnabled )
	{
		DebugPrintMessage(
			"BEGIN",
			mDebugColliderAssetName,
			mDebugColliderObjectName,
			" BuildHierarchicalOOBBs begin"
		);

		std::ostringstream oss;
		oss << " meshCount=" << meshes.size()
			<< " authoredPathGroupCount=" << mStaticSubMeshAuthoredOOBBs.size();

		DebugPrintMessage(
			"INPUT",
			mDebugColliderAssetName,
			mDebugColliderObjectName,
			oss.str()
		);
	}

	for ( size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex )
	{
		const shared_ptr<CMesh>& mesh = meshes[meshIndex];
		if ( !mesh )
			continue;

		MeshOOBBSet set{};
		std::unordered_set<std::string> consumedAuthoredPaths;

		if ( logEnabled )
		{
			std::ostringstream oss;
			oss << " meshSet=" << meshIndex
				<< " sourceMeshPath=\"" << mesh->GetSourceMeshPath() << "\""
				<< " subMeshCount=" << mesh->m_SubMeshes.size()
				<< " meshMin=" << DebugFloat3(mesh->GetMeshMin())
				<< " meshMax=" << DebugFloat3(mesh->GetMeshMax());

			DebugPrintMessage(
				"MESH_BEGIN",
				mDebugColliderAssetName,
				mDebugColliderObjectName,
				oss.str()
			);
		}

		for ( size_t subMeshIndex = 0; subMeshIndex < mesh->m_SubMeshes.size(); ++subMeshIndex )
		{
			const auto& submesh = mesh->m_SubMeshes[subMeshIndex];

			if ( submesh.isColliderHelper )
			{
				if ( logEnabled )
				{
					std::ostringstream oss;
					oss << " meshSet=" << meshIndex
						<< " subMesh=" << subMeshIndex
						<< " mesh=\"" << submesh.meshName << "\""
						<< " authoringPath=\"" << submesh.authoringPath << "\"";

					DebugPrintMessage(
						"SKIP_COLLIDER_HELPER",
						mDebugColliderAssetName,
						mDebugColliderObjectName,
						oss.str()
					);
				}
				continue;
			}

			const bool hasAuthoringPath = !submesh.authoringPath.empty();
			const auto authoredIt =
				hasAuthoringPath
				? mStaticSubMeshAuthoredOOBBs.find(submesh.authoringPath)
				: mStaticSubMeshAuthoredOOBBs.end();

			if ( logEnabled )
			{
				std::ostringstream oss;
				oss << " meshSet=" << meshIndex
					<< " subMesh=" << subMeshIndex
					<< " mesh=\"" << submesh.meshName << "\""
					<< " authoringPath=\"" << submesh.authoringPath << "\""
					<< " hasAuthoringPath=" << ( hasAuthoringPath ? 1 : 0 )
					<< " authoredMatch=" << ( authoredIt != mStaticSubMeshAuthoredOOBBs.end() ? 1 : 0 )
					<< " authoredBoxCount=" << (
						authoredIt != mStaticSubMeshAuthoredOOBBs.end()
						? authoredIt->second.size()
						: 0 )
					<< " hasBinExplicitLocalOOBB=" << ( submesh.hasExplicitLocalOOBB ? 1 : 0 )
					<< " subMin=" << DebugFloat3(submesh.subMeshMin)
					<< " subMax=" << DebugFloat3(submesh.subMeshMax);

				DebugPrintMessage(
					"SUBMESH_CHECK",
					mDebugColliderAssetName,
					mDebugColliderObjectName,
					oss.str()
				);
			}

			if ( authoredIt != mStaticSubMeshAuthoredOOBBs.end() && !authoredIt->second.empty() )
			{
				const bool firstUseOfThisPath =
					consumedAuthoredPaths.insert(submesh.authoringPath).second;

				if ( firstUseOfThisPath )
				{
					for ( size_t authoredIndex = 0; authoredIndex < authoredIt->second.size(); ++authoredIndex )
					{
						const AuthoredSubMeshOOBB& authoredBox = authoredIt->second[authoredIndex];

						BoundingOrientedBox subBox = MakeAuthoredLocalOOBB(
							authoredBox.Center,
							authoredBox.RotationQuat,
							authoredBox.Size
						);

						const size_t createdSubIndex = set.LocalSubOOBBs.size();
						set.LocalSubOOBBs.push_back(subBox);

						if ( logEnabled )
						{
							DebugPrintOOBBLine(
								"DETAIL_FROM_AUTHORED_REPORT",
								mDebugColliderAssetName,
								mDebugColliderObjectName,
								submesh.meshName,
								submesh.authoringPath,
								meshIndex,
								createdSubIndex,
								subBox
							);
						}
					}
				}
				else if ( logEnabled )
				{
					std::ostringstream oss;
					oss << " meshSet=" << meshIndex
						<< " subMesh=" << subMeshIndex
						<< " mesh=\"" << submesh.meshName << "\""
						<< " authoringPath=\"" << submesh.authoringPath << "\""
						<< " reason=authoringPathAlreadyConsumed";

					DebugPrintMessage(
						"AUTHORED_DUPLICATE_PATH_SKIP",
						mDebugColliderAssetName,
						mDebugColliderObjectName,
						oss.str()
					);
				}

				continue;
			}

			if ( submesh.hasExplicitLocalOOBB )
			{
				BoundingOrientedBox subBox = MakeLocalOOBBFromMatrix(submesh.explicitLocalOOBBMatrix);

				const size_t createdSubIndex = set.LocalSubOOBBs.size();
				set.LocalSubOOBBs.push_back(subBox);

				if ( logEnabled )
				{
					DebugPrintOOBBLine(
						"DETAIL_FROM_BIN_EXPLICIT_LOCAL_OOBB",
						mDebugColliderAssetName,
						mDebugColliderObjectName,
						submesh.meshName,
						submesh.authoringPath,
						meshIndex,
						createdSubIndex,
						subBox
					);
				}
			}
			else
			{
				BoundingOrientedBox subBox = MakeLocalOOBB(submesh.subMeshMin, submesh.subMeshMax);

				const size_t createdSubIndex = set.LocalSubOOBBs.size();
				set.LocalSubOOBBs.push_back(subBox);

				if ( logEnabled )
				{
					DebugPrintOOBBLine(
						"DETAIL_FROM_AUTO_SUBMESH_BOUNDS",
						mDebugColliderAssetName,
						mDebugColliderObjectName,
						submesh.meshName,
						submesh.authoringPath,
						meshIndex,
						createdSubIndex,
						subBox
					);
				}
			}
		}

		const XMFLOAT3 meshMin = mesh->GetMeshMin();
		const XMFLOAT3 meshMax = mesh->GetMeshMax();

		// 대표용 mesh OOBB는 항상 "실제 렌더 메시 전체 크기" 기준으로 만든다.
		// authored sub OOBB / explicit sub OOBB는 세부 충돌용으로만 유지한다.
		if ( meshMin.x <= meshMax.x &&
			 meshMin.y <= meshMax.y &&
			 meshMin.z <= meshMax.z )
		{
			set.LocalMeshOOBB = MakeLocalOOBB(meshMin, meshMax);

			if ( logEnabled )
			{
				DebugPrintOOBBLine(
					"MESH_REP_FROM_RENDER_MESH_BOUNDS",
					mDebugColliderAssetName,
					mDebugColliderObjectName,
					"",
					"",
					meshIndex,
					0,
					set.LocalMeshOOBB
				);
			}
		}
		else if ( !set.LocalSubOOBBs.empty() )
		{
			// 예외 fallback: 메시 bounds가 비정상이면 기존처럼 sub OOBB 외접 사용
			set.LocalMeshOOBB = MakeEnclosingLocalOOBB(set.LocalSubOOBBs);

			if ( logEnabled )
			{
				DebugPrintOOBBLine(
					"MESH_REP_FROM_ENCLOSING_SUB_OOBBS",
					mDebugColliderAssetName,
					mDebugColliderObjectName,
					"",
					"",
					meshIndex,
					0,
					set.LocalMeshOOBB
				);
			}
		}
		else
		{
			if ( logEnabled )
			{
				std::ostringstream oss;
				oss << " meshSet=" << meshIndex
					<< " reason=noMeshBoundsAndNoSubOOBBs";

				DebugPrintMessage(
					"MESH_SKIPPED",
					mDebugColliderAssetName,
					mDebugColliderObjectName,
					oss.str()
				);
			}
			continue;
		}

		set.WorldMeshOOBB = set.LocalMeshOOBB;
		set.WorldSubOOBBs = set.LocalSubOOBBs;

		if ( logEnabled )
		{
			std::ostringstream oss;
			oss << " meshSet=" << meshIndex
				<< " localSubOOBBCount=" << set.LocalSubOOBBs.size();

			DebugPrintMessage(
				"MESH_END",
				mDebugColliderAssetName,
				mDebugColliderObjectName,
				oss.str()
			);
		}

		mMeshOOBBSets.push_back(std::move(set));
	}

	if ( logEnabled )
	{
		size_t totalSubOOBBs = 0;
		for ( const MeshOOBBSet& set : mMeshOOBBSets )
			totalSubOOBBs += set.LocalSubOOBBs.size();

		std::ostringstream oss;
		oss << " meshSetCount=" << mMeshOOBBSets.size()
			<< " totalLocalSubOOBBCount=" << totalSubOOBBs;

		DebugPrintMessage(
			"END",
			mDebugColliderAssetName,
			mDebugColliderObjectName,
			oss.str()
		);
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
	{
		bool hasAnySubMesh = false;

		for ( const shared_ptr<CMesh>& mesh : meshes )
		{
			if ( !mesh ) continue;

			for ( const auto& submesh : mesh->m_SubMeshes )
			{
				if ( submesh.isColliderHelper )
					continue;

				objMin.x = min(objMin.x, submesh.subMeshMin.x);
				objMin.y = min(objMin.y, submesh.subMeshMin.y);
				objMin.z = min(objMin.z, submesh.subMeshMin.z);

				objMax.x = max(objMax.x, submesh.subMeshMax.x);
				objMax.y = max(objMax.y, submesh.subMeshMax.y);
				objMax.z = max(objMax.z, submesh.subMeshMax.z);

				hasAnySubMesh = true;
			}
		}

		if ( hasAnySubMesh )
			SetAABB(objMin, objMax);

		break;
	}
	case EColliderType::OOBB:
	{
		BuildHierarchicalOOBBs(meshes);

		std::vector<BoundingOrientedBox> meshBoxes;
		meshBoxes.reserve(mMeshOOBBSets.size());

		for ( const MeshOOBBSet& set : mMeshOOBBSets )
		{
			if ( set.LocalMeshOOBB.Extents.x <= 1e-6f &&
				 set.LocalMeshOOBB.Extents.y <= 1e-6f &&
				 set.LocalMeshOOBB.Extents.z <= 1e-6f )
			{
				continue;
			}

			meshBoxes.push_back(set.LocalMeshOOBB);
		}

		if ( !meshBoxes.empty() )
		{
			LocalOOBB = MakeEnclosingLocalOOBB(meshBoxes);
		}
		else
		{
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
		}

		break;
	}
    case EColliderType::BSphere:

        break;
	case EColliderType::BCapsule:
	{
		LocalSubBCapsules.clear();

		bool hasAnySubMesh = false;

		for ( const shared_ptr<CMesh>& mesh : meshes )
		{
			if ( !mesh ) continue;

			for ( const auto& submesh : mesh->m_SubMeshes )
			{
				if ( submesh.isColliderHelper )
					continue;

				objMin.x = min(objMin.x, submesh.subMeshMin.x);
				objMin.y = min(objMin.y, submesh.subMeshMin.y);
				objMin.z = min(objMin.z, submesh.subMeshMin.z);

				objMax.x = max(objMax.x, submesh.subMeshMax.x);
				objMax.y = max(objMax.y, submesh.subMeshMax.y);
				objMax.z = max(objMax.z, submesh.subMeshMax.z);

				SetSubBCapsule(submesh.subMeshMin, submesh.subMeshMax);
				hasAnySubMesh = true;
			}
		}

		if ( hasAnySubMesh )
			SetBCapsule(objMin, objMax);

		break;
	}
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

	if ( mDebugColliderBuildLogEnabled && mColliderType == EColliderType::OOBB )
	{
		size_t totalSubOOBBs = 0;
		for ( const MeshOOBBSet& set : mMeshOOBBSets )
			totalSubOOBBs += set.LocalSubOOBBs.size();

		std::ostringstream oss;
		oss << " finalLocalOOBB.center=" << DebugFloat3(LocalOOBB.Center)
			<< " finalLocalOOBB.extents=" << DebugFloat3(LocalOOBB.Extents)
			<< " finalLocalOOBB.size=" << DebugFloat3(XMFLOAT3(
				LocalOOBB.Extents.x * 2.0f,
				LocalOOBB.Extents.y * 2.0f,
				LocalOOBB.Extents.z * 2.0f))
			<< " meshSetCount=" << mMeshOOBBSets.size()
			<< " totalLocalSubOOBBCount=" << totalSubOOBBs;

		DebugPrintMessage(
			"FINAL",
			mDebugColliderAssetName,
			mDebugColliderObjectName,
			oss.str()
		);
	}
}

void CColliderComponent::SetAABB(const XMFLOAT3& Min, const XMFLOAT3& Max)
{
    XMFLOAT3 Center = XMFLOAT3(
        (Min.x + Max.x) * 0.5f,
        (Min.y + Max.y) * 0.5f,
        (Min.z + Max.z) * 0.5f);

	XMFLOAT3 Extents = XMFLOAT3(
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

			set.WorldSubOOBBs.resize(set.LocalSubOOBBs.size());
			for ( size_t i = 0; i < set.LocalSubOOBBs.size(); ++i )
			{
				set.LocalSubOOBBs[i].Transform(set.WorldSubOOBBs[i], W);
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

	// 계층형 데이터가 없으면 기존 단일 OOBB로 fallback
	if ( mMeshOOBBSets.empty() )
		return capsule.Intersects(WorldOOBB);

	for ( const MeshOOBBSet& set : mMeshOOBBSets )
	{
		// 1차: mesh 단위 OOBB
		if ( !capsule.Intersects(set.WorldMeshOOBB) )
			continue;

		// submesh가 없으면 mesh hit만으로도 true
		if ( set.WorldSubOOBBs.empty() )
			return true;

		// 2차: submesh 단위 OOBB
		for ( const BoundingOrientedBox& subBox : set.WorldSubOOBBs )
		{
			if ( capsule.Intersects(subBox) )
				return true;
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
					const float distSq = Collide::distPointToSegment(A, B, P);
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