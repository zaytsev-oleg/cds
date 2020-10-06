#pragma once

#include <atomic>
#include <memory>
#include <initializer_list>

template<typename _Ty> class exp_stack_atomic_shared_ptr
{
	class node
	{
		public:
			node(const _Ty& data)
				: m_data{ std::make_shared<_Ty>(data) }
			{
			}

		std::shared_ptr<_Ty>  m_data;
		std::shared_ptr<node> m_next;
	};

	public:
		exp_stack_atomic_shared_ptr()
			: m_is_active{ true }
		{
		}

		exp_stack_atomic_shared_ptr(std::initializer_list<_Ty>& init_list)
			: exp_stack_atomic_shared_ptr()
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

		exp_stack_atomic_shared_ptr(const exp_stack_atomic_shared_ptr&) = delete;
		exp_stack_atomic_shared_ptr& operator=(const exp_stack_atomic_shared_ptr&) = delete;
		
		~exp_stack_atomic_shared_ptr()
		{
			this->m_is_active = false;
			this->clear();
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
			{
				return this->pop_item();
			}

			return shared_ptr<_Ty>();
		}

		int clear()
		{
			int j = 0;

			while (this->pop_item())
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

			shared_ptr<node> const item = make_shared<node>(data);
			item->m_next = atomic_load(&this->m_head);
			while (!atomic_compare_exchange_weak(&this->m_head, &item->m_next, item));
		}

		std::shared_ptr<_Ty> pop_item()
		{
			using namespace std;

			shared_ptr<node> item = atomic_load(&this->m_head);
			while (item && !atomic_compare_exchange_weak(&this->m_head, &item, atomic_load(&item->m_next)));

			if (item)
			{
				atomic_store(&item->m_next, shared_ptr<node>());
				return item->m_data;
			}

			return shared_ptr<_Ty>();
		}

		std::shared_ptr<node> m_head;
		std::atomic<bool>     m_is_active;
};
