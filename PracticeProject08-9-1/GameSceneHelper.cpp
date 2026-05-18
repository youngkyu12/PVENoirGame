//-----------------------------------------------------------------------------
// File: GameSceneHelper.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "GameSceneHelper.h"

namespace GameSceneHelper
{
	void ConfigureProjectileCollider(CColliderComponent* collider, bool firedByPlayer)
	{
		if ( !collider )
			return;

		if ( firedByPlayer )
		{
			collider->SetLayer(kCollisionLayerPlayerWeapon);
			collider->SetMask(CollisionBit(kCollisionLayerMonster));
		}
		else
		{
			collider->SetLayer(kCollisionLayerMonsterWeapon);
			collider->SetMask(CollisionBit(kCollisionLayerPlayer));
		}
	}

	DecodedAnimStateCode DecodeStateCode(uint32_t stateCode)
	{
		DecodedAnimStateCode out{};

		// [die][hit][run][roll][attack][up][down][left][right][move]
		// move를 bit0(LSB)로 가정
		if ( ( stateCode & ( 1u << 0 ) ) != 0u )
		{
			if ( stateCode & ( 1u << 4 ) )
				out.moveDirBits |= DIR_FORWARD;

			if ( stateCode & ( 1u << 3 ) )
				out.moveDirBits |= DIR_BACKWARD;

			if ( stateCode & ( 1u << 2 ) )
				out.moveDirBits |= DIR_LEFT;

			if ( stateCode & ( 1u << 1 ) )
				out.moveDirBits |= DIR_RIGHT;

			// 규칙:
			// isMove=1이어도 방향 4비트가 모두 0이면 Move/Run 아님
			out.hasMove = ( out.moveDirBits != 0 );
			out.run = out.hasMove && ( ( stateCode & ( 1u << 7 ) ) != 0u );
		}

		// 우선순위: die > hit > roll > attack
		out.die = ( stateCode & ( 1u << 9 ) ) != 0u;
		out.hit = !out.die && ( ( stateCode & ( 1u << 8 ) ) != 0u );
		out.roll = !out.die && !out.hit && ( ( stateCode & ( 1u << 6 ) ) != 0u );
		out.attack = !out.die && !out.hit && !out.roll && ( ( stateCode & ( 1u << 5 ) ) != 0u );

		return out;
	}

	XMFLOAT4X4 BuildWorldMatrixFromOOBB(const BoundingOrientedBox& box)
	{
		XMFLOAT4X4 out{};

		const XMMATRIX S = XMMatrixScaling(
			box.Extents.x * 2.0f,
			box.Extents.y * 2.0f,
			box.Extents.z * 2.0f
		);

		const XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&box.Orientation));

		const XMMATRIX T = XMMatrixTranslation(
			box.Center.x,
			box.Center.y,
			box.Center.z
		);

		XMStoreFloat4x4(&out, S * R * T);
		return out;
	}

	XMFLOAT4X4 BuildIdentityMatrix4x4()
	{
		XMFLOAT4X4 out{};
		XMStoreFloat4x4(&out, XMMatrixIdentity());
		return out;
	}

	bool IsTreeCullBlockerAssetName(const std::string& assetName)
	{
		return
			( assetName == "VillageWall" ) ||
			( assetName == "Castle" ) ||
			( assetName == "Tower" ) ||
			( assetName == "Building1" ) ||
			( assetName == "Building2" ) ||
			( assetName == "Building3" ) ||
			( assetName == "Building4" ) ||
			( assetName == "Building5" ) ||
			( assetName == "Building6" ) ||
			( assetName == "Building7" ) ||
			( assetName == "Building8" ) ||
			( assetName == "Building9" );
	}

	bool ShouldStaticPlacementCastShadow(const std::string& assetName)
	{
		if ( assetName == "Ground" )
			return false;

		if ( assetName == "DirtRoad" )
			return false;

		if ( assetName == "Grass" )
			return false;

		return true;
	}

	void SetObjectAttackPower(CGameObject* obj, int attackPower)
	{
		if ( !obj )
			return;

		if ( auto* attack = obj->GetComponent<CAttackPowerComponent>() )
			attack->SetAttackPower(attackPower);
	}

	float DistanceSqXZ(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;

		return dx * dx + dz * dz;
	}

	void StoreStaticWorldRows(
		XMFLOAT4& out0,
		XMFLOAT4& out1,
		XMFLOAT4& out2,
		XMFLOAT4& out3,
		const XMFLOAT4X4& W)
	{
		out0 = XMFLOAT4(W._11, W._12, W._13, W._14);
		out1 = XMFLOAT4(W._21, W._22, W._23, W._24);
		out2 = XMFLOAT4(W._31, W._32, W._33, W._34);
		out3 = XMFLOAT4(W._41, W._42, W._43, W._44);
	}

	XMFLOAT3 GetSafeObjectForward(const CGameObject* obj)
	{
		if ( !obj )
			return XMFLOAT3(0.0f, 0.0f, 1.0f);

		const XMFLOAT4X4& W = obj->GetWorldMatrix();

		XMFLOAT3 dir = XMFLOAT3(W._31, W._32, W._33);
		XMVECTOR dirV = XMLoadFloat3(&dir);

		if ( XMVectorGetX(XMVector3LengthSq(dirV)) <= 1.0e-8f )
			return XMFLOAT3(0.0f, 0.0f, 1.0f);

		dirV = XMVector3Normalize(dirV);

		XMFLOAT3 out{};
		XMStoreFloat3(&out, dirV);
		return out;
	}

	bool IsWalkClipName(const std::string& clipName)
	{
		return clipName.rfind("Walk_", 0) == 0;
	}

	bool IsRunClipName(const std::string& clipName)
	{
		return clipName.rfind("Run_", 0) == 0;
	}

	int GetFootstepModeFromClipName(const std::string& clipName)
	{
		if ( IsWalkClipName(clipName) )
			return 1;

		if ( IsRunClipName(clipName) )
			return 2;

		return 0;
	}

	bool CrossedNormalizedEvent(
		float prevNormalized,
		float curNormalized,
		float eventNormalized)
	{
		if ( prevNormalized < 0.0f )
			prevNormalized = 0.0f;

		if ( prevNormalized > 1.0f )
			prevNormalized = 1.0f;

		if ( curNormalized < 0.0f )
			curNormalized = 0.0f;

		if ( curNormalized > 1.0f )
			curNormalized = 1.0f;

		// 일반 진행
		if ( curNormalized >= prevNormalized )
			return prevNormalized < eventNormalized && eventNormalized <= curNormalized;

		// 루프 wrap: 0.95 -> 0.05 같은 경우
		return eventNormalized > prevNormalized || eventNormalized <= curNormalized;
	}

	const char* SelectRandomFootstepGrassSfxPath()
	{
		static std::mt19937 rng{ std::random_device{}( ) };
		static std::uniform_int_distribution<int> dist(1, 3);

		switch ( dist(rng) )
		{
		case 1:
			return "Assets/Audio/Walk_Grass1.wav";
		case 2:
			return "Assets/Audio/Walk_Grass2.wav";
		case 3:
			return "Assets/Audio/Walk_Grass3.wav";
		default:
			break;
		}

		return "Assets/Audio/Walk_Grass1.wav";
	}

	const char* SelectRandomFootstepBlockSfxPath()
	{
		static std::mt19937 rng{ std::random_device{}( ) };
		static std::uniform_int_distribution<int> dist(1, 3);

		switch ( dist(rng) )
		{
		case 1:
			return "Assets/Audio/Walk_Block1.wav";
		case 2:
			return "Assets/Audio/Walk_Block2.wav";
		case 3:
			return "Assets/Audio/Walk_Block3.wav";
		default:
			break;
		}

		return "Assets/Audio/Walk_Block1.wav";
	}

	bool ParsePlacementEntryLine(const std::string& line, StaticPlacementEntry& outEntry)
	{
		char asset[64] = {};
		char objectName[128] = {};

		float px = 0.0f;
		float py = 0.0f;
		float pz = 0.0f;

		float qx = 0.0f;
		float qy = 0.0f;
		float qz = 0.0f;
		float qw = 1.0f;

		const int matched = sscanf_s(
			line.c_str(),
			"ENTRY|asset=\"%63[^\"]\"|object=\"%127[^\"]\"|pos=(%f,%f,%f)|rot=(%f,%f,%f,%f)",
			asset,
			static_cast< unsigned >( _countof(asset) ),
			objectName,
			static_cast< unsigned >( _countof(objectName) ),
			&px,
			&py,
			&pz,
			&qx,
			&qy,
			&qz,
			&qw
		);

		if ( matched != 9 )
			return false;

		outEntry.assetName = asset;
		outEntry.objectName = objectName;
		outEntry.pos = XMFLOAT3(px, py, pz);
		outEntry.rot = XMFLOAT4(qx, qy, qz, qw);
		return true;
	}

	std::string TrimString(const std::string& text)
	{
		size_t begin = 0;

		while ( begin < text.size() &&
			std::isspace(static_cast< unsigned char >(text[begin])) )
		{
			++begin;
		}

		size_t end = text.size();

		while ( end > begin &&
			std::isspace(static_cast< unsigned char >( text[end - 1] )) )
		{
			--end;
		}

		return text.substr(begin, end - begin);
	}

	std::string StripTopRootFromAPath(const std::string& fullAPath)
	{
		const size_t firstSlash = fullAPath.find('/');

		if ( firstSlash == std::string::npos )
			return fullAPath;

		return fullAPath.substr(firstSlash + 1);
	}

	bool ParseVector3Tuple(const std::string& text, XMFLOAT3& outValue)
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;

		if ( sscanf_s(text.c_str(), "(%f, %f, %f)", &x, &y, &z) != 3 )
			return false;

		outValue = XMFLOAT3(x, y, z);
		return true;
	}

	bool ParseVector4Tuple(const std::string& text, XMFLOAT4& outValue)
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 1.0f;

		if ( sscanf_s(text.c_str(), "(%f, %f, %f, %f)", &x, &y, &z, &w) != 4 )
			return false;

		outValue = XMFLOAT4(x, y, z, w);
		return true;
	}

	std::string ToLowerAscii(const std::string& text)
	{
		std::string out = text;

		std::transform(
			out.begin(),
			out.end(),
			out.begin(),
			[ ] (unsigned char c)
			{
				return static_cast< char >( std::tolower(c) );
			}
		);

		return out;
	}

#ifndef USING_NETWORK
	std::string NormalizeTowerDoorNameForMatch(const std::string& text)
	{
		std::string out;
		out.reserve(text.size());

		for ( unsigned char c : text )
		{
			if ( c == ' ' ||
				c == '_' ||
				c == '-' ||
				c == '[' ||
				c == ']' ||
				c == '/' ||
				c == '|' )
			{
				continue;
			}

			out.push_back(static_cast< char >( std::tolower(c) ));
		}

		return out;
	}

	float NormalizeYawDegrees180(float yaw)
	{
		while ( yaw > 180.0f )
			yaw -= 360.0f;

		while ( yaw <= -180.0f )
			yaw += 360.0f;

		return yaw;
	}

	bool IsTowerDoorFrame2Name(const std::string& meshName, const std::string& authoringPath)
	{
		UNREFERENCED_PARAMETER(authoringPath);

		const std::string name = NormalizeTowerDoorNameForMatch(meshName);
		return name == "doubledoorframe2";
	}

	int GetCastleDoorFrameIndexFromMeshName(const std::string& meshName)
	{
		const std::string name = NormalizeTowerDoorNameForMatch(meshName);

		static constexpr const char* kPrefix = "doubledoorframe";
		const std::string prefix = kPrefix;

		if ( name == prefix )
			return 0; // Unity: Double Door Frame

		if ( name.rfind(prefix, 0) != 0 )
			return -1;

		const std::string suffix = name.substr(prefix.size());

		if ( suffix.empty() )
			return 0;

		int value = 0;

		for ( unsigned char c : suffix )
		{
			if ( !std::isdigit(c) )
				return -1;

			value = value * 10 + static_cast< int >( c - '0' );
		}

		if ( value < 0 || value > 7 )
			return -1;

		return value;
	}

	const char* GetCastleDoorFrameDebugName(int index)
	{
		switch ( index )
		{
		case 0:
			return "Double Door Frame";
		case 1:
			return "Double Door Frame (1)";
		case 2:
			return "Double Door Frame (2)";
		case 3:
			return "Double Door Frame (3)";
		case 4:
			return "Double Door Frame (4)";
		case 5:
			return "Double Door Frame (5)";
		case 6:
			return "Double Door Frame (6)";
		case 7:
			return "Double Door Frame (7)";
		default:
			return "Double Door Frame (?)";
		}
	}

	bool IsTowerDoorFrame1Name(const std::string& meshName, const std::string& authoringPath)
	{
		UNREFERENCED_PARAMETER(authoringPath);

		const std::string name = NormalizeTowerDoorNameForMatch(meshName);
		return name == "doubledoorframe";
	}

	void DebugPrintTowerDoorPortalLine(const char* text)
	{
		if ( !kEnableTowerDoorPortalCollisionLog )
			return;

		OutputDebugStringA(text);
		OutputDebugStringA("\n");
	}

	void DebugPrintTowerDoorPortalFloat3(
		const char* tag,
		const XMFLOAT3& v)
	{
		if ( !kEnableTowerDoorPortalCollisionLog )
			return;

		char buf[256];

		sprintf_s(
			buf,
			"[TowerDoorPortal][%s] (%.3f, %.3f, %.3f)\n",
			tag,
			v.x,
			v.y,
			v.z
		);

		OutputDebugStringA(buf);
	}

	void DebugPrintTowerDoorPortalOOBB(
		const char* tag,
		const BoundingOrientedBox& box)
	{
		if ( !kEnableTowerDoorPortalCollisionLog )
			return;

		char buf[512];

		sprintf_s(
			buf,
			"[TowerDoorPortal][%s] center=(%.3f, %.3f, %.3f) extents=(%.3f, %.3f, %.3f)\n",
			tag,
			box.Center.x,
			box.Center.y,
			box.Center.z,
			box.Extents.x,
			box.Extents.y,
			box.Extents.z
		);

		OutputDebugStringA(buf);
	}
#endif

	bool ResolvePlacementFilePathFromMapId(
		const std::string& mapId,
		std::string& outPlacementFilePath)
	{
		std::string normalized = ToLowerAscii(TrimString(mapId));

		if ( normalized.size() >= 2 &&
			normalized.front() == '"' &&
			normalized.back() == '"' )
		{
			normalized = normalized.substr(1, normalized.size() - 2);
		}

		if ( normalized.empty() ||
			normalized == "full" ||
			normalized == "fullstage" ||
			normalized == "map_fullstage" ||
			normalized == "mapdata_fullstage" )
		{
			outPlacementFilePath = "MapData/MapData_fullstage(NoTree).txt";
			return true;
		}

		if ( normalized == "fullstage_tree" ||
			normalized == "map_fullstage_tree" ||
			normalized == "mapdata_fullstage_tree" )
		{
			outPlacementFilePath = "MapData/MapData_fullstage.txt";
			return true;
		}

		if ( normalized == "fullstage_withboss" ||
			normalized == "fullstagewithboss" ||
			normalized == "map_fullstage_withboss" ||
			normalized == "map_fullstage_with_boss" ||
			normalized == "mapdata_fullstage_withboss" ||
			normalized == "mapdata_fullstage_with_boss" )
		{
			outPlacementFilePath = "MapData/MapData_fullstage(withBoss).txt";
			return true;
		}

		if ( normalized == "stage1" ||
			normalized == "map_stage1" ||
			normalized == "mapdata_stage1" )
		{
			outPlacementFilePath = "MapData/MapData_stage1_with_Tree.txt";
			return true;
		}

		if ( normalized == "test" ||
			normalized == "tst" ||
			normalized == "map_tst" ||
			normalized == "mapdata_tst" )
		{
			outPlacementFilePath = "MapData/MapData_tst.txt";
			return true;
		}

		return false;
	}

	void TriggerMonsterTestCommand(
		CGameObject* obj,
		EMonsterAnimCommand cmd,
		EMonsterAnimState locomotion)
	{
		if ( !obj )
			return;

		auto* animComp = obj->GetComponent<CAnimatorComponent>();

		if ( !animComp )
			return;

		auto* ctrl = animComp->EnsureMonsterController();

		if ( !ctrl )
			return;

		ctrl->SetLocomotionState(locomotion);
		ctrl->RequestCommand(cmd);
	}
}