//-----------------------------------------------------------------------------
// File: GameSceneContentCatalog.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "GameSceneContentCatalog.h"

#include <utility>

namespace
{
	static int ClampStaticWorldLodLevelInternal(int lodLevel)
	{
		if ( lodLevel < 0 ) return 0;
		if ( lodLevel > 2 ) return 2;
		return lodLevel;
	}

	static int ClampSkinnedWorldLodLevelInternal(int lodLevel)
	{
		if ( lodLevel < 0 ) return 0;
		if ( lodLevel > 2 ) return 2;
		return lodLevel;
	}

	static bool BuildStaticLodMeshBinPathInternal(
		const std::string& baseMeshBinPath,
		int lodLevel,
		std::string& outMeshBinPath)
	{
		const size_t dotPos = baseMeshBinPath.find_last_of('.');
		if ( dotPos == std::string::npos )
			return false;

		const int clampedLodLevel = ClampStaticWorldLodLevelInternal(lodLevel);

		outMeshBinPath = baseMeshBinPath.substr(0, dotPos);
		outMeshBinPath += "_LOD";
		outMeshBinPath += std::to_string(clampedLodLevel);
		outMeshBinPath += baseMeshBinPath.substr(dotPos);
		return true;
	}

	static bool BuildSkinnedLodMeshBinPathInternal(
		const std::string& baseMeshBinPath,
		int lodLevel,
		std::string& outMeshBinPath)
	{
		const size_t dotPos = baseMeshBinPath.find_last_of('.');
		if ( dotPos == std::string::npos )
			return false;

		const int clampedLodLevel = ClampSkinnedWorldLodLevelInternal(lodLevel);

		outMeshBinPath = baseMeshBinPath.substr(0, dotPos);
		outMeshBinPath += "_LOD";
		outMeshBinPath += std::to_string(clampedLodLevel);
		outMeshBinPath += baseMeshBinPath.substr(dotPos);
		return true;
	}
}

const GameSceneStageFileSet& GetLocalStageFileSet(ELocalStagePreset preset)
{
	static const GameSceneStageFileSet kTest =
	{
		"MapData/MapData_tst.txt",
		"MapData/Navmesh_tst.nvm",
		"MapData/CubeBoxColliderReport.txt",
		"MapData/monster_spawn_points_little.txt"
	};

	static const GameSceneStageFileSet kStage1 =
	{
		"MapData/MapData_stage1_with_Tree.txt",
		"MapData/Navmesh_Stage1.nvm",
		"MapData/CubeBoxColliderReport.txt",
		"MapData/monster_spawn_points_little.txt"
	};

	static const GameSceneStageFileSet kFullStageNoBoss =
	{
		"MapData/MapData_fullstage.txt",
		"MapData/Navmesh_FullStage.nvm",
		"MapData/CubeBoxColliderReport.txt",
		"MapData/monster_spawn_points.txt"
	};

	static const GameSceneStageFileSet kFullStageNoTree =
	{
		"MapData/MapData_fullstage(NoTree).txt",
		"MapData/Navmesh_FullStage.nvm",
		"MapData/CubeBoxColliderReport.txt",
		"MapData/monster_spawn_points.txt"
	};

	static const GameSceneStageFileSet kFullStage =
	{
		"MapData/MapData_fullstage(withBoss).txt",
		"MapData/Navmesh_FullStage.nvm",
		"MapData/CubeBoxColliderReport.txt",
		"MapData/monster_spawn_points.txt"
	};

	switch ( preset )
	{
	case ELocalStagePreset::Test:           return kTest;
	case ELocalStagePreset::Stage1:         return kStage1;
	case ELocalStagePreset::FullStage:      return kFullStage;
	case ELocalStagePreset::FullStageNoTree:return kFullStageNoTree;
	case ELocalStagePreset::FullStageNoBoss:return kFullStageNoBoss;
	default:                                return kTest;
	}
}

bool GetGameSceneAssetBuildDesc(EGameSceneAssetId assetId, AssetBuildDesc& outDesc)
{
	switch ( assetId )
	{
	case EGameSceneAssetId::Arrow:
		outDesc = { AssetType::Arrow, "Assets/Weapon/Arrow/Mesh/Arrow_Mesh.bin", "Assets/Weapon/Arrow/Texture" };
		return true;

	case EGameSceneAssetId::Bullet:
		outDesc = { AssetType::Bullet, "Assets/Weapon/Bullet/Mesh/Bullet_Mesh.bin", "Assets/Weapon/Bullet/Texture" };
		return true;

	case EGameSceneAssetId::Helmet:
		outDesc = { AssetType::Helmet, "Assets/Helmet/Mesh/Helmet.bin", "Assets/Helmet/Texture" };
		return true;

	case EGameSceneAssetId::PlayerSword:
		outDesc = { AssetType::Sword, "Assets/Weapon/SwordP/Mesh/Sword_Mesh.bin", "Assets/Weapon/SwordP/Texture" };
		return true;

	case EGameSceneAssetId::PlayerAxe:
		outDesc = { AssetType::Axe, "Assets/Weapon/Axe/Mesh/Axe_Mesh.bin", "Assets/Weapon/Axe/Texture" };
		return true;

	case EGameSceneAssetId::PlayerGun:
		outDesc = { AssetType::Gun, "Assets/Weapon/Gun/Mesh/Gun_Mesh.bin", "Assets/Weapon/Gun/Texture" };
		return true;

	case EGameSceneAssetId::PlayerBow:
		outDesc = { AssetType::Bow, "Assets/Weapon/BowP/Mesh/Bow_Mesh.bin", "Assets/Weapon/BowP/Texture" };
		return true;

	case EGameSceneAssetId::EnemySword:
		outDesc = { AssetType::Sword, "Assets/Weapon/SwordE/Mesh/Sword_Mesh.bin", "Assets/Weapon/SwordE/Texture" };
		return true;

	case EGameSceneAssetId::EnemyBow:
		outDesc = { AssetType::Bow, "Assets/Weapon/BowE/Mesh/Bow_Mesh.bin", "Assets/Weapon/BowE/Texture" };
		return true;

	case EGameSceneAssetId::PlayerMesh1:
		outDesc = { AssetType::Player, "Assets/Player/Mesh/Player_Mesh1.bin", "Assets/Player/Texture" };
		return true;

	case EGameSceneAssetId::PlayerMesh2:
		outDesc = { AssetType::Player, "Assets/Player/Mesh/Player_Mesh2.bin", "Assets/Player/Texture" };
		return true;

	case EGameSceneAssetId::PlayerMesh3:
		outDesc = { AssetType::Player, "Assets/Player/Mesh/Player_Mesh3.bin", "Assets/Player/Texture" };
		return true;

	case EGameSceneAssetId::PlayerMesh4:
		outDesc = { AssetType::Player, "Assets/Player/Mesh/Player_Mesh4.bin", "Assets/Player/Texture" };
		return true;

	case EGameSceneAssetId::Ghoul:
		outDesc = { AssetType::Ghoul, "Assets/Ghoul/Mesh/Ghoul_Mesh.bin", "Assets/Ghoul/Texture" };
		return true;

	case EGameSceneAssetId::SwordMan:
		outDesc = { AssetType::SwordMan, "Assets/Enemy/Mesh/Enemy_Mesh1.bin", "Assets/Enemy/Texture" };
		return true;

	case EGameSceneAssetId::BowMan:
		outDesc = { AssetType::BowMan, "Assets/Enemy/Mesh/Enemy_Mesh1.bin", "Assets/Enemy/Texture" };
		return true;

	case EGameSceneAssetId::Mutant:
		outDesc = { AssetType::Mutant, "Assets/Mutant/Mesh/Mutant_Mesh.bin", "Assets/Mutant/Texture" };
		return true;

	case EGameSceneAssetId::Boss:
		outDesc = { AssetType::Boss, "Assets/Boss/Mesh/Boss_Mesh.bin", "Assets/Boss/Texture" };
		return true;

	default:
		return false;
	}
}

const std::vector<GameSceneClipEntry>& GetGhoulClipEntries()
{
	static const std::vector<GameSceneClipEntry> kEntries =
	{
		{ "Assets/Ghoul/Animation/Ghoul_Anim_Idle.bin",   "Idle" },
		{ "Assets/Ghoul/Animation/Ghoul_Anim_Walk.bin",   "Walk" },
		{ "Assets/Ghoul/Animation/Ghoul_Anim_Run.bin",    "Run" },
		{ "Assets/Ghoul/Animation/Ghoul_Anim_Hit.bin",    "Hit" },
		{ "Assets/Ghoul/Animation/Ghoul_Anim_Attack.bin", "Attack" },
		{ "Assets/Ghoul/Animation/Ghoul_Anim_Death.bin",  "Death" }
	};
	return kEntries;
}

const std::vector<GameSceneClipEntry>& GetEnemySwordClipEntries()
{
	static const std::vector<GameSceneClipEntry> kEntries =
	{
		{ "Assets/Enemy/Animation/Enemy_Normal_Idle.bin", "Idle_Common" },
		{ "Assets/Enemy/Animation/Enemy_Sword_Idle.bin",  "Idle" },
		{ "Assets/Enemy/Animation/Enemy_Walk_F.bin",      "Walk" },
		{ "Assets/Enemy/Animation/Enemy_Run_F.bin",       "Run" },
		{ "Assets/Enemy/Animation/Enemy_Normal_Hit.bin",  "Hit_Common" },
		{ "Assets/Enemy/Animation/Enemy_Sword_Hit.bin",   "Hit" },
		{ "Assets/Enemy/Animation/Enemy_Sword_Attack.bin","Attack" },
		{ "Assets/Enemy/Animation/Enemy_Death.bin",       "Death" }
	};
	return kEntries;
}

const std::vector<GameSceneClipEntry>& GetEnemyBowClipEntries()
{
	static const std::vector<GameSceneClipEntry> kEntries =
	{
		{ "Assets/Enemy/Animation/Enemy_Normal_Idle.bin", "Idle_Common" },
        { "Assets/Enemy/Animation/Enemy_Bow_Idle.bin",    "Idle" },
        { "Assets/Enemy/Animation/Enemy_Walk_F.bin",      "Walk" },
        { "Assets/Enemy/Animation/Enemy_Run_F.bin",       "Run" },
        { "Assets/Enemy/Animation/Enemy_Normal_Hit.bin",  "Hit" },
        { "Assets/Enemy/Animation/Enemy_Bow_Hold.bin",    "Bow_Hold" },
        { "Assets/Enemy/Animation/Enemy_Bow_Load.bin",    "Bow_Load" },
        { "Assets/Enemy/Animation/Enemy_Bow_Release.bin", "Bow_Release" },
        { "Assets/Enemy/Animation/Enemy_Death.bin",       "Death" }
	};
	return kEntries;
}

const std::vector<GameSceneClipEntry>& GetMutantClipEntries()
{
	static const std::vector<GameSceneClipEntry> kEntries =
	{
		{ "Assets/Mutant/Animation/Mutant_Anim_Idle.bin",   "Idle" },
		{ "Assets/Mutant/Animation/Mutant_Anim_Walk.bin",   "Walk" },
		{ "Assets/Mutant/Animation/Mutant_Anim_Run.bin",    "Run" },
		{ "Assets/Mutant/Animation/Mutant_Anim_Hit.bin",    "Hit" },
		{ "Assets/Mutant/Animation/Mutant_Anim_Attack.bin", "Attack" },
		{ "Assets/Mutant/Animation/Mutant_Anim_Dead.bin",   "Death" }
	};
	return kEntries;
}

const std::vector<GameSceneClipEntry>& GetBossClipEntries()
{
	static const std::vector<GameSceneClipEntry> kEntries =
	{
		{ "Assets/Boss/Animation/Boss_Anim_Appear.bin",      "Appear" },
		{ "Assets/Boss/Animation/Boss_Anim_AttackLeft.bin",  "AttackLeft" },
		{ "Assets/Boss/Animation/Boss_Anim_AttackRight.bin", "AttackRight" },
		{ "Assets/Boss/Animation/Boss_Anim_Call.bin",        "Call" },
		{ "Assets/Boss/Animation/Boss_Anim_Death.bin",       "Death" },
		{ "Assets/Boss/Animation/Boss_Anim_Hit.bin",         "Hit" },
		{ "Assets/Boss/Animation/Boss_Anim_Idle.bin",        "Idle" },
		{ "Assets/Boss/Animation/Boss_Anim_Spell.bin",       "Spell" },
		{ "Assets/Boss/Animation/Boss_Anim_Walk.bin",        "Walk" }
	};
	return kEntries;
}

bool ResolveStaticAssetPathDesc(const std::string& assetName, StaticAssetPathDesc& outDesc)
{
	if ( assetName == "Grass" )
	{
		outDesc = { AssetType::Grass, "Assets/GroundPlane/Mesh/Grass.bin", "Assets/GroundPlane/Texture" };
		return true;
	}
	if ( assetName == "Ground" )
	{
		outDesc = { AssetType::Ground, "Assets/GroundPlane/Mesh/Ground.bin", "Assets/GroundPlane/Texture" };
		return true;
	}
	if ( assetName == "VillageWall" )
	{
		outDesc = { AssetType::VillageWall, "Assets/VillageWall/Mesh/VillageWall.bin", "Assets/VillageWall/Texture" };
		return true;
	}
	if ( assetName == "DirtRoad" )
	{
		outDesc = { AssetType::DirtRoad, "Assets/GroundPlane/Mesh/DirtRoad.bin", "Assets/GroundPlane/Texture" };
		return true;
	}
	if ( assetName == "Building1" )
	{
		outDesc = { AssetType::House, "Assets/House/Mesh/Building1.bin", "Assets/House/Texture" };
		return true;
	}
	if ( assetName == "Building2" )
	{
		outDesc = { AssetType::House, "Assets/House/Mesh/Building2.bin", "Assets/House/Texture" };
		return true;
	}
	if ( assetName == "Building3" )
	{
		outDesc = { AssetType::House, "Assets/House/Mesh/Building3.bin", "Assets/House/Texture" };
		return true;
	}
	if ( assetName == "Building4" )
	{
		outDesc = { AssetType::House, "Assets/House/Mesh/Building4.bin", "Assets/House/Texture" };
		return true;
	}
	if ( assetName == "Building5" )
	{
		outDesc = { AssetType::House, "Assets/House/Mesh/Building5.bin", "Assets/House/Texture" };
		return true;
	}
	if ( assetName == "Building6" )
	{
		outDesc = { AssetType::House, "Assets/House/Mesh/Building6.bin", "Assets/House/Texture" };
		return true;
	}
	if ( assetName == "Building7" )
	{
		outDesc = { AssetType::House, "Assets/House/Mesh/Building7.bin", "Assets/House/Texture" };
		return true;
	}
	if ( assetName == "Building8" )
	{
		outDesc = { AssetType::House, "Assets/House/Mesh/Building8.bin", "Assets/House/Texture" };
		return true;
	}
	if ( assetName == "Building9" )
	{
		outDesc = { AssetType::House, "Assets/House/Mesh/Building9.bin", "Assets/House/Texture" };
		return true;
	}
	if ( assetName == "Tower" )
	{
		outDesc = { AssetType::Tower, "Assets/Tower/Mesh/Tower.bin", "Assets/Tower/Texture" };
		return true;
	}
	if ( assetName == "Castle" )
	{
		outDesc = { AssetType::Castle, "Assets/Castle/Mesh/Castle.bin", "Assets/Castle/Texture" };
		return true;
	}
	if ( assetName == "Tree1" )
	{
		outDesc = { AssetType::Tree, "Assets/Tree/Mesh/Tree1.bin", "Assets/Tree/Texture" };
		return true;
	}
	if ( assetName == "Tree2" )
	{
		outDesc = { AssetType::Tree, "Assets/Tree/Mesh/Tree2.bin", "Assets/Tree/Texture" };
		return true;
	}
	if ( assetName == "Tree3" )
	{
		outDesc = { AssetType::Tree, "Assets/Tree/Mesh/Tree3.bin", "Assets/Tree/Texture" };
		return true;
	}
	if ( assetName == "Tree4" )
	{
		outDesc = { AssetType::Tree, "Assets/Tree/Mesh/Tree4.bin", "Assets/Tree/Texture" };
		return true;
	}
	if ( assetName == "Tree5" )
	{
		outDesc = { AssetType::Tree, "Assets/Tree/Mesh/Tree5.bin", "Assets/Tree/Texture" };
		return true;
	}
	if ( assetName == "Tree6" )
	{
		outDesc = { AssetType::Tree, "Assets/Tree/Mesh/Tree6.bin", "Assets/Tree/Texture" };
		return true;
	}

	return false;
}

bool IsStaticWorldLodSupportedAssetName(const std::string& assetName)
{
	if ( assetName == "Grass" ) return false;
	if ( assetName == "Ground" ) return false;
	if ( assetName == "DirtRoad" ) return false;

	if ( assetName == "VillageWall" ) return true;

	if ( assetName == "Building1" ) return true;
	if ( assetName == "Building2" ) return true;
	if ( assetName == "Building3" ) return true;
	if ( assetName == "Building4" ) return true;
	if ( assetName == "Building5" ) return true;
	if ( assetName == "Building6" ) return true;
	if ( assetName == "Building7" ) return true;
	if ( assetName == "Building8" ) return true;
	if ( assetName == "Building9" ) return true;

	if ( assetName == "Tower" ) return true;
	if ( assetName == "Castle" ) return true;

	if ( assetName == "Tree1" ) return true;
	if ( assetName == "Tree2" ) return true;
	if ( assetName == "Tree3" ) return true;
	if ( assetName == "Tree4" ) return true;
	if ( assetName == "Tree5" ) return true;
	if ( assetName == "Tree6" ) return true;

	return false;
}

bool ShouldUseStaticWorldDistanceCull(const std::string& assetName)
{
	if ( assetName == "Grass" ) return false;
	if ( assetName == "Ground" ) return false;
	if ( assetName == "DirtRoad" ) return false;
	if ( assetName == "VillageWall" ) return false;
	if ( assetName == "Castle" ) return false;

	return true;
}

bool ResolveStaticLodAssetPathDesc(
	const std::string& assetName,
	int lodLevel,
	StaticAssetPathDesc& outDesc)
{
	if ( !ResolveStaticAssetPathDesc(assetName, outDesc) )
		return false;

	if ( !IsStaticWorldLodSupportedAssetName(assetName) )
		return true;

	std::string lodMeshBinPath;
	if ( !BuildStaticLodMeshBinPathInternal(outDesc.meshBinPath, lodLevel, lodMeshBinPath) )
		return false;

	outDesc.meshBinPath = std::move(lodMeshBinPath);
	return true;
}

bool ResolveStaticAssetDesc(const std::string& assetName, AssetBuildDesc& outDesc, AssetType* outResolvedType)
{
	StaticAssetPathDesc resolved{};
	if ( !ResolveStaticAssetPathDesc(assetName, resolved) )
		return false;

	outDesc = { resolved.type, resolved.meshBinPath, resolved.texturePath };

	if ( outResolvedType )
		*outResolvedType = resolved.type;

	return true;
}

bool ResolveStaticLodAssetDesc(
	const std::string& assetName,
	int lodLevel,
	AssetBuildDesc& outDesc,
	AssetType* outResolvedType)
{
	StaticAssetPathDesc resolved{};
	if ( !ResolveStaticLodAssetPathDesc(assetName, lodLevel, resolved) )
		return false;

	outDesc = { resolved.type, resolved.meshBinPath, resolved.texturePath };

	if ( outResolvedType )
		*outResolvedType = resolved.type;

	return true;
}

bool ResolveGhoulSkinnedLodAssetDesc(int lodLevel, AssetBuildDesc& outDesc)
{
	std::string meshPath;
	if ( !BuildSkinnedLodMeshBinPathInternal("Assets/Ghoul/Mesh/Ghoul_Mesh.bin", lodLevel, meshPath) )
		return false;

	outDesc =
	{
		AssetType::Ghoul,
		meshPath,
		"Assets/Ghoul/Texture"
	};
	return true;
}

bool ShouldCreateWorldStaticCollider(const std::string& assetName)
{
	if ( assetName == "Grass" )    return false;
	if ( assetName == "Ground" )   return false;
	if ( assetName == "DirtRoad" ) return false;
	return true;
}

const std::vector<GameSceneClipEntry>& GetPlayerClipEntries()
{
	static const std::vector<GameSceneClipEntry> kEntries =
	{
		{ "Assets/Player/Animation/Player_Normal_Idle.bin", "Idle_Normal" },
		{ "Assets/Player/Animation/Player_Sword_Idle.bin",  "Idle_Sword" },
		{ "Assets/Player/Animation/Player_Axe_Idle.bin",    "Idle_Axe" },
		{ "Assets/Player/Animation/Player_Bow_Idle.bin",    "Idle_Bow" },
		{ "Assets/Player/Animation/Player_Gun_Idle.bin",    "Idle_Gun" },

		{ "Assets/Player/Animation/Player_Normal_Hit.bin",  "Hit_Normal" },
		{ "Assets/Player/Animation/Player_Sword_Hit.bin",   "Hit_Sword" },
		{ "Assets/Player/Animation/Player_Death.bin",       "Death" },

		{ "Assets/Player/Animation/Player_Sword_Attack.bin","Attack_Sword" },
		{ "Assets/Player/Animation/Player_Axe_Attack.bin",  "Attack_Axe" },
		{ "Assets/Player/Animation/Player_Bow_Load.bin",    "Bow_Load" },
		{ "Assets/Player/Animation/Player_Bow_Release.bin", "Bow_Release" },
		{ "Assets/Player/Animation/Player_Gun_Shoot.bin",   "Gun_Shoot" },
		{ "Assets/Player/Animation/Player_Roll_F.bin",      "Roll_F" },
		{ "Assets/Player/Animation/Player_Roll_B.bin",      "Roll_B" },

		{ "Assets/Player/Animation/Player_Walk_F.bin",      "Walk_F" },
		{ "Assets/Player/Animation/Player_Walk_B.bin",      "Walk_B" },
		{ "Assets/Player/Animation/Player_Walk_L.bin",      "Walk_L" },
		{ "Assets/Player/Animation/Player_Walk_R.bin",      "Walk_R" },
		{ "Assets/Player/Animation/Player_Walk_FL.bin",     "Walk_FL" },
		{ "Assets/Player/Animation/Player_Walk_FR.bin",     "Walk_FR" },
		{ "Assets/Player/Animation/Player_Walk_BL.bin",     "Walk_BL" },
		{ "Assets/Player/Animation/Player_Walk_BR.bin",     "Walk_BR" },

		{ "Assets/Player/Animation/Player_Run_F.bin",       "Run_F" },
		{ "Assets/Player/Animation/Player_Run_B.bin",       "Run_B" },
		{ "Assets/Player/Animation/Player_Run_L.bin",       "Run_L" },
		{ "Assets/Player/Animation/Player_Run_R.bin",       "Run_R" },
		{ "Assets/Player/Animation/Player_Run_FL.bin",      "Run_FL" },
		{ "Assets/Player/Animation/Player_Run_FR.bin",      "Run_FR" },
		{ "Assets/Player/Animation/Player_Run_BL.bin",      "Run_BL" },
		{ "Assets/Player/Animation/Player_Run_BR.bin",      "Run_BR" }
	};
	return kEntries;
}

const std::vector<GameSceneClipEntry>& GetPlayerBowClipEntries()
{
	static const std::vector<GameSceneClipEntry> kEntries =
	{
		{ "Assets/Weapon/BowP/Animation/Bow_Anim.bin", "Fire" }
	};
	return kEntries;
}

const std::vector<GameSceneClipEntry>& GetEnemyBowWeaponClipEntries()
{
	static const std::vector<GameSceneClipEntry> kEntries =
	{
		{ "Assets/Weapon/BowE/Animation/Bow_Anim.bin", "Fire" }
	};
	return kEntries;
}

bool GetGameSceneAttachmentPresetDesc(
	EGameSceneAttachmentPresetId id,
	GameSceneAttachmentPresetDesc& outDesc)
{
	outDesc = GameSceneAttachmentPresetDesc{};

	switch ( id )
	{
	case EGameSceneAttachmentPresetId::PlayerSword:
		outDesc.boneName = "hand_r";
		outDesc.local.pos = XMFLOAT3(0.09635256f, -0.02604572f, -0.008439302f);
		outDesc.local.rotDeg = XMFLOAT3(-15.925f, 0.919f, -7.3f);
		outDesc.local.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
		return true;

	case EGameSceneAttachmentPresetId::PlayerBow:
		outDesc.boneName = "hand_l";
		outDesc.local.pos = XMFLOAT3(-0.1f, 0.0f, 0.0f);
		outDesc.local.rotDeg = XMFLOAT3(0.0f, 180.0f, 0.0f);
		outDesc.local.scale = XMFLOAT3(2.0f, 1.0f, 1.0f);
		return true;

	case EGameSceneAttachmentPresetId::PlayerAxe:
		outDesc.boneName = "hand_r";
		outDesc.local.pos = XMFLOAT3(0.099f, 0.025f, 0.166f);
		outDesc.local.rotDeg = XMFLOAT3(-15.925f, 0.919f, 172.7f);
		outDesc.local.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
		return true;

	case EGameSceneAttachmentPresetId::PlayerGun:
		outDesc.boneName = "hand_r";
		outDesc.local.pos = XMFLOAT3(-0.00403512f, 0.00763781f, 0.04897089f);
		outDesc.local.rotDeg = XMFLOAT3(-2.441f, 75.104f, 80.119f);
		outDesc.local.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
		return true;

	case EGameSceneAttachmentPresetId::EnemySword:
		outDesc.boneName = "hand_r";
		outDesc.local.pos = XMFLOAT3(
			0.09635256f * 1.5f,
			-0.02604572f * 1.5f,
			-0.008439302f * 1.5f
		);
		outDesc.local.rotDeg = XMFLOAT3(-15.925f, 0.919f, -7.3f);
		outDesc.local.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
		return true;

	case EGameSceneAttachmentPresetId::EnemyBow:
		outDesc.boneName = "hand_l";
		outDesc.local.pos = XMFLOAT3(
			-0.1f * 1.5f,
			0.0f * 1.5f,
			0.0f * 1.5f
		);
		outDesc.local.rotDeg = XMFLOAT3(0.0f, 180.0f, 0.0f);
		outDesc.local.scale = XMFLOAT3(2.0f, 1.0f, 1.0f);
		return true;

	case EGameSceneAttachmentPresetId::MutantHelmet:
		outDesc.boneName = "CATRigHub002";
		outDesc.local.pos = XMFLOAT3(3.8f, 0.35f, 0.0f);
		outDesc.local.rotDeg = XMFLOAT3(-90.0f, 0.0f, 90.0f);
		outDesc.local.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
		return true;

	default:
		break;
	}

	return false;
}