#pragma once

#include <deque>
#include <mutex>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <initializer_list>

template <typename _Ty> class concurrent_stack
{
	public:
		concurrent_stack() 
			: m_is_active{ true }
		{
		};

		concurrent_stack(std::initializer_list<_Ty>& init_list)
			: concurrent_stack()
		{
			auto it = init_list.begin();

			while (it != init_list.end())
			{
				this->m_container.push_front(*it);
				++it;
			}
		}
		
		~concurrent_stack()
		{
			using namespace std;

			this->m_is_active = false;
			this->m_cv.notify_all();
		};
		
		concurrent_stack(const concurrent_stack& other) 
			: concurrent_stack()
		{
			*this = other;
		}

		concurrent_stack(concurrent_stack&& other) 
			: concurrent_stack()
		{
			using namespace std;
			*this = move(other);
		}

		concurrent_stack& operator=(const concurrent_stack& other)
		{
			using namespace std;

			if (this == &other)
				return *this;

			lock(this->m_mutex, other.m_mutex);

			lock_guard<mutex> lock1(this->m_mutex, adopt_lock);
			lock_guard<mutex> lock2(other.m_mutex, adopt_lock);

			this->m_container = other.m_container;

			return *this;
		}

		concurrent_stack& operator=(concurrent_stack&& other)
		{
			using namespace std;

			if (this == &other)
				return *this;

			lock(this->m_mutex, other.m_mutex);

			lock_guard<mutex> lock1(this->m_mutex, adopt_lock);
			lock_guard<mutex> lock2(other.m_mutex, adopt_lock);

			this->m_container = move(other.m_container);

			return *this;
		}

		void push(_Ty item, bool& res)
		{
			using namespace std;
			
			if (this->m_is_active)
			{
				{
					lock_guard<mutex> lock(this->m_mutex);
					this->m_container.push_front(item);
				}

				res = true;
				this->m_cv.notify_one();

				return;
			}

			res = false;
		}

		bool try_pop(_Ty& res)
		{
			using namespace std;

			lock_guard<mutex> lock(this->m_mutex);

			if (this->m_is_active && this->m_container.size())
			{
				res = move(this->get_item());
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
				return make_shared<_Ty>(move(this->get_item()));
			}

			return shared_ptr<_Ty>();
		}

		bool wait_and_pop(_Ty& result)
		{
			using namespace std;

			unique_lock lock(this->m_mutex);
			this->m_cv.wait(
				lock, [&]()->bool { return !m_is_active || m_container.size(); });
			
			if (this->m_is_active)
			{
				result = move(this->get_item());
				return true;
			}

			return false;
		}

		std::shared_ptr<_Ty> wait_and_pop()
		{
			using namespace std;

			unique_lock lock(this->m_mutex);
			this->m_cv.wait(
				lock, [&]()->bool { return !m_is_active || m_container.size(); });

			if (this->m_is_active)
			{
				return make_shared<_Ty>(move(this->get_item()));
			}

			return shared_ptr<_Ty>();
		}

		int clear()
		{
			using namespace std;

			lock_guard<mutex> lock(this->m_mutex);

			int res = this->m_container.size();
			this->m_container.clear();

			return res;
		}

		void swap(concurrent_stack& other)
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
		_Ty get_item()
		{
			using namespace std;

			_Ty item = move(this->m_container.front());
			this->m_container.pop_front();

			return move(item);
		}

		std::deque<_Ty>		    m_container;
		mutable std::mutex	    m_mutex;
		std::condition_variable m_cv;
		std::atomic<bool>	    m_is_active;
};
