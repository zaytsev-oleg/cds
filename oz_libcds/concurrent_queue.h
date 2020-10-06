#pragma once

#include <queue>
#include <deque>
#include <mutex>
#include <atomic>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <initializer_list>

template <typename _Ty> class concurrent_queue
{
	public:
		concurrent_queue() 
			: m_is_active{ true }
		{
		}

		concurrent_queue(std::initializer_list<_Ty> init_list)
			: concurrent_queue()
		{
			using namespace std;

			auto it = init_list.begin();

			while (it != init_list.end())
			{
				this->m_container.push_back(make_shared<_Ty>(*it));
				++it;
			}
		}

		~concurrent_queue()
		{
			using namespace std;

			this->m_is_active = false;
			this->m_cv.notify_all();
		}

		concurrent_queue(const concurrent_queue& other)
		{
			*this = other;
		}
		
		concurrent_queue(concurrent_queue&& other)
			: concurrent_queue()
		{
			using namespace std;
			*this = move(other);
		}

		concurrent_queue& operator=(const concurrent_queue& other)
		{
			using namespace std;

			if (this == &other)
				return *this;

			this->m_container.clear();
			auto it = other.m_container.begin();

			while (it != other.m_container.end())
			{
				this->m_container.push_back(move(make_shared<_Ty>(**it)));
				++it;
			}

			return *this;
		}

		concurrent_queue& operator=(concurrent_queue&& other)
		{
			using namespace std;

			if (this == &other)
				return *this;

			lock(this->m_mutex, other.m_mutex);

			lock_guard<mutex> lock1(this->m_mutex, adopt_lock);
			lock_guard<mutex> lock2(other.m_mutex, adopt_lock);

			this->m_container = move(other.m_container);
		}

		bool try_pop(_Ty& res)
		{
			using namespace std;

			lock_guard<mutex> lock(this->m_mutex);

			if (this->m_is_active && this->m_container.size())
			{
				res = move(*this->m_container.front());
				this->m_container.pop_front();

				return true;
			}

			return false;
		}

		std::shared_ptr<_Ty> try_pop()
		{
			using namespace std;

			lock_guard<mutex> lock(this->m_mutex);

			if (this->m_is_active && this->m_container.size())
			{
				shared_ptr<_Ty> res = this->m_container.front();
				this->m_container.pop_front();

				return res;
			}

			return shared_ptr<_Ty>();
		}

		bool wait_and_pop(_Ty& res)
		{
			using namespace std;

			unique_lock lock(this->m_mutex);
			this->m_cv.wait(
				lock, [&]()->bool { return !m_is_active || m_container.size(); });
			
			if (this->m_is_active)
			{
				res = move(*this->m_container.front());
				this->m_container.pop_front();

				return true;
			}

			return false;
		}

		std::shared_ptr<_Ty> wait_and_pop()
		{
			using namespace std;

			unique_lock<mutex> lock(this->m_mutex);
			this->m_cv.wait(
				lock, [&]()->bool { return !m_is_active || m_container.size(); });

			if (this->m_is_active)
			{
				shared_ptr<_Ty> res = this->m_container.front();
				this->m_container.pop_front();

				return res;
			}

			return shared_ptr<_Ty>();
		}

		void push(_Ty item, bool& res)
		{
			using namespace std;

			if (this->m_is_active)
			{
				{
					shared_ptr<_Ty> ptr = make_shared<_Ty>(move(item));

					lock_guard<mutex> lock(this->m_mutex);
					this->m_container.push_back(ptr);
				}

				res = true;
				this->m_cv.notify_one();

				return;
			}

			res = false;
		}

		int clear()
		{
			using namespace std;

			lock_guard<mutex> lock(this->m_mutex);

			int res = this->m_container.size();
			this->m_container.clear();

			return res;
		}

		void swap(concurrent_queue& other)
		{
			using namespace std;

			lock(this->m_mutex, other.m_mutex);

			lock_guard<mutex> lock1(this->m_mutex, adopt_lock);
			lock_guard<mutex> lock2(other.m_mutex, adopt_lock);

			this->m_container.swap(other.m_container);
		}

		bool is_active() const
		{
			return this->m_is_active;
		}

	private:
		std::deque<std::shared_ptr<_Ty>> m_container;
		mutable std::mutex				 m_mutex;
		std::condition_variable			 m_cv;
		std::atomic<bool>				 m_is_active;
};
