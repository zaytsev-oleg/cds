#pragma once

#include <mutex>
#include <memory>
#include <atomic>
#include <initializer_list>

template<typename _Ty> class concurrent_list
{
	class node
	{
		public:
			node() 
				: m_data_ptr{ nullptr }, m_next { nullptr }
			{
			}

			node(const _Ty& item) 
				: m_data_ptr{ std::make_shared<_Ty>(std::move(item)) }, m_next { nullptr }
			{
			}

			node(const node&) = delete;
			node& operator=(const node&) = delete;

			std::unique_lock<std::mutex> lock()
			{
				using namespace std;

				unique_lock<mutex> res(this->m_mutex);
				return move(res);
			}

			std::mutex           m_mutex;
			std::shared_ptr<_Ty> m_data_ptr;
			node*				 m_next;
	};

	public:
		concurrent_list()
		{
		}

		concurrent_list(std::initializer_list<_Ty>& init_list)
			: concurrent_list()
		{
			using namespace std;

			auto it = init_list.begin();

			while (it != init_list.end())
			{
				this->push_front(*it);
				++it;
			}
		}

		~concurrent_list()
		{
			node* item = this->m_head.m_next;

			while (item)
			{
				node* next = item->m_next;
				delete item;

				item = next;
			}
		}

		concurrent_list(const concurrent_list&) = delete;
		concurrent_list& operator=(const concurrent_list&) = delete;

		void push_front(const _Ty& data)
		{
			using namespace std;

			node* item = new node(data);
			unique_lock<mutex> lock(this->m_head.lock());
			item->m_next = this->m_head.m_next;
			this->m_head.m_next = item;
		}

		std::shared_ptr<_Ty> pop_front()
		{
			using namespace std;
			
			unique_ptr<node> node_ptr(this->pop_front_node());

			if (node_ptr)
			{
				return move(node_ptr->m_data_ptr);
			}

			return shared_ptr<_Ty>();
		}

		bool pop_front(_Ty& res)
		{
			using namespace std;
			
			unique_ptr<node> node_ptr(this->pop_front_node());

			if (node_ptr)
			{
				res = move(*node_ptr->m_data_ptr);
				return true;
			}

			return false;
		}

		template<typename _Func> void for_each(_Func func)
		{
			using namespace std;

			node* item = &this->m_head;
			unique_lock<mutex> lock(item->lock());

			while (node* const next_item = item->m_next)
			{
				unique_lock<mutex> next_lock(next_item->lock());
				lock.unlock();
				
				func(*next_item->m_data_ptr);
				
				item = next_item;
				lock = move(next_lock);
			}
		}

		template<typename _Pred> std::shared_ptr<_Ty> first_or_default(_Pred pred)
		{
			using namespace std;

			node* item = &this->m_head;
			unique_lock<mutex> lock(item->lock());

			while (node* const next_item = item->m_next)
			{
				unique_lock<mutex> next_lock(next_item->lock());
				lock.unlock();

				if (pred(*next_item->m_data_ptr))
				{
					return next_item->m_data_ptr;
				}

				item = next_item;
				lock = move(next_lock);
			}

			return shared_ptr<_Ty>();
		}

		template<typename _Pred> int remove_if(_Pred pred)
		{
			using namespace std;

			int j = 0;

			node* item = &this->m_head;
			unique_lock<mutex> lock(item->lock());

			while (node* const next_item = item->m_next)
			{
				unique_lock<mutex> next_lock(next_item->lock());

				if (pred(*next_item->m_data_ptr))
				{
					unique_ptr<node> next_ptr(next_item);
					item->m_next = next_item->m_next;

					next_lock.unlock();
					next_lock.release();

					++j;
				}
				else
				{
					item = next_item;
					lock = move(next_lock);
				}
			}

			return j;
		}

		int clear()
		{
			int res = this->remove_if([](int& x)->bool { return 1 == 1; });
			return res;
		}

	private:
		std::unique_ptr<node> pop_front_node()
		{
			using namespace std;

			unique_lock<mutex> lock_head(this->m_head.lock());

			if (this->m_head.m_next)
			{
				unique_lock<mutex> lock_next(this->m_head.m_next->lock());

				unique_ptr<node> item(this->m_head.m_next);
				this->m_head.m_next = item->m_next;

				return move(item);
			}

			return unique_ptr<node>();
		}

		node m_head;
};
