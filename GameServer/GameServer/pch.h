#pragma once

#define WIN32_LEAN_AND_MEAN // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.

#ifdef _DEBUG
#pragma comment(lib, "ServerCore\\Debug\\ServerCore.lib")
#pragma comment(lib, "Protobuf\\Debug\\libprotobufd.lib")
#pragma comment(lib, "BaseComponent\\Debug\\BaseComponent.lib")
#else
#pragma comment(lib, "ServerCore\\Release\\ServerCore.lib")
#pragma comment(lib, "Protobuf\\Release\\libprotobuf.lib")
#pragma comment(lib, "BaseComponent\\Release\\BaseComponent.lib"))
#endif

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
//using ItemRef = shared_ptr<class CItem>;