#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "Enemy.h"
#include "Building.h"
#include "GameSession.h"
#include "GameArea.h"

#include "CommonPlayerControllerComponent.h"

#include "Protocol.pb.h"
#include "ClientPacketHandler.h"

shared_ptr<Room> GRoom = make_shared<Room>();

void Room::Enter(PlayerRef player)
{
	players[player->playerId] = player;
}

void Room::Leave(PlayerRef player)
{
	players.erase(player->playerId);
}

void Room::BroadCastAll(SendBufferRef sendBuffer)
{
	//for(auto& area : gameAreas)
	//{
	//	area->BroadCast(sendBuffer);
	//}

	for (auto& p : players)
	{
		p.second->ownerSession->Send(sendBuffer);
	}
}



void Room::BuildRoom()
{
	for (int i = 0; i < 10; ++i)
	{
		auto enemy = make_shared<CEnemy>(i, u8"Zombie", Protocol::ENEMY_TYPE_BASIC, nullptr);
		enemy->Build(GameMath::Vec3(i * 10.0f, 0, 0), GameMath::Vec3(0, 0, 0));
		//enemies[i]->AddComponent<CTransformComponent>();
		//enemies[i]->CreateComponents();
		enemies[i] = enemy;
	}

	for (int i = 0; i < 30; ++i)
	{
		auto enemy = make_shared<CEnemy>(i + 10, u8"FIghter", Protocol::ENEMY_TYPE_ARCHER, nullptr);
		enemy->Build(GameMath::Vec3((i + 10) * 10.0f, 0, 10), GameMath::Vec3(0, 0, 0));
		//enemies[i]->AddComponent<CTransformComponent>();
		//enemies[i]->CreateComponents();
		enemies[i] = enemy;
	}
}

void Room::StartGame(bool ready, uint32 index)
{
	// 모든 플레이어가 Ready를 보냈음을 확인하면 시작
	if(players.find(index) == players.end())
		return;

	WRITE_LOCK;
	static bool p_ready[4] = { false, false, false, false };
	p_ready[index] = ready;

	static Atomic<bool> gameStarted = false;

	// if(p_ready[0] && p_ready[1] && p_ready[2] && p_ready[3])
	if(p_ready[0] && p_ready[1])
	{
		if (gameStarted.exchange(true) == false)
		{
			GRoom->DoAsync(&Room::MakeInitStruct, Protocol::S_GAME_START());
		}
	}
	else
	{
		// 아직 모든 플레이어가 Ready를 보내지 않았다면 S_ENTER_GAME 패킷을 보내서 Ready를 요청한다.
		Protocol::S_ENTER_GAME enterGamePkt;
		GRoom->DoAsync(&Room::MakeEnterGameStruct, enterGamePkt);

	}
	
}

void Room::EndGame()
{
}

void Room::TickAdvance()
{
	MakeFrameState();
}

void Room::ProcessInput(uint64 playerId, int32 keyCodes, float deltaX, float deltaY)
{
	// 플레이어 찾기
	auto it = players.find(playerId);
	if (it == players.end())
		return;

	PlayerRef& player = it->second;

	// ========== 1. 회전 처리 (클라이언트 Rotate 함수와 동일) ==========
	if (deltaX != 0.0f)
	{
		float currentYaw = player->GetYaw();
		player->SetYaw(GameMath::NormalizeYaw(currentYaw + deltaX));
	}

	// ========== 2. 이동 처리 (클라이언트 Move 함수와 동일) ==========
	constexpr int kDirForward  = 1 << 0;
	constexpr int kDirBackward = 1 << 1;
	constexpr int kDirLeft     = 1 << 2;
	constexpr int kDirRight    = 1 << 3;

	if (keyCodes == 0)
		return;

	// Look/Right 벡터 (GameMath 사용)
	GameMath::Vec3 look  = player->GetLook();
	GameMath::Vec3 right = player->GetRight();

	// 이동 거리 계산
	const float speed = 5.0f;
	const float dt = 0.03f;
	float fDistance = speed * dt;

	// 방향별 shift 누적 (클라이언트 Move 함수와 동일한 로직)
	GameMath::Vec3 shift = GameMath::Vec3::Zero();

	if (keyCodes & kDirForward)
		shift += look * fDistance;

	if (keyCodes & kDirBackward)
		shift += look * (-fDistance);

	if (keyCodes & kDirRight)
		shift += right * fDistance;

	if (keyCodes & kDirLeft)
		shift += right * (-fDistance);

	// 위치 적용
	player->Move(shift);
}



void Room::MakeFrameState()
{
	// 게임 로직 업데이트 (예: 적 이동, 충돌 검사 등)
	Protocol::S_FRAME_STATE frameStatePkt;
	// 프레임 상태 패킷 작성 (예: 플레이어 위치, 적 상태 등)

	for (auto playerMap : players)
	{
		PlayerRef& player = playerMap.second;

		auto p = frameStatePkt.add_players();
		p->set_id(player->playerId);
		p->set_name(player->name);
		p->set_playertype(player->type);

		Protocol::Transform* transform = p->mutable_transform();
		Protocol::Vec3f* position = transform->mutable_position();
		position->set_x(player->GetPosition().x);
		position->set_y(player->GetPosition().y);
		position->set_z(player->GetPosition().z);

		transform->set_yaw(player->GetYaw());
	}


	for (auto enemyMap : enemies)
	{
		EnemyRef& enemy = enemyMap.second;
		auto e = frameStatePkt.add_enemies();
		e->set_id(enemyMap.first);
		e->set_enemytype(enemy->type);


		Protocol::Transform* transform = e->mutable_transform();
		Protocol::Vec3f* position = transform->mutable_position();
		position->set_x(enemy->GetPosition().x);
		position->set_y(enemy->GetPosition().y);
		position->set_z(enemy->GetPosition().z);


	}





	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(frameStatePkt);
	GRoom->DoAsync(&Room::BroadCastAll, sendBuffer);


	// 다음 업데이트 예약
	GRoom->DoTimer(30, &Room::TickAdvance);
}

void Room::MakeInitStruct(Protocol::S_GAME_START gameStartPkt)
{
	// 최초로 들어온 스레드가 패킷 작성 후 전송
	Protocol::InitStruct* initStruct = gameStartPkt.mutable_initstruct();
	for (auto playerMap : players)
	{
		PlayerRef& player = playerMap.second;

		auto p = initStruct->add_players();
		p->set_id(player->playerId);
		p->set_name(player->name);
		p->set_playertype(player->type);

		Protocol::Transform* transform = p->mutable_transform();
		Protocol::Vec3f* position = transform->mutable_position();
		position->set_x(player->GetPosition().x);
		position->set_y(player->GetPosition().y);
		position->set_z(player->GetPosition().z);

		transform->set_yaw(player->GetYaw());
	}


	for (auto enemyMap : enemies)
	{
		EnemyRef& enemy = enemyMap.second;
		auto e = initStruct->add_enemies();
		e->set_id(enemyMap.first);
		e->set_enemytype(enemy->type);


		Protocol::Transform* transform = e->mutable_transform();
		Protocol::Vec3f* position = transform->mutable_position();
		position->set_x(enemy->GetPosition().x);
		position->set_y(enemy->GetPosition().y);
		position->set_z(enemy->GetPosition().z);


	}





	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(gameStartPkt);
	GRoom->DoAsync(&Room::BroadCastAll, sendBuffer);

	cout << "Game Started!" << endl;

	// 게임 시작 로직 (예: 타이머 시작, 적 스폰 등)
	GRoom->DoTimer(100, &Room::TickAdvance);
}

void Room::MakeEnterGameStruct(Protocol::S_ENTER_GAME enterGamePkt)
{
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(enterGamePkt);
	BroadCastAll(sendBuffer);
}

GameAreaRef Room::GetArea(uint32 areaId)
{
	return GameAreaRef();
}

void Room::TransferPlayer(PlayerRef player, uint32 fromAreaId, uint32 toAreaId)
{
	//gameAreas[fromAreaId]->Leave(player);
	//gameAreas[toAreaId]->Enter(player);
}
