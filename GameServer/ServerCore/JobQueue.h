#pragma once
#include "Job.h"
#include "LockQueue.h"


class JobQueue : public enable_shared_from_this<JobQueue>
{
public:
	void DoAsync(CallBackType&& callback)
	{
		Push(ObjectPool<Job>::MakeShared(std::move(callback)));
	}

	template<typename T, typename Ret, typename... Args>
	void DoAsync(Ret(T:: *memFunc)(Args...), Args... args)
	{
		shared_ptr<T>owner = static_pointer_cast<T>(shared_from_this());
		Push(ObjectPool<Job>::MakeShared(owner, memFunc, std::forward<Args>(args)...));

	}


private:
	void Push(JobRef job);

public:
	void Execute();

protected:
	LockQueue<JobRef> _jobs;
	Atomic<int32>		_jobCount{ 0 };
};

