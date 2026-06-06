#include "pch.h"
#include "ServerPacketHandler.h"
#include "DummyClientWorldView.h"
#include "BufferReader.h"
#include "Protocol.pb.h"
#include "GameMath.h"

#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <vector>
#include <cmath>

PacketHandlerFunc GPacketHandler[UINT16_MAX];

namespace
{
	struct AvoidanceObstacle
	{
		GameMath::Vec3 position = GameMath::Vec3::Zero();
		float radius = 1.0f;
	};

	struct StaticWorldObject
	{
		GameMath::Vec3 position = GameMath::Vec3::Zero();
		int type = 0;
	};

	struct ClientState
	{
		PacketSessionRef session;
		int index = 0;
		uint32 playerId = 0;
		bool loggedIn = false;
		bool gameStarted = false;
		bool readySent = false;
		bool gameStartRequested = false;
		bool clockwise = true;
		int lapCount = 0;
		size_t waypointIndex = 0;
		bool hasSelfTransform = false;
		GameMath::Vec3 selfPos = GameMath::Vec3::Zero();
		float selfYaw = 0.0f;
		std::chrono::steady_clock::time_point startAt = std::chrono::steady_clock::now();
		std::unordered_map<uint64, GameMath::Vec3> players;
		std::unordered_map<uint64, GameMath::Vec3> enemies;
		std::unordered_map<uint64, StaticWorldObject> staticObjects;
		std::vector<AvoidanceObstacle> staticObstacles;
	};

	std::mutex g_stressLock;
	std::unordered_map<uint64, ClientState> g_clients;
	int g_nextClientIndex = 0;
	auto g_lastRender = std::chrono::steady_clock::now();

	constexpr float kCenterX = 0.0f;
	constexpr float kCenterZ = 400.0f;
	constexpr float kSectorSize = 200.0f;
	constexpr float kWaypointReachDist = 18.0f;
	constexpr float kMaxDeltaYaw = 8.0f;
	constexpr float kAvoidLookAhead = 55.0f;
	constexpr float kDynamicAvoidRadius = 14.0f;

	float Clamp(float value, float minValue, float maxValue)
	{
		return (std::max)(minValue, (std::min)(maxValue, value));
	}

	float NormalizeSignedYaw(float yaw)
	{
		while (yaw > 180.0f)
			yaw -= 360.0f;
		while (yaw < -180.0f)
			yaw += 360.0f;
		return yaw;
	}

	float YawToTarget(const GameMath::Vec3& from, const GameMath::Vec3& to)
	{
		const float dx = to.x - from.x;
		const float dz = to.z - from.z;
		return GameMath::NormalizeYaw(atan2f(dx, dz) * GameMath::RAD_TO_DEG);
	}

	GameMath::Vec3 NormalizeXZ(const GameMath::Vec3& v)
	{
		const float len = sqrtf(v.x * v.x + v.z * v.z);
		if (len <= GameMath::kEpsilon)
			return GameMath::Vec3::Zero();
		return GameMath::Vec3(v.x / len, 0.0f, v.z / len);
	}

	float DotXZ(const GameMath::Vec3& a, const GameMath::Vec3& b)
	{
		return a.x * b.x + a.z * b.z;
	}

	GameMath::Vec3 PatrolWaypointAt(size_t index, bool clockwise)
	{
		const float left = kCenterX - kSectorSize;
		const float midX = kCenterX;
		const float right = kCenterX + kSectorSize;
		const float bottom = kCenterZ - kSectorSize;
		const float midZ = kCenterZ;
		const float top = kCenterZ + kSectorSize;

		const GameMath::Vec3 clockwisePath[8] =
		{
			{ left, 0.0f, top },
			{ midX, 0.0f, top },
			{ right, 0.0f, top },
			{ right, 0.0f, midZ },
			{ right, 0.0f, bottom },
			{ midX, 0.0f, bottom },
			{ left, 0.0f, bottom },
			{ left, 0.0f, midZ }
		};

		const size_t wrapped = index % 8;
		return clockwise ? clockwisePath[wrapped] : clockwisePath[7 - wrapped];
	}

	float StaticObstacleRadius(Protocol::BuildingType type)
	{
		switch (type)
		{
		case Protocol::BUILDING_TYPE_GRASS:
		case Protocol::BUILDING_TYPE_GROUND:
		case Protocol::BUILDING_TYPE_DIRT_ROAD:
			return 0.0f;
		case Protocol::BUILDING_TYPE_CASTLE:
			return 15.0f;
		case Protocol::BUILDING_TYPE_VILLAGE_WALL:
			return 7.0f;
		case Protocol::BUILDING_TYPE_TOWER:
			return 4.0f;
		case Protocol::BUILDING_TYPE_BUILDING1:
		case Protocol::BUILDING_TYPE_BUILDING2:
		case Protocol::BUILDING_TYPE_BUILDING3:
		case Protocol::BUILDING_TYPE_BUILDING4:
		case Protocol::BUILDING_TYPE_BUILDING5:
		case Protocol::BUILDING_TYPE_BUILDING6:
		case Protocol::BUILDING_TYPE_BUILDING7:
		case Protocol::BUILDING_TYPE_BUILDING8:
		case Protocol::BUILDING_TYPE_BUILDING9:
			return 8.0f;
		default:
			return 4.0f;
		}
	}

	std::vector<AvoidanceObstacle> BuildStaticObstacles(const Protocol::InitStruct& init)
	{
		std::vector<AvoidanceObstacle> obstacles;
		obstacles.reserve(init.buildings_size() + 1);

		for (const auto& building : init.buildings())
		{
			const float radius = StaticObstacleRadius(building.buildingtype());
			if (radius <= 0.0f)
				continue;

			const auto& p = building.transform().position();
			obstacles.push_back({ GameMath::Vec3(p.x(), p.y(), p.z()), radius });
		}

		// The center 200 x 200 sector is intentionally excluded from patrol.
		obstacles.push_back({ GameMath::Vec3(kCenterX, 0.0f, kCenterZ), 145.0f });
		return obstacles;
	}

	GameMath::Vec3 AvoidObstacles(
		const ClientState& client,
		const GameMath::Vec3& desiredDirection)
	{
		GameMath::Vec3 direction = NormalizeXZ(desiredDirection);
		if (direction.LengthSq() <= 1e-8f)
			return direction;

		auto ApplyAvoidance = [&](const GameMath::Vec3& obstaclePos, float radius)
			{
				const GameMath::Vec3 toObstacle = obstaclePos - client.selfPos;
				const float forwardDist = DotXZ(toObstacle, direction);
				if (forwardDist < -radius || forwardDist > kAvoidLookAhead + radius)
					return;

				const GameMath::Vec3 closest = client.selfPos + direction * forwardDist;
				const float lateralSq = GameMath::DistSqXZ(closest, obstaclePos);
				const float avoidRadius = radius + 8.0f;
				if (lateralSq > avoidRadius * avoidRadius)
					return;

				GameMath::Vec3 side(direction.z, 0.0f, -direction.x);
				if (DotXZ(side, obstaclePos - client.selfPos) > 0.0f)
					side = -side;

				const float strength = Clamp((avoidRadius - sqrtf((std::max)(0.0f, lateralSq))) / avoidRadius, 0.0f, 1.0f);
				direction = NormalizeXZ(direction + side * (0.85f + strength));
			};

		for (const AvoidanceObstacle& obstacle : client.staticObstacles)
			ApplyAvoidance(obstacle.position, obstacle.radius);

		for (const auto& player : client.players)
		{
			if (player.first == client.playerId)
				continue;
			ApplyAvoidance(player.second, kDynamicAvoidRadius);
		}

		for (const auto& enemy : client.enemies)
			ApplyAvoidance(enemy.second, kDynamicAvoidRadius);

		return direction;
	}

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

	void SendClientReady(const PacketSessionRef& session, uint32 playerId)
	{
		Protocol::C_CLIENT_READY pkt;
		pkt.set_playerid(playerId);
		pkt.set_ready(true);
		auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
		session->Send(sendBuffer);
	}

	void SendGameStartRequest(const PacketSessionRef& session, uint32 playerId)
	{
		Protocol::C_GAME_START pkt;
		pkt.set_playerid(playerId);
		pkt.set_playerweapon(0x1010);
		pkt.set_ready(true);
		auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
		session->Send(sendBuffer);
	}

	void SendMoveInput(const PacketSessionRef& session, uint32 playerId, bool move, float deltaYaw)
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
		pkt.set_deltax(move ? deltaYaw : 0.0f);
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
				<< " lapCount=" << c.lapCount
				<< " direction=" << (c.clockwise ? "CW" : "CCW")
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
	bool sendGameStart = false;
	{
		std::lock_guard<std::mutex> lock(g_stressLock);
		auto it = g_clients.find(SessionKey(session));
		if (it != g_clients.end())
		{
			playerId = it->second.playerId;
			sendGameStart = !it->second.gameStartRequested;
			it->second.gameStartRequested = true;
		}
	}

	if (sendGameStart)
		SendGameStartRequest(session, playerId);

	return true;
}

bool Handle_S_GAME_START(PacketSessionRef& session, Protocol::S_GAME_START& pkt)
{
	uint32 playerId = 0;
	bool sendReady = false;
	{
		std::lock_guard<std::mutex> lock(g_stressLock);
		auto it = g_clients.find(SessionKey(session));
		if (it != g_clients.end())
		{
			it->second.gameStarted = true;
			playerId = it->second.playerId;
			sendReady = !it->second.readySent;
			it->second.readySent = true;
			it->second.players.clear();
			it->second.enemies.clear();
			it->second.staticObjects.clear();

			const Protocol::InitStruct& init = pkt.initstruct();
			it->second.staticObstacles = BuildStaticObstacles(init);
			for (const auto& building : init.buildings())
			{
				const float radius = StaticObstacleRadius(building.buildingtype());
				if (radius <= 0.0f)
					continue;

				const auto& p = building.transform().position();
				it->second.staticObjects[building.id()] =
				{
					GameMath::Vec3(p.x(), p.y(), p.z()),
					static_cast<int>(building.buildingtype())
				};
			}

			for (const auto& player : init.players())
			{
				const auto& p = player.transform().position();
				const GameMath::Vec3 pos(p.x(), p.y(), p.z());
				it->second.players[player.id()] = pos;
				if (player.id() == it->second.playerId)
				{
					it->second.selfPos = pos;
					it->second.selfYaw = player.transform().yaw();
					it->second.hasSelfTransform = true;
				}
			}

			for (const auto& enemy : init.enemies())
			{
				const auto& p = enemy.transform().position();
				it->second.enemies[enemy.id()] = GameMath::Vec3(p.x(), p.y(), p.z());
			}
		}
	}

	if (sendReady)
		SendClientReady(session, playerId);

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
		const GameMath::Vec3 pos(p.x(), p.y(), p.z());
		it->second.players[player.id()] = pos;
		if (player.id() == it->second.playerId)
		{
			it->second.selfPos = pos;
			it->second.selfYaw = player.transform().yaw();
			it->second.hasSelfTransform = true;
		}
	}

	for (const auto& enemy : pkt.enemies())
	{
		const auto& p = enemy.transform().position();
		it->second.enemies[enemy.id()] = GameMath::Vec3(p.x(), p.y(), p.z());
	}

	return false;
}

bool Handle_S_FORCED_TRANSFORM(PacketSessionRef& session, Protocol::S_FORCED_TRANSFORM& pkt)
{
	UNREFERENCED_PARAMETER(session);
	UNREFERENCED_PARAMETER(pkt);
	return true;
}

void GetWorldDrawSnapshot(DrawSnapshot& outSnapshot)
{
	outSnapshot.players.clear();
	outSnapshot.enemies.clear();
	outSnapshot.staticObjects.clear();

	std::unordered_map<uint64, GameMath::Vec3> mergedPlayers;
	std::unordered_map<uint64, GameMath::Vec3> mergedEnemies;
	std::unordered_map<uint64, StaticWorldObject> mergedStaticObjects;

	{
		std::lock_guard<std::mutex> lock(g_stressLock);
		for (const auto& kv : g_clients)
		{
			const ClientState& client = kv.second;
			for (const auto& player : client.players)
				mergedPlayers[player.first] = player.second;
			for (const auto& enemy : client.enemies)
				mergedEnemies[enemy.first] = enemy.second;
			for (const auto& object : client.staticObjects)
				mergedStaticObjects[object.first] = object.second;
		}
	}

	outSnapshot.players.reserve(mergedPlayers.size());
	for (const auto& player : mergedPlayers)
		outSnapshot.players.push_back({ player.second.x, player.second.z });

	outSnapshot.enemies.reserve(mergedEnemies.size());
	for (const auto& enemy : mergedEnemies)
		outSnapshot.enemies.push_back({ enemy.second.x, enemy.second.z });

	outSnapshot.staticObjects.reserve(mergedStaticObjects.size());
	for (const auto& object : mergedStaticObjects)
		outSnapshot.staticObjects.push_back({ object.second.position.x, object.second.position.z, object.second.type });
}

int GetConnectedStressClientCount()
{
	std::lock_guard<std::mutex> lock(g_stressLock);
	return static_cast<int>(g_clients.size());
}
void RegisterStressSession(PacketSessionRef session)
{
	if (!session)
		return;

	std::lock_guard<std::mutex> lock(g_stressLock);
	ClientState state{};
	state.session = session;
	state.index = g_nextClientIndex++;
	state.clockwise = (state.index % 4) < 2;
	state.waypointIndex = static_cast<size_t>((state.index % 4) * 2);
	state.startAt = std::chrono::steady_clock::now() + std::chrono::seconds((state.index % 4) * 4);
	g_clients[SessionKey(session)] = std::move(state);
}

void UnregisterStressSession(PacketSessionRef session)
{
	if (!session)
		return;

	std::lock_guard<std::mutex> lock(g_stressLock);
	g_clients.erase(SessionKey(session));
}

void SendDebugKillMega5()
{
	PacketSessionRef target;
	{
		std::lock_guard<std::mutex> lock(g_stressLock);
		for (auto& kv : g_clients)
		{
			if (kv.second.session && kv.second.gameStarted)
			{
				target = kv.second.session;
				break;
			}
		}
	}
	if (!target) return;

	Protocol::C_DEBUG_COMMAND pkt;
	pkt.set_commandtype(Protocol::DEBUG_COMMAND_KILL_MEGA5_ENEMIES);
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	target->Send(sendBuffer);
}

void SendDebugTeleportToMegaGrid(int megaGridNumber)
{
	std::vector<PacketSessionRef> targets;
	{
		std::lock_guard<std::mutex> lock(g_stressLock);
		for (auto& kv : g_clients)
		{
			if (kv.second.session && kv.second.gameStarted)
				targets.push_back(kv.second.session);
		}
	}

	Protocol::C_DEBUG_COMMAND pkt;
	pkt.set_commandtype(Protocol::DEBUG_COMMAND_TELEPORT_TO_MEGA_GRID);
	pkt.set_megagridnumber(megaGridNumber);
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	for (auto& s : targets)
		s->Send(sendBuffer);
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

	const auto now = std::chrono::steady_clock::now();
	for (auto& c : clients)
	{
		if (!c.gameStarted || !c.session || !c.hasSelfTransform || now < c.startAt)
		{
			if (c.session)
				SendMoveInput(c.session, c.playerId, false, 0.0f);
			continue;
		}

		GameMath::Vec3 target = PatrolWaypointAt(c.waypointIndex, c.clockwise);
		if (GameMath::DistSqXZ(c.selfPos, target) <= kWaypointReachDist * kWaypointReachDist)
		{
			++c.waypointIndex;
			if ((c.waypointIndex % 8) == 0)
				++c.lapCount;
			target = PatrolWaypointAt(c.waypointIndex, c.clockwise);
		}

		const GameMath::Vec3 desiredDirection = NormalizeXZ(target - c.selfPos);
		const GameMath::Vec3 moveDirection = AvoidObstacles(c, desiredDirection);
		const float targetYaw = YawToTarget(c.selfPos, c.selfPos + moveDirection);
		const float deltaYaw = Clamp(NormalizeSignedYaw(targetYaw - c.selfYaw), -kMaxDeltaYaw, kMaxDeltaYaw);

		SendMoveInput(c.session, c.playerId, true, deltaYaw);
	}

	{
		std::lock_guard<std::mutex> lock(g_stressLock);
		for (const auto& c : clients)
		{
			auto it = g_clients.find(SessionKey(c.session));
			if (it != g_clients.end())
			{
				it->second.waypointIndex = c.waypointIndex;
				it->second.lapCount = c.lapCount;
			}
		}
	}

}


