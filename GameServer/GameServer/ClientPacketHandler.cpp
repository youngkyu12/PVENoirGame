#include "pch.h"
#include "ClientPacketHandler.h"
#include "Player.h"
#include "Enemy.h"
#include "Room.h"
#include "GameSession.h"

PacketHandlerFunc GPacketHandler[UINT16_MAX];

// 컨텐츠 작업자가 직접

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
	// TODO : 로그 남기기
	return false;
}

bool Handle_C_LOGIN(PacketSessionRef& session, Protocol::C_LOGIN& pkt)
{
	GameSessionRef gameSession = static_pointer_cast<GameSession>(session);

	// TODO: Validation Check

	Protocol::S_LOGIN loginPkt;
	loginPkt.set_success(true);

	// DB에서 플레이 정보 가져오기
	// GameSession에 플레이 정보 저장 (메모리)

	// ID 발급
	static Atomic<uint64> idGenerator = 0;
	if(idGenerator < 4) // 4명만 받는다
	{
		loginPkt.set_playerid(idGenerator);
		auto player = loginPkt.add_players();
		player->set_id(idGenerator++);
		player->set_name(u8"Player");
		player->set_playertype(Protocol::PLAYER_TYPE_KNIGHT);

		PlayerRef playerRef = make_shared<Player>();
		playerRef->playerId = player->id();
		playerRef->name = player->name();
		playerRef->type = player->playertype();
		playerRef->ownerSession = gameSession;
		playerRef->Build();

		player->set_allocated_transform(new Protocol::Transform());

		gameSession->_players.push_back(playerRef);
	}
	else
	{
		loginPkt.set_success(false);
	}

	std::cout << loginPkt.DebugString() << "\n";
	std::cout << "ByteSizeLong=" << loginPkt.ByteSizeLong() << "\n";
	auto SendBuffer = ClientPacketHandler::MakeSendBuffer(loginPkt);
	session->Send(SendBuffer);

	return true;
}

bool Handle_C_ENTER_GAME(PacketSessionRef& session, Protocol::C_ENTER_GAME& pkt)
{
	GameSessionRef gameSession = static_pointer_cast<GameSession>(session);

	uint64 index = pkt.playerid();
	// TODO: Validation Check

	gameSession->_currentPlayer= gameSession->_players[0];
	gameSession->_room = GRoom;

	GRoom->DoAsync(&Room::Enter, gameSession->_currentPlayer);

	Protocol::S_ENTER_GAME enterGamePkt;
	//enterGamePkt.set_success(true);
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(enterGamePkt);
	gameSession->_currentPlayer->ownerSession->Send(sendBuffer);

	cout << "Player" << index << " Entered Game..." << endl;
	return true;
}

bool Handle_C_GAME_START(PacketSessionRef& session, Protocol::C_GAME_START& pkt)
{
	GameSessionRef gameSession = static_pointer_cast<GameSession>(session);
	//cout << "Send World Info..." << endl;		
	
	//GRoom->DoAsync(&Room::StartGame, pkt.ready(), pkt.playerid());
	GRoom->DoTimer(1000, &Room::StartGame, pkt.ready(), pkt.playerid());

	return true;
}

bool Handle_C_INPUT(PacketSessionRef& session, Protocol::C_INPUT& pkt)
{
	GameSessionRef gameSession = static_pointer_cast<GameSession>(session);
	cout << "Receive Input..." << endl;

	// KeyCode를 받아서 게임에 적용하기
	// 0: Up, 1: Down, 2: Left, 3: Right

	enum KEY_CODE
	{
		UP = 0,
		DOWN = 1,
		LEFT = 2,
		RIGHT = 3,
	};

	int id = pkt.playerid();
	int keyCode = pkt.keycodes();

	KEY_CODE key = static_cast<KEY_CODE>(keyCode);

	cout << "PlayerId: " << id << endl;
	cout << "KeyCodes: " << key << endl;

	return false;
}


