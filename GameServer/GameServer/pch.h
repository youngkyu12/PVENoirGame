#pragma once

#define WIN32_LEAN_AND_MEAN // 잘 안쓰는 WIndows 파일들 거르기.

#ifdef _DEBUG
#pragma comment(lib, "ServerCore\\Debug\\ServerCore.lib")
#pragma comment(lib, "Protobuf\\Debug\\libprotobufd.lib")
#pragma comment(lib, "BaseComponent\\Debug\\BaseComponent.lib")
#else
#pragma comment(lib, "ServerCore\\Release\\ServerCore.lib")
#pragma comment(lib, "Protobuf\\Release\\libprotobuf.lib")
#pragma comment(lib, "BaseComponent\\Release\\BaseComponent.lib")
#endif

#pragma comment(lib, "LUA\\lua55.lib")

#include "CorePch.h"
#include "ComponentPch.h"
#include "Enum.pb.h"

#ifndef COMMON_OWNER_TYPE
#define COMMON_OWNER_TYPE CServerObject
#endif

#ifndef COMMON_OWNER_HEADER
#define COMMON_OWNER_HEADER "ServerObject.h"

#endif
using GameSessionRef = shared_ptr<class GameSession>;
using GameAreaRef = shared_ptr<class GameArea>;

using PlayerRef = shared_ptr<class Player>;
using EnemyRef = shared_ptr<class CEnemy>;
using BuildingRef = shared_ptr<class CBuilding>;
using ProjectileRef = shared_ptr<class CProjectile>;
//using ItemRef = shared_ptr<class CItem>;