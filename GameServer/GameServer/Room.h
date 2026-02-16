#pragma once
#include "Job.h"
class Room
{


public:
	// 싱글쓰레드 환경인 것처럼 코딩
	void Enter(PlayerRef player);
	void Leave(PlayerRef player);
	void BroadCast(SendBufferRef sendBuffer);

public:
	// 멀티쓰레드 환경에서는 Job으로 접근
	void PushJob(JobRef job) { _jobs.Push(job); }
	void FlushJob();

	template<typename T, typename Ret, typename... Args>
	void PushJob(Ret(T::*memfunc)(Args...), Args... args)
	{
		auto job = MakeShared<MemberJob<T, Ret, Args...>>(static_cast<T*>(this), memfunc, args...);
		_jobs.Push(job);

	}

private:
	map<uint64, PlayerRef> _players;

	JobQueue _jobs;
};

extern Room GRoom;

