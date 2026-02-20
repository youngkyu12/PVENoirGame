#pragma once

#include <functional>

// JOB

using CallBackType = std::function<void()>;

class Job
{
public:
	Job(CallBackType&& callback)
		: _callback(std::move(callback))
	{
	}

	template<typename T, typename Ret, typename... Args>
	Job(shared_ptr<T> owner, Ret(T::* memFunc)(Args...), Args&&... args)
	{
		_callback = [owner, memFunc, args...]()
			{
				(owner.get()->*memFunc)(args...);
			};
	}

	void Execute()
	{
		_callback();
	}

private:
	CallBackType _callback;
};