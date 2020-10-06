#pragma once

#include <initializer_list>
#include <chrono>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>

template <typename _Ty> class concurrent_queue_fast
{
	class node
	{
		public:
			node() : m_next{ nullptr }, m_data_ptr{ nullptr }
			{
			}

			node* m_next;
			std::shared_ptr<_Ty> m_data_ptr;
	};

	std::unique_lock<std::mutex> lock_tail() const
	{
		using namespace std;

		unique_lock<mutex> lock(this->m_tail_mutex);
		return move(lock);
	}

	std::unique_lock<std::mutex> lock_head() const
	{
		using namespace std;

		unique_lock<mutex> lock(this->m_head_mutex);
		return move(lock);
	}

	node* get_tail() const
	{
		using namespace std;
		
		unique_lock<mutex> lock(this->lock_tail());
		return this->m_tail;
	}

	std::unique_ptr<node> pop_head()
	{
		using namespace std;

		unique_ptr<node> head_ptr(this->m_head);
		this->m_head = this->m_head->m_next;
		
		return move(head_ptr);
	}

	std::unique_ptr<node> try_pop_head()
	{
		using namespace std;

		unique_lock<mutex> lock(this->lock_head());

		if (this->m_is_active && this->m_head != this->get_tail())
		{
			return this->pop_head();
		}

		return unique_ptr<node>();
	}

	std::unique_lock<std::mutex> wait_for_data()
	{
		using namespace std;

		unique_lock<mutex> lock(this->lock_head());
		this->m_cv.wait(lock, [&]()->bool { return !m_is_active || m_head != get_tail(); });

		return move(lock);
	}

	std::unique_ptr<node> wait_pop_head()
	{
		using namespace std;

		unique_lock<mutex> lock(this->wait_for_data());

		if (this->m_is_active)
		{
			return this->pop_head();
		}

		return unique_ptr<node>();
	}

	public:
		concurrent_queue_fast()
			: m_is_active{ true }, m_head{ new node }, m_tail{ m_head }
		{
		}

		concurrent_queue_fast(std::initializer_list<_Ty>& init_list) 
			: concurrent_queue_fast()
		{
			auto it = init_list.begin();

			while (it != init_list.end())
			{
				this->push(*it);
				++it;
			}
		}

		~concurrent_queue_fast()
		{
			using namespace std;

			this->m_is_active = false;
			this->m_cv.notify_all();

			unique_lock<mutex> lock_head(this->lock_head());

			while (this->m_head != this->get_tail())
			{
				unique_ptr<node> temp(this->pop_head());
			}

			unique_lock<mutex> lock_tail(this->lock_tail());

			delete this->m_head;
			this->m_head = this->m_tail = nullptr;
		}

		concurrent_queue_fast(const concurrent_queue_fast& other) 
			: concurrent_queue_fast()
		{
			*this = other;
		}

		concurrent_queue_fast(concurrent_queue_fast&& other) 
			: concurrent_queue_fast()
		{
			using namespace std;
			*this = move(other);
		}

		concurrent_queue_fast& operator=(const concurrent_queue_fast& other)
		{
			using namespace std;

			if (this == &other)
				return *this;
			
			scoped_lock s_lock(this->m_head_mutex, this->m_tail_mutex, other.m_head_mutex, other.m_tail_mutex);

			if (other.m_head == other.m_tail)
			{
				while (this->m_head != this->m_tail)
				{
					node* temp = this->m_head;
					this->m_head = temp->m_next;

					delete temp;
				}

				return;
			}

			if (this->m_head == this->m_tail)
			{
				node* temp = new node;
				this->m_head = temp;
				this->m_head->m_next = this->m_tail;
			}

			node* this_node = nullptr;
			node* other_node = other.m_head;

			while (other_node != other.m_tail)
			{
				this_node = this_node
					? this_node->m_next : this->m_head;

				if (this_node->m_next == this->m_tail && other_node->m_next != other.m_tail)
				{
					node* temp = new node;
					temp->m_next = this->m_tail;
					this_node->m_next = temp;
				}

				this_node->m_data_ptr = make_shared<_Ty>(*other_node->m_data_ptr);
				other_node = other_node->m_next;
			}

			if (this_node->m_next != this->m_tail)
			{
				node* temp = this_node->m_next;
				this_node->m_next = this->m_tail;

				while (temp != this->m_tail)
				{
					node* next = temp->m_next;
					delete temp;
					temp = next;
				}
			}
		}

		concurrent_queue_fast& operator=(concurrent_queue_fast&& other)
		{
			using namespace std;

			if (this == &other)
				return *this;

			scoped_lock s_lock(this->m_head_mutex, this->m_tail_mutex, other.m_head_mutex, other.m_tail_mutex);

			while (this->m_head != this->m_tail)
			{
				this->pop_head();
			}

			node* temp = this->m_head;

			this->m_head = other.m_head;
			this->m_tail = other.m_tail;

			other.m_head = temp;
			other.m_tail = temp;
		}

		void push(_Ty item, bool& res)
		{
			using namespace std;

			if (!this->m_is_active)
			{
				res = false;
				return;
			}

			shared_ptr<_Ty>  data_ptr(make_shared<_Ty>(move(item)));
			unique_ptr<node> node_ptr(make_unique<node>());

			unique_lock<mutex> lock(this->lock_tail());

			this->m_tail->m_data_ptr = move(data_ptr);
			this->m_tail->m_next = node_ptr.get();
			this->m_tail = node_ptr.get();

			node_ptr.release();
			
			lock.unlock();
			lock.release();

			this->m_cv.notify_one();
			res = true;
		}

		std::shared_ptr<_Ty> try_pop()
		{
			using namespace std;

			unique_ptr<node> const head(this->try_pop_head());

			if (head)
			{
				return move(head->m_data_ptr);
			}

			return shared_ptr<_Ty>();
		}

		bool try_pop(_Ty& res)
		{
			using namespace std;

			unique_ptr<node> const head(this->try_pop_head());

			if (head)
			{
				res = move(*head->m_data_ptr);
				return true;
			}

			return false;
		}

		std::shared_ptr<_Ty> wait_and_pop()
		{
			using namespace std;

			unique_ptr<node> const head(this->wait_pop_head());
			
			if (head)
			{
				return move(head->m_data_ptr);
			}

			return shared_ptr<_Ty>();
		}

		bool wait_and_pop(_Ty& res)
		{
			using namespace std;

			unique_ptr<node> const head(this->wait_pop_head());

			if (head)
			{
				res = move(*head->m_data_ptr);
				return true;
			}

			return false;
		}

		bool is_empty() const
		{
			using namespace std;

			unique_lock lock(this->lock_head());
			return this->m_head == this->get_tail();
		}

		bool is_active() const
		{
			return this->m_is_active;
		}

		void swap(concurrent_queue_fast& other)
		{
			using namespace std;

			scoped_lock s_lock(this->m_head_mutex, this->m_tail_mutex, other.m_head_mutex, other.m_tail_mutex);

			node* this_head = this->m_head;
			node* this_tail = this->m_tail;

			this->m_head = other.m_head;
			other.m_head = this_head;

			this->m_tail = other.m_tail;
			other.m_tail = this_tail;
		}

		int clear()
		{
			using namespace std;

			unique_lock<mutex> lock_head(this->lock_head());
			unique_lock<mutex> lock_tail(this->lock_tail());

			int j = 0;

			while (this->m_head != this->m_tail)
			{
				++j;
				this->pop_head();
			}

			return j;
		}

		void print() const
		{
			using namespace std;

			unique_lock<mutex> lock(this->lock_head());
			node* item = this->m_head;

			while (item != this->get_tail())
			{
				cout << *item->m_data_ptr << endl;
				item = item->m_next;
			}
		}
	
	private:
		mutable std::mutex      m_head_mutex;
		mutable std::mutex      m_tail_mutex;
		node*				    m_head;
		node*                   m_tail;
		std::condition_variable m_cv;
		std::atomic<bool>       m_is_active;
};
