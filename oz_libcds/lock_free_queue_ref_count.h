#pragma once

#include <atomic>
#include <memory>
#include <initializer_list>

template<typename _Ty> class lock_free_queue_ref_count
{
	class node;

	struct counted_node_ptr
	{
		counted_node_ptr()
			: m_node{ nullptr }, m_external_counter{ 0 }
		{
		}

		node* m_node;
		int   m_external_counter;
	};

	struct node_counter
	{
		node_counter()
			: m_internal_counter{ 0 }, m_counters{ 0 }
		{
		}

		int m_internal_counter;
		int m_counters;
	};

	class node
	{
		public:
			node() : m_data{ nullptr }, m_is_free{ true }
			{
				node_counter counter;
				counter.m_internal_counter = 0;
				counter.m_counters = 2;

				this->m_counter.store(counter);
			}

			bool release_ref()
			{
				using namespace std;

				node_counter old_counter = this->m_counter.load();
				node_counter new_counter;

				do
				{
					new_counter = old_counter;
					--new_counter.m_internal_counter;
				} while (!this->m_counter.compare_exchange_strong(old_counter, new_counter));

				if (!new_counter.m_internal_counter && !new_counter.m_counters)
				{
					delete this;
					return true;
				}

				return false;
			}

			std::shared_ptr<_Ty>      m_data;
			counted_node_ptr          m_next;
			std::atomic<node_counter> m_counter;
			std::atomic<bool>         m_is_free;
	};

	public:
		lock_free_queue_ref_count() 
			: m_is_active{ true }
		{
			node* item = new node;

			counted_node_ptr node_ptr;
			node_ptr.m_node = item;
			node_ptr.m_external_counter = 1;

			this->m_head.store(node_ptr);
			this->m_tail.store(this->m_head.load());
		}

		lock_free_queue_ref_count(std::initializer_list<_Ty>& init_list)
			: lock_free_queue_ref_count()
		{
			using namespace std;

			auto it = init_list.begin();

			while (it != init_list.end())
			{
				bool res = false;
				this->push(*it, res);

				++it;
			}
		}

		lock_free_queue_ref_count(const lock_free_queue_ref_count&) = delete;
		lock_free_queue_ref_count& operator=(const lock_free_queue_ref_count&) = delete;

		~lock_free_queue_ref_count()
		{
			this->m_is_active = false;
			while (this->pop_item());

			node* p_node = this->m_head.load().m_node;
			delete p_node;
		}

		void push(const _Ty& data, bool& res)
		{
			if (this->m_is_active)
			{
				this->push_item(data);
				res = true;

				return;
			}

			res = false;
		}

		std::shared_ptr<_Ty> pop()
		{
			using namespace std;

			if (this->m_is_active)
				return this->pop_item();

			return shared_ptr<_Ty>();
		}

		int clear()
		{
			int j = 0;

			while (this->pop())
			{
				++j;
			}

			return j;
		}

		bool is_active() const
		{
			return this->m_is_active;
		}

	private:
		void push_item(const _Ty& data)
		{
			using namespace std;

			shared_ptr<_Ty> data_ptr(make_shared<_Ty>(data));

			counted_node_ptr new_next;
			new_next.m_node = new node;
			new_next.m_external_counter = 1;

			counted_node_ptr old_tail = this->m_tail.load();

			for (;;)
			{
				this->increase_external_counter(this->m_tail, old_tail);
				node* const p_node = old_tail.m_node;

				bool is_free = true;

				if (p_node->m_is_free.compare_exchange_strong(is_free, false))
				{
					p_node->m_data = move(data_ptr);
					p_node->m_next = new_next;
					old_tail = this->m_tail.exchange(new_next);
					this->free_external_counter(old_tail);

					return;
				}

				p_node->release_ref();
			}
		}

		std::shared_ptr<_Ty> pop_item()
		{
			using namespace std;

			counted_node_ptr old_head = this->m_head.load();

			for (;;)
			{
				this->increase_external_counter(this->m_head, old_head);
				node* const p_node = old_head.m_node;

				if (p_node == this->m_tail.load().m_node)
				{
					p_node->release_ref();
					return shared_ptr<_Ty>();
				}

				if (this->m_head.compare_exchange_strong(old_head, p_node->m_next))
				{
					shared_ptr<_Ty> res;
					res.swap(p_node->m_data);
					this->free_external_counter(old_head);

					return res;
				}

				p_node->release_ref();
			}
		}

		void increase_external_counter(std::atomic<counted_node_ptr>& counter, counted_node_ptr& old_counter)
		{
			using namespace std;

			counted_node_ptr new_counter;
			
			do
			{
				new_counter = old_counter;
				++new_counter.m_external_counter;
			} while (!counter.compare_exchange_strong(old_counter, new_counter));

			old_counter.m_external_counter = new_counter.m_external_counter;
		}

		void free_external_counter(counted_node_ptr& node_ptr)
		{
			using namespace std;

			node* const p_node = node_ptr.m_node;
			int const inc = node_ptr.m_external_counter - 2;

			node_counter old_counter = p_node->m_counter.load();
			node_counter new_counter;

			do
			{
				new_counter = old_counter;
				--new_counter.m_counters;
				new_counter.m_internal_counter += inc;
			} while (!p_node->m_counter.compare_exchange_strong(old_counter, new_counter));

			if (!new_counter.m_internal_counter && !new_counter.m_counters)
			{
				delete p_node;
			}
		}

		std::atomic<counted_node_ptr> m_head;
		std::atomic<counted_node_ptr> m_tail;
		std::atomic<bool>			  m_is_active;
};
