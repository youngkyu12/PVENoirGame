#include "pch.h"
#include "JobTimer.h"
#include "JobQueue.h"

void JobTimer::Reserve(uint64 tickAfter, weak_ptr<JobQueue> owner, JobRef job)
{
	const uint64 executeTick = ::GetTickCount64() + tickAfter;
}

void JobTimer::Distribute(uint64 now)
{
}

void JobTimer::Clear()
{
}