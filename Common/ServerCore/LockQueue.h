#pragma once

template<typename T>
class LockQueue
{
public:
	void Push(const T& job)
	{
		WRITE_LOCK;
		_items.push(job);
	}

	T Pop()
	{
		WRITE_LOCK;
		if (_items.empty())
			return T();

		T ref = _items.front();
		_items.pop();
		return ref;
	}

	void Clear()
	{
		WRITE_LOCK;
		_items = Queue<T>();
	}

	void PopAll(OUT Vector<T>& outItem)
	{
		WRITE_LOCK;
		while (T item = Pop())
			outItem.push_back(item);
	}

private:
	USE_LOCK;
	Queue<T> _items;
};