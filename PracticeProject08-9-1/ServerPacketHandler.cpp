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
		session->Disconnect(L"Room full");
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
	// C_GAME_START is sent when the player confirms ready in the wait scene.
	return true;
}

bool Handle_S_GAME_START(PacketSessionRef& session, Protocol::S_GAME_START& pkt)
{
	GameStartData data;

	// 초기 정보를 수신받아 적용

	Protocol::InitStruct worldInit = pkt.initstruct();
	auto players = worldInit.players();
	auto enemies = worldInit.enemies();
	data.mapId = pkt.mapid();

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

       PlayerState state{};
		state.id = player.id();
		state.playerType = static_cast<uint32_t>(player.playertype());
		state.hp = player.hp();
		state.position = XMFLOAT3(position.x(), position.y(), position.z());
		state.yaw = yaw;
		state.weaponType = static_cast<EWeaponType>(player.weapontype() - 1);

		data.players.push_back(std::move(state));
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

        EnemyState state{};
		state.id = enemy.id();
		state.enemyType = static_cast<uint32_t>(enemy.enemytype());
		state.hp = enemy.hp();
		state.position = XMFLOAT3(position.x(), position.y(), position.z());
		state.yaw = yaw;
		state.weaponType = static_cast<EWeaponType>(enemy.weapontype() - 1);
		state.spawnFxType = enemy.spawnfxtype();
		state.spawnFxTick = enemy.spawnfxtick();
		state.spawnFxSerial = enemy.spawnfxserial();

		data.enemies.push_back(std::move(state));
	}

	auto items = worldInit.items();
	data.items.reserve(items.size());
	for (auto& item : items)
	{
		auto pos = item.position();
		ItemSpawnState s{};
		s.id       = item.id();
		s.kind     = static_cast<uint32_t>(item.kind());
		s.position = XMFLOAT3(pos.x(), pos.y(), pos.z());
		s.active   = item.active();
		data.items.push_back(std::move(s));
	}

	// networkQueue에 게임 시작 패킷 push
	g_NetworkQueue.PushGameStart(std::move(data));

	g_GameStarted = true;
	return true;
}

bool Handle_S_FRAME_STATE(PacketSessionRef& session, Protocol::S_FRAME_STATE& pkt)
{
	FrameSnapshot data;

	data.frameId = pkt.servertick();

	auto players = pkt.players();
	auto enemies = pkt.enemies();
	auto bullets = pkt.bullets();

	for (auto& player : players)
	{
		auto transform = player.transform();
		auto position = transform.position();
		auto yaw = transform.yaw();
		auto animation = player.animation();
		auto weaponType = player.weapontype();

		AnimationState animState;
		animState.stateCode = animation.statecode(); // 변환 없이 저장
		animState.animTick = static_cast<int>(animation.animationtick());

		EWeaponType eweaponType = static_cast<EWeaponType>(weaponType - 1);

		PlayerState state{};
		state.id = player.id();
		state.playerType = static_cast<uint32_t>(player.playertype());
		state.hp = player.hp();
		state.position = XMFLOAT3(position.x(), position.y(), position.z());
		state.yaw = yaw;
		state.animation = animState;
		state.weaponType = eweaponType;

		for (auto& inv : player.inventory())
		{
			InventoryEntryState e{};
			e.kind  = static_cast<uint32_t>(inv.kind());
			e.count = inv.count();
			state.inventory.push_back(e);
		}

		data.players.push_back(std::move(state));
	}

	for (auto& enemy : enemies)
	{
		auto transform = enemy.transform();
		auto position = transform.position();
		auto yaw = transform.yaw();
		auto animation = enemy.animation();
		auto weaponType = enemy.weapontype();

		AnimationState animState;
		animState.stateCode = animation.statecode(); // 변환 없이 저장
		animState.animTick = static_cast<int>(animation.animationtick());

		EWeaponType eweaponType = static_cast<EWeaponType>(weaponType - 1);

		EnemyState state{};
		state.id = enemy.id();
		state.enemyType = static_cast<uint32_t>(enemy.enemytype());
		state.hp = enemy.hp();
		state.position = XMFLOAT3(position.x(), position.y(), position.z());
		state.yaw = yaw;
		state.animation = animState;
		state.weaponType = eweaponType;
		state.spawnFxType = enemy.spawnfxtype();
		state.spawnFxTick = enemy.spawnfxtick();
		state.spawnFxSerial = enemy.spawnfxserial();

		data.enemies.push_back(std::move(state));
	}

	data.bullets.reserve(bullets.size());
	for (auto& bullet : bullets)
	{
		auto position = bullet.position();
		auto velocity = bullet.velocity();

		BulletState b{};
		b.id = bullet.id();
		b.ownerId = bullet.ownerid();
		b.bulletType = static_cast<uint32_t>(bullet.bullettype());
		b.position = XMFLOAT3(position.x(), position.y(), position.z());
		b.velocity = XMFLOAT3(velocity.x(), velocity.y(), velocity.z());

		data.bullets.push_back(std::move(b));
	}

	data.bossRoomState = static_cast<uint32_t>(pkt.bossroomstate());

	auto items = pkt.items();
	data.items.reserve(items.size());
	for (auto& item : items)
	{
		auto pos = item.position();

		ItemSpawnState s{};
		s.id       = item.id();
		s.kind     = static_cast<uint32_t>(item.kind());
		s.position = XMFLOAT3(pos.x(), pos.y(), pos.z());
		s.active   = item.active();

		data.items.push_back(std::move(s));
	}

	g_NetworkQueue.PushFrameState(std::move(data));
	return false;
}

bool Handle_S_FORCED_TRANSFORM(PacketSessionRef& session, Protocol::S_FORCED_TRANSFORM& pkt)
{
	UNREFERENCED_PARAMETER(session);

	ForcedTransformEvent event{};
	event.playerId = pkt.playerid();
	event.yawDelta = pkt.yawdelta();
	event.reason = static_cast<uint32_t>(pkt.reason());

	g_NetworkQueue.PushForcedTransform(std::move(event));
	return true;
}

