#pragma once
#include "pch.h"
#include "Job.h"


class JobQueue
{
public:
	void Push(const JobRef& job)
	{
		WRITE_LOCK;
		_jobs.push(job);
	}

	JobRef Pop()
	{
		WRITE_LOCK;
		if (_jobs.empty())
			return nullptr;

		JobRef ref = _jobs.front();
		_jobs.pop();
		return ref;
	}

private:
	USE_LOCK;
	queue<JobRef> _jobs;
};