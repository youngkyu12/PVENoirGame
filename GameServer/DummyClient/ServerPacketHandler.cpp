#include "pch.h"
#include "ServerPacketHandler.h"
#include "BufferReader.h"
#include "Protocol.pb.h"


PacketHandlerFunc GPacketHandler[UINT16_MAX];

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
	// TODO : 로그 남기기
	return false;
}

bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt)
{
	if (pkt.success() == false)
	{
		// 로그인 실패 처리
		return true;
	}

	//g_myPlayerId = pkt.playerid();


	if (pkt.players().size() == 0)
	{
		//캐릭터 생성 화면
	}

	// 입장 UI 버튼을 눌러서 게임에 입장
	Protocol::C_ENTER_GAME enterGamePkt;
	//enterGamePkt.set_playerid(g_myPlayerId);
	enterGamePkt.set_ready(true);
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(enterGamePkt);
	session->Send(sendBuffer);


	return true;
}

bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt)
{
	// GAME_START 패킷을 계속 전송함
	Protocol::C_GAME_START startPkt;
	//startPkt.set_playerid(g_myPlayerId);
	startPkt.set_playerweapon(0x1010);
	startPkt.set_ready(true);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(startPkt);
	session->Send(sendBuffer);

	return true;
}

bool Handle_S_GAME_START(PacketSessionRef& session, Protocol::S_GAME_START& pkt)
{
	// 초기 정보를 수신받아 적용
	const std::string& mapId = pkt.mapid();

	Protocol::InitStruct worldInit = pkt.initstruct();
	auto players = worldInit.players();
	auto enemies = worldInit.enemies();
	(void)mapId;

	for (auto& player : players)
	{
		// 플레이어 정보를 옮긴다
	}

	for (auto& enemy : enemies)
	{
		// 적 정보를 옮긴다
	}

	return false;
}

bool Handle_S_FRAME_STATE(PacketSessionRef& session, Protocol::S_FRAME_STATE& pkt)
{
	// 프레임마다 적의 위치, 플레이어의 위치, 플레이어의 HP 등등을 수신받아 적용
	auto players = pkt.players();
	auto enemies = pkt.enemies();

	for (auto& player : players)
	{
		// 플레이어 정보를 옮긴다
	}

	for (auto& enemy : enemies)
	{
		// 적 정보를 옮긴다
	}

	return false;
}


