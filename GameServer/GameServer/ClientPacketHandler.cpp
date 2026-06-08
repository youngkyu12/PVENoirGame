#include "pch.h"
#include "ClientPacketHandler.h"
#include "Player.h"
#include "Enemy.h"
#include "Room.h"
#include "GameSession.h"

PacketHandlerFunc GPacketHandler[UINT16_MAX];

// ������ �۾��ڰ� ����

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
	// TODO : �α� �����
	return false;
}

bool Handle_C_LOGIN(PacketSessionRef& session, Protocol::C_LOGIN& pkt)
{
	GameSessionRef gameSession = static_pointer_cast<GameSession>(session);

	// TODO: Validation Check

	Protocol::S_LOGIN loginPkt;
	loginPkt.set_success(true);

	// DB���� �÷��� ���� ��������
	// GameSession�� �÷��� ���� ���� (�޸�)

	// ID 발급
	static Atomic<uint64> idGenerator = 0;
	uint64 playerId = idGenerator.fetch_add(1);

	if (playerId < static_cast<uint64>(MaxPlayers)) // �ο��� ����
	{
		loginPkt.set_playerid(playerId);
		auto player = loginPkt.add_players();
		player->set_id(playerId);
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

	if (!loginPkt.success())
		session->Disconnect(L"Room full");

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

bool Handle_C_CLIENT_READY(PacketSessionRef& session, Protocol::C_CLIENT_READY& pkt)
{
	uint32 localPlayerId = pkt.playerid();

	GRoom->DoAsync(&Room::SetPlayerReady, pkt.ready(), localPlayerId);

	return false;
}

bool Handle_C_INPUT(PacketSessionRef& session, Protocol::C_INPUT& pkt)
{
	GameSessionRef gameSession = static_pointer_cast<GameSession>(session);

	// TODO: �ش� �÷��̾��� id�� ��ġ ������Ʈ ����
	GRoom->DoAsync(&Room::ProcessInput, pkt.playerid(), pkt.keycodes(), 
		pkt.deltax(), pkt.deltay(), pkt.clientdeltatime());

	return true;
}

bool Handle_C_DEBUG_COMMAND(PacketSessionRef& session, Protocol::C_DEBUG_COMMAND& pkt)
{
	if (pkt.commandtype() == Protocol::DEBUG_COMMAND_KILL_MEGA5_ENEMIES)
	{
		GRoom->DoAsync(&Room::DebugKillMega5Enemies);
	}
	else if (pkt.commandtype() == Protocol::DEBUG_COMMAND_TELEPORT_TO_MEGA_GRID)
	{
		GameSessionRef gameSession = static_pointer_cast<GameSession>(session);
		if (gameSession->_currentPlayer)
		{
			uint64 pid = gameSession->_currentPlayer->GetObjectId();
			int grid = pkt.megagridnumber();
			GRoom->DoAsync(&Room::DebugTeleportToMegaGrid, pid, grid);
		}
	}
	else if (pkt.commandtype() == Protocol::DEBUG_COMMAND_DAMAGE_BOSS)
	{
		GRoom->DoAsync(&Room::DebugDamageBoss);
	}
	return true;
}


