#pragma once
#include "Job.h"
class Room
{
	friend class EnterJob;
	friend class LeaveJob;
	friend class BroadCastJob;

private:
	// 싱글쓰레드 환경인 것처럼 코딩
	void Enter(PlayerRef player);
	void Leave(PlayerRef player);
	void BroadCast(SendBufferRef sendBuffer);

public:
	// 멀티쓰레드 환경에서는 Job으로 접근
	void PushJob(JobRef job) { _jobs.Push(job); }
	void FlushJob();


private:
	map<uint64, PlayerRef> _players;

	JobQueue _jobs;
};

extern Room GRoom;

//Room Jobs
class EnterJob : public IJob
{
public:
	EnterJob(Room& room, PlayerRef player)
		: _room(room), _player(player)
	{
	}

	virtual void Execute() override
	{
		_room.Enter(_player);
	}
private:
	Room& _room;
	PlayerRef _player;
};

class LeaveJob : public IJob
{
public:
	LeaveJob(Room& room, PlayerRef player)
		: _room(room), _player(player)
	{
	}

	virtual void Execute() override
	{
		_room.Leave(_player);
	}
private:
	Room& _room;
	PlayerRef _player;
};

class BroadCastJob : public IJob
{
public:
	BroadCastJob(Room& room, SendBufferRef sendBuffer)
		: _room(room), _sendBuffer(sendBuffer)
	{
	}

	virtual void Execute() override
	{
		_room.BroadCast(_sendBuffer);
	}
private:
	Room& _room;
	SendBufferRef _sendBuffer;
};