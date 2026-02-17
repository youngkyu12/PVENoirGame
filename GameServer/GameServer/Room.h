#pragma once
#include "JobSerializer.h"

class Room : public JobSerializer
{


public:
	// 싱글쓰레드 환경인 것처럼 코딩
	void Enter(PlayerRef player);
	void Leave(PlayerRef player);
	void BroadCast(SendBufferRef sendBuffer);

public:
	// 멀티쓰레드 환경에서는 Job으로 접근
	virtual void FlushJob() override;


	 
private:
	map<uint64, PlayerRef> _players;
};

extern shared_ptr<Room> GRoom;

