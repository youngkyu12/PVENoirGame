#include "pch.h"
#include "ServerPacketHandler.h"
#include "BufferReader.h"
#include "Protocol.pb.h"
#include "GameMath.h"

#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <vector>

PacketHandlerFunc GPacketHandler[UINT16_MAX];

namespace
{
	struct ClientState
	{
		PacketSessionRef session;
		int index = 0;
		uint32 playerId = 0;
		bool loggedIn = false;
		bool gameStarted = false;
		float accumulatedTurnDeg = 0.0f;
		bool lapDone = false;
		std::unordered_map<uint64, GameMath::Vec3> players;
		std::unordered_map<uint64, GameMath::Vec3> enemies;
	};

	std::mutex g_stressLock;
	std::unordered_map<uint64, ClientState> g_clients;
	int g_nextClientIndex = 0;
	size_t g_activeClientOrder = 0;
	auto g_lastRender = std::chrono::steady_clock::now();

	uint64 SessionKey(const PacketSessionRef& s)
	{
		return reinterpret_cast<uint64>(s.get());
	}

	void SendEnterGame(const PacketSessionRef& session, uint32 playerId)
	{
		Protocol::C_ENTER_GAME pkt;
		pkt.set_playerid(playerId);
		pkt.set_playerweapon(0x1010);
		pkt.set_ready(true);
		auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
		session->Send(sendBuffer);
	}

	void SendGameStart(const PacketSessionRef& session, uint32 playerId)
	{
		Protocol::C_GAME_START pkt;
		pkt.set_playerid(playerId);
		pkt.set_playerweapon(0x1010);
		pkt.set_ready(true);
		auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
		session->Send(sendBuffer);
	}

	void SendMoveInput(const PacketSessionRef& session, uint32 playerId, bool move)
	{
		Protocol::C_INPUT pkt;
		pkt.set_playerid(playerId);

		int keyCodes = 0;
		if (move)
		{
			keyCodes |= (1 << 0); // forward
			keyCodes |= (1 << 8); // run
		}

		pkt.set_keycodes(keyCodes);
		pkt.set_deltax(move ? 2.5f : 0.0f);
		pkt.set_deltay(0.0f);

		auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
		session->Send(sendBuffer);
	}

	void RenderAsciiMap(const std::vector<ClientState>& clients)
	{
		constexpr int kMapW = 80;
		constexpr int kMapH = 32;
		constexpr float kMinX = -600.0f;
		constexpr float kMaxX = 600.0f;
		constexpr float kMinZ = -200.0f;
		constexpr float kMaxZ = 1000.0f;

		std::vector<std::string> grid(kMapH, std::string(kMapW, '.'));

		auto PutPoint = [&](const GameMath::Vec3& p, char c)
			{
				const float tx = (p.x - kMinX) / (kMaxX - kMinX);
				const float tz = (p.z - kMinZ) / (kMaxZ - kMinZ);
				int x = static_cast<int>(tx * (kMapW - 1));
				int y = static_cast<int>((1.0f - tz) * (kMapH - 1));
				if (x < 0 || x >= kMapW || y < 0 || y >= kMapH)
					return;
				grid[y][x] = c;
			};

		for (const auto& c : clients)
			for (const auto& e : c.enemies)
				PutPoint(e.second, 'E');

		for (const auto& c : clients)
			for (const auto& p : c.players)
				PutPoint(p.second, 'P');

		std::cout << "\x1B[2J\x1B[H";
		std::cout << "[Stress] Top-Down Map (\x1B[32mP\x1B[0m=Player, \x1B[31mE\x1B[0m=Visible Enemy)\n";
		for (const std::string& row : grid)
		{
			for (char ch : row)
			{
				if (ch == 'P')
					std::cout << "\x1B[32mP\x1B[0m";
				else if (ch == 'E')
					std::cout << "\x1B[31mE\x1B[0m";
				else
					std::cout << ch;
			}
			std::cout << '\n';
		}

		for (const auto& c : clients)
		{
			std::cout << "Client#" << c.index
				<< " playerId=" << c.playerId
				<< " visibleEnemies=" << c.enemies.size()
				<< " lapDone=" << (c.lapDone ? 1 : 0)
				<< '\n';
		}
	}
}

bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	UNREFERENCED_PARAMETER(session);
	UNREFERENCED_PARAMETER(buffer);
	UNREFERENCED_PARAMETER(len);
	return false;
}

bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt)
{
	if (pkt.success() == false)
		return true;

	{
		std::lock_guard<std::mutex> lock(g_stressLock);
		auto it = g_clients.find(SessionKey(session));
		if (it != g_clients.end())
		{
			it->second.playerId = pkt.playerid();
			it->second.loggedIn = true;
		}
	}

	SendEnterGame(session, pkt.playerid());
	return true;
}

bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt)
{
	UNREFERENCED_PARAMETER(pkt);

	uint32 playerId = 0;
	{
		std::lock_guard<std::mutex> lock(g_stressLock);
		auto it = g_clients.find(SessionKey(session));
		if (it != g_clients.end())
			playerId = it->second.playerId;
	}

	SendGameStart(session, playerId);
	return true;
}

bool Handle_S_GAME_START(PacketSessionRef& session, Protocol::S_GAME_START& pkt)
{
	UNREFERENCED_PARAMETER(pkt);

	std::lock_guard<std::mutex> lock(g_stressLock);
	auto it = g_clients.find(SessionKey(session));
	if (it != g_clients.end())
		it->second.gameStarted = true;

	return false;
}

bool Handle_S_FRAME_STATE(PacketSessionRef& session, Protocol::S_FRAME_STATE& pkt)
{
	std::lock_guard<std::mutex> lock(g_stressLock);
	auto it = g_clients.find(SessionKey(session));
	if (it == g_clients.end())
		return false;

	it->second.players.clear();
	it->second.enemies.clear();

	for (const auto& player : pkt.players())
	{
		const auto& p = player.transform().position();
		it->second.players[player.id()] = GameMath::Vec3(p.x(), p.y(), p.z());
	}

	for (const auto& enemy : pkt.enemies())
	{
		const auto& p = enemy.transform().position();
		it->second.enemies[enemy.id()] = GameMath::Vec3(p.x(), p.y(), p.z());
	}

	return false;
}

void RegisterStressSession(PacketSessionRef session)
{
	if (!session)
		return;

	std::lock_guard<std::mutex> lock(g_stressLock);
	ClientState state{};
	state.session = session;
	state.index = g_nextClientIndex++;
	g_clients[SessionKey(session)] = std::move(state);
}

void UnregisterStressSession(PacketSessionRef session)
{
	if (!session)
		return;

	std::lock_guard<std::mutex> lock(g_stressLock);
	g_clients.erase(SessionKey(session));
}

void TickStressTest()
{
	std::vector<ClientState> clients;
	{
		std::lock_guard<std::mutex> lock(g_stressLock);
		for (auto& kv : g_clients)
			clients.push_back(kv.second);
	}

	if (clients.empty())
		return;

	std::sort(clients.begin(), clients.end(), [](const ClientState& a, const ClientState& b) { return a.index < b.index; });

	std::vector<ClientState*> movers;
	for (auto& c : clients)
	{
		if (c.gameStarted && !c.lapDone && c.session)
			movers.push_back(&c);
	}

	if (!movers.empty())
	{
		if (g_activeClientOrder >= movers.size())
			g_activeClientOrder = 0;

		for (size_t i = 0; i < movers.size(); ++i)
		{
			ClientState* c = movers[i];
			const bool active = (i == g_activeClientOrder);
			SendMoveInput(c->session, c->playerId, active);

			if (active)
			{
				c->accumulatedTurnDeg += 2.5f;
				if (c->accumulatedTurnDeg >= 360.0f)
				{
					c->lapDone = true;
					c->accumulatedTurnDeg = 0.0f;
					++g_activeClientOrder;
				}
			}
		}
	}

	{
		std::lock_guard<std::mutex> lock(g_stressLock);
		for (const auto& c : clients)
		{
			auto it = g_clients.find(SessionKey(c.session));
			if (it != g_clients.end())
			{
				it->second.accumulatedTurnDeg = c.accumulatedTurnDeg;
				it->second.lapDone = c.lapDone;
			}
		}
	}

	const auto now = std::chrono::steady_clock::now();
	if (std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastRender).count() >= 200)
	{
		RenderAsciiMap(clients);
		g_lastRender = now;
	}
}


