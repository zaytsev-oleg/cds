#pragma once

#include <atomic>
#include <memory>
#include <initializer_list>

template<typename _Ty> class lock_free_stack_ref_count
{
	private:
		struct node;

		struct counted_node_ptr
		{
			counted_node_ptr()
				: m_external_counter{ 0 }, m_node{ nullptr }
			{
			}

			int   m_external_counter;
			node* m_node;
		};

		class node
		{
			public:
				node(const _Ty& data)
					: m_data{ std::make_shared<_Ty>(data) }, m_internal_counter{ 0 }
				{
				}

				std::shared_ptr<_Ty> m_data;
				std::atomic<int>     m_internal_counter;
				counted_node_ptr     m_next;
		};

	public:
		lock_free_stack_ref_count()
			: m_is_active { true }
		{
		}

		lock_free_stack_ref_count(std::initializer_list<_Ty>& init_list)
			: lock_free_stack_ref_count()
		{
			using namespace std;

			auto it = init_list.begin();

			while (it != init_list.end())
			{
				this->push_item(*it);
				++it;
			}
		}
		
		~lock_free_stack_ref_count()
		{
			this->m_is_active.store(false);
			while (this->pop_item());
		}

		lock_free_stack_ref_count(const lock_free_stack_ref_count&) = delete;
		lock_free_stack_ref_count& operator=(const lock_free_stack_ref_count&) = delete;

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

		bool is_active()
		{
			return this->m_is_active;
		}

	private:
		void push_item(const _Ty& data)
		{
			using namespace std;

			counted_node_ptr item;
			item.m_node = new node(data);
			item.m_external_counter = 1;
			item.m_node->m_next = this->m_head.load(memory_order_relaxed);
			while (!this->m_head.compare_exchange_weak(
				item.m_node->m_next, item, memory_order_release, memory_order_relaxed));
		}

		std::shared_ptr<_Ty> pop_item()
		{
			using namespace std;

			counted_node_ptr old_head = this->m_head.load(memory_order_relaxed);

			for(;;)
			{
				this->increase_head_count(old_head);
				node* const p_node = old_head.m_node;

				if (!p_node)
					return shared_ptr<_Ty>();

				if (this->m_head.compare_exchange_strong(
					old_head, p_node->m_next, memory_order_relaxed))
				{
					shared_ptr<_Ty> res;
					res.swap(p_node->m_data);

					const int inc = old_head.m_external_counter - 2;

					if (p_node->m_internal_counter.fetch_add(inc, memory_order_release) == -inc)
					{
						delete p_node;
					}

					return res;
				}
				else if (p_node->m_internal_counter.fetch_add(-1, memory_order_relaxed) == 1)
				{
					p_node->m_internal_counter.load(memory_order_acquire);
					delete p_node;
				}
			}
		}

		void increase_head_count(counted_node_ptr& old_counter)
		{
			using namespace std;

			counted_node_ptr new_counter;

			do
			{
				new_counter = old_counter;
				++new_counter.m_external_counter;
			} while (!this->m_head.compare_exchange_strong(
				old_counter, new_counter, memory_order_acquire, memory_order_relaxed));

			old_counter.m_external_counter = new_counter.m_external_counter;
		}

		std::atomic<counted_node_ptr> m_head;
		std::atomic<bool>			  m_is_active;
};
