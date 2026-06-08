//-----------------------------------------------------------------------------
// File: GameScenePrivate.h
// Description:
//   GameScene*.cpp 전용 공통 include 묶음.
//   로직/상수/helper는 넣지 않고 include만 모은다.
//-----------------------------------------------------------------------------
#pragma once

#include "GameScene.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <cstdio>
#include <cctype>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <random>
#include <string>
#include <vector>
#include <array>
#include <memory>

#include "AnimatorComponent.h"
#include "AnimatorData.h"
#include "Animator.h"
#include "AnimController.h"
#include "MonsterAnimController.h"
#include "MonsterAnimTypes.h"

#include "Material.h"
#include "AssetManager.h"
#include "Texture.h"

#include "LightComponent.h"
#include "PlayerControllerComponent.h"
#include "Object.h"
#include "Mesh.h"
#include "SkinningComponent.h"
#include "ActorTagComponent.h"
#include "ArrowComponent.h"
#include "BulletComponent.h"
#include "Camera.h"
#include "FollowBoneComponent.h"
#include "PlayerEquipmentComponent.h"

#include "CollisionSystem.h"
#include "ColliderComponent.h"
#include "WeaponHitboxComponent.h"
#include "MonsterWeaponHitboxComponent.h"

#include "MonsterCombatComponent.h"
#include "NavMesh.h"
#include "MonsterAIComponent.h"
#include "MonsterAIVariants.h"
#include "HealthComponent.h"
#include "AttackPowerComponent.h"
#include "TerrainAttachComponent.h"
#include "InventoryComponent.h"
#include "TerrainData.h"

#include "AudioManager.h"
#include "MusicDirector.h"
#include "EnemySpawner.h"

#include "ThreadManager.h"
#include "Service.h"
#include "Session.h"
#include "BufferReader.h"
#include "ServerPacketHandler.h"

#include "GlobalValues.h"
#include "GameSceneContentCatalog.h"
#include "GameSceneObjectFactory.h"
#include "GameSceneAttachmentBinder.h"
