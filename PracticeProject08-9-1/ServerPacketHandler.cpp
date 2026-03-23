#include "stdafx.h"
#include "ServerPacketHandler.h"
#include "BufferReader.h"
#include "Protocol.pb.h"
#include "GlobalValues.h"
#include "NetworkQueue.h"


#include "GlobalEnum.h"

PacketHandlerFunc GPacketHandler[UINT16_MAX];

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
	// TODO : 로그 남기기
	return false;
}

#include <string>

bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt)
{
	if (pkt.success() == false)
	{
		// 로그인 실패 처리
		return true;
	}

	std::string pktDebug = pkt.DebugString();
	pktDebug += "\n";
	OutputDebugStringA(pktDebug.c_str());

	g_myPlayerId = pkt.playerid();


	if (pkt.players().size() == 0)
	{
		//캐릭터 생성 화면
	}

	// 입장 UI 버튼을 눌러서 게임에 입장
	Protocol::C_ENTER_GAME enterGamePkt;
	enterGamePkt.set_playerid(g_myPlayerId);
	enterGamePkt.set_ready(true);
	//enterGamePkt.set_playerindex(0); // 첫번째 캐릭터로 입장
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(enterGamePkt);
	session->Send(sendBuffer);


	return true;
}

bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt)
{
	// GAME_START 패킷을 계속 전송함
	Protocol::C_GAME_START startPkt;
	startPkt.set_playerid(g_myPlayerId);
	startPkt.set_playerweapon(0x1010);
	startPkt.set_ready(true);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(startPkt);
	session->Send(sendBuffer);

	return true;
}

bool Handle_S_GAME_START(PacketSessionRef& session, Protocol::S_GAME_START& pkt)
{
	GameStartData data;

	// 초기 정보를 수신받아 적용

	Protocol::InitStruct worldInit = pkt.initstruct();
	auto players = worldInit.players();
	auto enemies = worldInit.enemies();
	//auto buildings = worldInit.buildings();

	data.players.reserve(players.size());
	data.enemies.reserve(enemies.size());

	for (auto& player : players)
	{
		// 플레이어 정보를 옮긴다
		auto transform = player.transform();
		auto position = transform.position();
		auto yaw = transform.yaw();

		std::string s = player.DebugString();
		s += "\n";
		OutputDebugStringA(s.c_str());

		data.players.push_back({ player.id(), {position.x(), position.y(), position.z()}, yaw });
	}

	for (auto& enemy : enemies)
	{
		// 적 정보를 옮긴다
		auto transform = enemy.transform();
		auto position = transform.position();
		auto yaw = transform.yaw();

		std::string s = enemy.DebugString();
		s += "\n";
		OutputDebugStringA(s.c_str());

		data.enemies.push_back({ enemy.id(), {position.x(), position.y(), position.z()}, yaw });
	}

	// networkQueue에 게임 시작 패킷 push
	g_NetworkQueue.PushGameStart(std::move(data));

	g_GameStarted = true;
	return true;
}

EAnimState GetEAnimState(Protocol::AnimationType animType)
{
	switch (animType)
	{
	case Protocol::ANIMATION_TYPE_IDLE:
		return EAnimState::Idle;
	case Protocol::ANIMATION_TYPE_WALK:
		return EAnimState::Move;
	case Protocol::ANIMATION_TYPE_RUN:
		return EAnimState::Move;
	case Protocol::ANIMATION_TYPE_ATTACK:
		return EAnimState::Attack;
	case Protocol::ANIMATION_TYPE_ROLL:
		// 현재 클라 enum에는 Roll 상태가 없으므로 Attack 상태 트랙으로 태운다.
		return EAnimState::Attack;
	case Protocol::ANIMATION_TYPE_DIE:
		return EAnimState::Die;
	default:
		return EAnimState::Idle; // 기본값
	}
}

bool Handle_S_FRAME_STATE(PacketSessionRef& session, Protocol::S_FRAME_STATE& pkt)
{
	FrameSnapshot data;

	data.frameId = pkt.servertick();

	// 프레임마다 적의 위치, 플레이어의 위치, 플레이어의 HP 등등을 수신받아 적용
	auto players = pkt.players();
	auto enemies = pkt.enemies();

	for (auto& player : players)
	{
		// 플레이어 정보를 옮긴다
		auto transform = player.transform();
		auto position = transform.position();
		auto yaw = transform.yaw();
		auto animation = player.animation();
		auto weaponType = player.weapontype();

		std::string s = player.DebugString();
		s += "\n";
		//OutputDebugStringA(s.c_str());

		AnimationState animState;

		// 애니메이션 상태를 옮긴다
		animState.animationId = GetEAnimState(animation.animationtype());
		animState.animTick = animation.animationtick();
		animState.isRoll = (animation.animationtype() == Protocol::ANIMATION_TYPE_ROLL);

		if (animState.isRoll)
		{
			
			std::cout << animState.isRoll << std::endl;
		}

		EWeaponType eweaponType = static_cast<EWeaponType>(weaponType - 1);

		data.players.push_back({ player.id(), {position.x(), position.y(), position.z()}, yaw, animState, eweaponType });
	}

	for (auto& enemy : enemies)
	{
		// 적 정보를 옮긴다
		auto transform = enemy.transform();
		auto position = transform.position();
		auto yaw = transform.yaw();
		auto animation = enemy.animation();
		auto weaponType = enemy.weapontype();
		//std::string s = enemy.DebugString();

		AnimationState animState;

		// 애니메이션 상태를 옮긴다
		animState.animationId = static_cast<EAnimState>(animation.animationtype());
		animState.animTick = animation.animationtick();

		EWeaponType eweaponType = static_cast<EWeaponType>(weaponType - 1);

		data.enemies.push_back({ enemy.id(), {position.x(), position.y(), position.z()}, yaw, animState, eweaponType });
	}

	g_NetworkQueue.PushFrameState(std::move(data));
	return false;
}

