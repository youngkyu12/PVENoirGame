#include "pch.h"
#include <iostream>

#include <atomic>
#include <mutex>
#include <thread>
#include <future>
#include <iostream>
#include <chrono>

#include "ThreadManager.h"
#include "Memory.h"
#include "Allocator.h"

class Knight
{
public:
	int32 _hp = rand() % 1000;
};

int main()
{

	for (int32 i = 0; i < 5; ++i)
	{
		GThreadManager->Launch([]()
		{
				while (true)
				{
					Knight* knight = xnew<Knight>();

					cout << knight->_hp << endl;

					this_thread::sleep_for(chrono::milliseconds(10ms));

					xdelete(knight);
				}
		});
	}



	GThreadManager->Join();
}