#pragma once
class IJob
{
public:
	virtual void Execute() {}
};

class HealJob : public IJob
{
public:
	virtual void Execute() override
	{
		// Heal logic here
		cout << _target << "한테 힐" << _healValue << "만큼 줌" << endl;
	}
public:
	uint64 _target = 0;
	uint32 _healValue = 0;
};

class AttackJob : public IJob
{
public:
	virtual void Execute() override
	{
		// Attack logic here
		cout << _attacker << "가" << _damageValue << "만큼 공격" << endl;
	}
public:
	uint64 _attacker = 0;
	uint32 _damageValue = 0;
};

using JobRef = shared_ptr<IJob>;

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