#pragma once
#include "Protocol.pb.h"

using PacketHandlerFunc = std::function<bool(PacketSessionRef&, BYTE*, int32)>;
extern PacketHandlerFunc GPacketHandler[UINT16_MAX];


enum : uint16
{
	PKT_C_LOGIN = 1000,
	PKT_S_LOGIN = 1001,
	PKT_C_ENTER_GAME = 1002,
	PKT_S_ENTER_GAME = 1003,
	PKT_C_GAME_START = 1004,
	PKT_S_GAME_START = 1005,
	PKT_C_CLIENT_READY = 1006,
	PKT_C_INPUT = 1007,
	PKT_S_FRAME_STATE = 1008,
	PKT_S_FORCED_TRANSFORM = 1009,
	PKT_C_USE_ITEM = 1010,
	PKT_C_DEBUG_COMMAND = 1011,
};

// 자동화 예정
// Custom Packet Handler
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len);
bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt);
bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt);
bool Handle_S_GAME_START(PacketSessionRef& session, Protocol::S_GAME_START& pkt);
bool Handle_S_FRAME_STATE(PacketSessionRef& session, Protocol::S_FRAME_STATE& pkt);
bool Handle_S_FORCED_TRANSFORM(PacketSessionRef& session, Protocol::S_FORCED_TRANSFORM& pkt);


class ServerPacketHandler
{
public:
	//자동화 예정
	static void Init()
	{
		for (int32 i = 0; i < UINT16_MAX; ++i)
			GPacketHandler[i] = Handle_INVALID;
		GPacketHandler[PKT_S_LOGIN] = [](PacketSessionRef& session, BYTE* buffer, int32 len){return HandlePacket<Protocol::S_LOGIN>(Handle_S_LOGIN, session, buffer, len);};
		GPacketHandler[PKT_S_ENTER_GAME] = [](PacketSessionRef& session, BYTE* buffer, int32 len){return HandlePacket<Protocol::S_ENTER_GAME>(Handle_S_ENTER_GAME, session, buffer, len);};
		GPacketHandler[PKT_S_GAME_START] = [](PacketSessionRef& session, BYTE* buffer, int32 len){return HandlePacket<Protocol::S_GAME_START>(Handle_S_GAME_START, session, buffer, len);};
		GPacketHandler[PKT_S_FRAME_STATE] = [](PacketSessionRef& session, BYTE* buffer, int32 len){return HandlePacket<Protocol::S_FRAME_STATE>(Handle_S_FRAME_STATE, session, buffer, len);};
		GPacketHandler[PKT_S_FORCED_TRANSFORM] = [](PacketSessionRef& session, BYTE* buffer, int32 len){return HandlePacket<Protocol::S_FORCED_TRANSFORM>(Handle_S_FORCED_TRANSFORM, session, buffer, len);};

	}
	static bool HandlePacket(PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
		return GPacketHandler[header->id](session, buffer, len);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_LOGIN& pkt) { return _MakeSendBuffer(pkt, PKT_C_LOGIN); }
	static SendBufferRef MakeSendBuffer(Protocol::C_ENTER_GAME& pkt) { return _MakeSendBuffer(pkt, PKT_C_ENTER_GAME); }
	static SendBufferRef MakeSendBuffer(Protocol::C_GAME_START& pkt) { return _MakeSendBuffer(pkt, PKT_C_GAME_START); }
	static SendBufferRef MakeSendBuffer(Protocol::C_CLIENT_READY& pkt) { return _MakeSendBuffer(pkt, PKT_C_CLIENT_READY); }
	static SendBufferRef MakeSendBuffer(Protocol::C_INPUT& pkt) { return _MakeSendBuffer(pkt, PKT_C_INPUT); }
	static SendBufferRef MakeSendBuffer(Protocol::C_USE_ITEM& pkt) { return _MakeSendBuffer(pkt, PKT_C_USE_ITEM); }
	static SendBufferRef MakeSendBuffer(Protocol::C_DEBUG_COMMAND& pkt) { return _MakeSendBuffer(pkt, PKT_C_DEBUG_COMMAND); }

private:
	template<typename PacketType, typename ProcessFunc>
	static bool HandlePacket(ProcessFunc func, PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketType pkt;
		if (pkt.ParseFromArray(buffer + sizeof(PacketHeader), len - sizeof(PacketHeader)) == false)
			return false;

		return func(session, pkt);
	}


	template<typename T>
	static SendBufferRef _MakeSendBuffer(T& pkt, uint16 pktId)
	{
		const uint16 dataSize = static_cast<uint16>(pkt.ByteSizeLong());
		const uint16 packetSize = dataSize + sizeof(PacketHeader);

		SendBufferRef sendBuffer = GSendBufferManager->Open(packetSize);
		PacketHeader* header = reinterpret_cast<PacketHeader*>(sendBuffer->Buffer());
		header->size = packetSize;
		header->id = pktId;
		ASSERT_CRASH(pkt.SerializeToArray(&header[1], dataSize));
		sendBuffer->Close(packetSize);

		return sendBuffer;
	}
};
