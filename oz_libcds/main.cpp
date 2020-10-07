#define _ENABLE_ATOMIC_ALIGNMENT_FIX

#include <initializer_list>
#include <iostream>
#include <thread>

#include "concurrent_stack.h"
#include "concurrent_queue.h"
#include "concurrent_queue_fast.h"
#include "concurrent_map.h"
#include "concurrent_list.h"

#include "lock_free_stack_pop_count.h"
#include "lock_free_stack_hp.h"
#include "lock_free_stack_ref_count.h"
#include "lock_free_queue_ref_count.h"
#include "exp_stack_atomic_shared_ptr.h"

#define stop __asm nop

int main()
{
	using namespace std;

	{
		int len = 5;
		long chunk = 1 * 1000000;

		initializer_list<int> init_list{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
		lock_free_queue_ref_count<int> myBag(init_list);

		auto reader = [&](double d)->void
		{
			printf("reader_%i: started\n", this_thread::get_id());

			long i = 0;
			long n = d * len * chunk;

			while (n)
			{
				if (!myBag.is_active())
					break;

				shared_ptr<int> res = myBag.pop();

				if (res)
				{
					--n;
					++i;
				}
			}

			printf("reader_%i: ended, i = %i\n", this_thread::get_id(), i);
		};

		auto writer = [&](int value)->void
		{
			long n = chunk;

			printf("writer_%i: started, n = %i\n", this_thread::get_id(), n);

			while (n)
			{
				if (!myBag.is_active())
					break;

				bool res = false;
				myBag.push(value, res);

				--n;
			}

			printf("writer_%i: ended, n = %i\n", this_thread::get_id(), n);
		};

		vector<thread> th_writers;
		vector<thread> th_readers;

		for (int i = 0; i < len; i++)
		{
			th_writers.push_back(thread(writer, i));
		}

		this_thread::sleep_for(1s);
		
		th_readers.push_back(thread(reader, 0.1));
		th_readers.push_back(thread(reader, 0.1));
		th_readers.push_back(thread(reader, 0.1));
		th_readers.push_back(thread(reader, 0.1));
		th_readers.push_back(thread(reader, 0.1));
		th_readers.push_back(thread(reader, 0.1));
		th_readers.push_back(thread(reader, 0.1));
		th_readers.push_back(thread(reader, 0.1));
		th_readers.push_back(thread(reader, 0.1));
		th_readers.push_back(thread(reader, 0.1));

		for (auto& th : th_writers)
		{
			th.join();
		}

		for (auto& th : th_readers)
		{
			th.join();
		}

		int k = myBag.clear();
		printf("k = %i\n", k);

		stop
	}

	stop

	return 0;
}
