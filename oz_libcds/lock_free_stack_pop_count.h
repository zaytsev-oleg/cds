#pragma once

#include <atomic>
#include <memory>
#include <initializer_list>

template<typename _Ty> class lock_free_stack_pop_count
{
	class node
	{
		public:
			node(const _Ty& data)
				: m_data_ptr{ std::make_shared<_Ty>(std::move(data)) }, m_next{ nullptr }
			{
			}

			std::shared_ptr<_Ty> m_data_ptr;
			node* m_next;
	};

	public:
		lock_free_stack_pop_count()
			: m_head{ nullptr }, m_nodes_to_delete{ nullptr }, m_is_active{ true }, m_pop_counter{ 0 }
		{
		}
	
		lock_free_stack_pop_count(std::initializer_list<_Ty>& init_list)
			: lock_free_stack_pop_count()
		{
			using namespace std;
	
			auto it = init_list.begin();
	
			while (it != init_list.end())
			{
				this->push(*it);
				++it;
			}
		}
	
		~lock_free_stack_pop_count()
		{
			using namespace std;

			this->m_is_active.store(false);
			while (this->m_pop_counter.load());

			node* nodes = this->m_head.exchange(nullptr);
	
			if (nodes)
			{
				this->add_pending_nodes(nodes);
				nodes = nullptr;
			}
	
			lock_free_stack_pop_count::delete_nodes(this->m_nodes_to_delete);
		}
	
		lock_free_stack_pop_count(const lock_free_stack_pop_count&) = delete;
		lock_free_stack_pop_count& operator=(const lock_free_stack_pop_count&) = delete;
		
		void push(const _Ty& data, bool& res)
		{
			using namespace std;
	
			if (!this->m_is_active)
			{
				res = false;
				return;
			}

			node* const item = new node(move(data));
			item->m_next = this->m_head.load();
			while (!m_head.compare_exchange_weak(item->m_next, item));

			res = true;
		}
	
		std::shared_ptr<_Ty> pop()
		{
			using namespace std;
			
			if (!this->m_is_active)
				return shared_ptr<_Ty>();

			this->m_pop_counter.fetch_add(1);
	
			node* item = this->m_head.load();
			while (item && !this->m_head.compare_exchange_weak(item, item->m_next));
	
			shared_ptr<_Ty> res;
	
			if (item)
			{
				res.swap(item->m_data_ptr);
			}
	
			this->clean_up(item);

			return res;
		}
	
		int clear()
		{
			using namespace std;

			int j = 0;

			if (!this->m_is_active)
				return j;

			node* head = this->m_head.exchange(nullptr);

			if (head)
			{
				++j;
				node* tail = head;
				tail->m_data_ptr.reset();

				while (node* const next = tail->m_next)
				{
					++j;
					tail = next;
					tail->m_data_ptr.reset();
				}

				this->add_pending_nodes(head, tail);
			}

			return j;
		}
	
		bool is_active() const
		{
			return this->m_is_active;
		}
	
	private:
		static int delete_nodes(node* nodes)
		{
			int res = 0;

			while (nodes)
			{
				node* item = nodes;
				nodes = nodes->m_next;
	
				delete item;
				++res;
			}

			return res;
		}
	
		void clean_up(node* item)
		{
			if (this->m_pop_counter == 1)
			{
				node* nodes_to_delete = this->m_nodes_to_delete.exchange(nullptr);

				if (this->m_pop_counter == 1)
				{
					lock_free_stack_pop_count::delete_nodes(nodes_to_delete);
				}
				else if (nodes_to_delete)
				{
					this->add_pending_nodes(nodes_to_delete);
				}

				this->m_pop_counter.fetch_sub(1);
				delete item;

				return;
			}
	
			if (item)
			{
				this->add_pending_node(item);
			}
	
			this->m_pop_counter.fetch_sub(1);
		}
	
		void add_pending_nodes(node* head)
		{
			node* tail = head;
	
			while (node* const next = tail->m_next)
			{
				tail = next;
			}
	
			this->add_pending_nodes(head, tail);
		}
	
		void add_pending_nodes(node* head, node* tail)
		{
			tail->m_next = this->m_nodes_to_delete;
			while (!this->m_nodes_to_delete.compare_exchange_weak(tail->m_next, head));
		}
	
		void add_pending_node(node* item)
		{
			this->add_pending_nodes(item, item);
		}

		std::atomic<node*> m_head;
		std::atomic<node*> m_nodes_to_delete;
		std::atomic<int>   m_pop_counter;
		std::atomic<bool>  m_is_active;
};
