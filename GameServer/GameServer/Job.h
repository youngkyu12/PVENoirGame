#pragma once

//C++ 11 apply

template<int... Remains>
struct seq
{
};

template<int N, int... Remains>
struct gen_seq : gen_seq<N - 1, N - 1, Remains...>
{
};

template<int... Remains>
struct gen_seq<0, Remains...> : seq<Remains...>
{
};

template<typename Ret, typename... Args>
void xapply(Ret(*func)(Args...), std::tuple<Args...>& tuple)
{
	xapply_helper(func, tuple, gen_seq<sizeof...(Args)>());
}

template<typename F, typename... Args, int... Is>
void xapply_helper(F func, std::tuple<Args...>& tuple, seq<Is...>)
{
	func(std::get<Is>(tuple)...);
}

template<typename T, typename Ret, typename... Args>
void xapply(T* obj, Ret(T::*func)(Args...), std::tuple<Args...>& tuple)
{
	xapply_helper(obj, func, tuple, gen_seq<sizeof...(Args)>());
}

template<typename T, typename F, typename... Args, int... Is>
void xapply_helper(T* obj, F func, std::tuple<Args...>& tuple, seq<Is...>)
{
	(obj->*func)(std::get<Is>(tuple)...);
}


class IJob
{
public:
	virtual void Execute() {}
};

template<typename Ret, typename... Args>
class FuncJob : public IJob
{
	using FuncType = Ret(*)(Args...);
public:
	FuncJob(FuncType func, Args... args) : _func(func), _tuple(args...) {}

	Ret operator()(Args... args)
	{
		//std::apply(_func, _tuple);
	}

	virtual void Execute() override
	{
		xapply(_func, _tuple);
	}

private:
	FuncType _func;
	std::tuple<Args...> _tuple;
};

template<typename T, typename Ret, typename... Args>
class MemberJob : public IJob
{
	using FuncType = Ret(T::*)(Args...);
public:
	MemberJob(T* obj, FuncType func, Args... args) : _obj(obj), _func(func), _tuple(args...) {}

	Ret operator()(Args... args)
	{
		//std::apply(obj, _func, _tuple);
	}

	virtual void Execute() override
	{
		xapply(_obj, _func, _tuple);
	}

private:
	T* _obj;
	FuncType _func;
	std::tuple<Args...> _tuple;
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