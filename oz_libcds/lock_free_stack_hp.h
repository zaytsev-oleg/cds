#pragma once

#include <memory>
#include <utility>
#include <atomic>
#include <thread>
#include <initializer_list>
#include <functional>

template<typename _Ty, int max_hazards = 100> class lock_free_stack_hp
{
	class node
	{
		public:
			node(const _Ty& data) 
				: m_data_ptr { std::make_shared<_Ty>(move(data)) }, m_next{ nullptr }
			{
			}

			std::shared_ptr<_Ty> m_data_ptr;
			node* m_next;
	};

	class hp
	{
		public:
			std::atomic<std::thread::id> m_id;
			std::atomic<void*> m_ptr;
	};

	class hp_owner
	{
		public:
			hp_owner(hp(&hazard_pointers)[max_hazards])
				: m_hp{ nullptr }
			{
				using namespace std;

				for (int i = 0; i < max_hazards; i++)
				{
					thread::id id;

					if (hazard_pointers[i].m_id.compare_exchange_strong(id, this_thread::get_id()))
					{
						this->m_hp = &hazard_pointers[i];
						break;
					}
				}

				if (!this->m_hp)
				{
					throw exception("No hazard pointers available");
				}
			}

			~hp_owner()
			{
				using namespace std;

				if (this->m_hp)
				{
					this->m_hp->m_id.store(thread::id());
					this->m_hp->m_ptr.store(nullptr);

					this->m_hp = nullptr;
				}
			}

			hp_owner(const hp_owner&) = delete;
			hp_owner& operator=(const hp_owner&) = delete;

			std::atomic<void*>& get_ptr()
			{
				return this->m_hp->m_ptr;
			}

		private:
			hp* m_hp;
	};

	class data_to_delete
	{
		public:
			template<typename _Tx> data_to_delete(_Tx* data)
				: m_data{ data }, m_next{ nullptr }
			{
				this->m_deleter = [](void* data)->void { delete static_cast<_Tx*>(data); };
			}

			~data_to_delete()
			{
				this->m_deleter(this->m_data);
			}
			
			void* m_data;
			std::function<void(void*)> m_deleter;
			data_to_delete* m_next;
	};

	std::atomic<void*>& get_hazard_pointer()
	{
		thread_local static hp_owner owner(this->m_hazards);
		return owner.get_ptr();
	}

	bool has_hazard_pointers(void* item)
	{
		for (int i = 0; i < max_hazards; i++)
		{
			if (this->m_hazards[i].m_ptr.load() == item)
			{
				return true;
			}
		}

		return false;
	}

	template<typename _Tx> void delete_later(_Tx* item)
	{
		this->set_for_deletion(new data_to_delete(item));
	}

	void set_for_deletion(data_to_delete* to_be_deleted)
	{
		to_be_deleted->m_next = this->m_to_be_deleted.load();
		while (!this->m_to_be_deleted.compare_exchange_weak(to_be_deleted->m_next, to_be_deleted));
	}

	void clean_up_memory(bool force_delete = false)
	{
		using namespace std;
		
		data_to_delete* first = this->m_to_be_deleted.exchange(nullptr);

		if (!first)
			return;

		if (!force_delete)
		{
			int i = 0;
			data_to_delete* last = first;

			if (last)
			{
				++i;

				while (last->m_next)
				{
					++i;
					last = last->m_next;
				}
			}

			if (i < 2 * max_hazards)
			{
				last->m_next = this->m_to_be_deleted.load();
				while (!this->m_to_be_deleted.compare_exchange_weak(last->m_next, first));

				return;
			}
		}

		while (first)
		{
			data_to_delete* const next = first->m_next;

			if (!force_delete && this->has_hazard_pointers(first->m_data))
			{
				this->set_for_deletion(first);
			}
			else
			{
				delete first;
			}

			first = next;
		}
	}

	public:
		lock_free_stack_hp() 
			: m_head{ nullptr }, m_to_be_deleted{ nullptr }, m_is_active{ true }
		{
		}

		lock_free_stack_hp(std::initializer_list<_Ty>& init_list)
			: lock_free_stack_hp()
		{
			using namespace std;

			auto it = init_list.begin();

			while (it != init_list.end())
			{
				this->push_item(*it);
				++it;
			}
		}

		~lock_free_stack_hp()
		{
			this->m_is_active = false;
			while (this->pop_item());
			this->clean_up_memory(true);
		}

		lock_free_stack_hp(const lock_free_stack_hp&) = delete;
		lock_free_stack_hp& operator=(const lock_free_stack_hp&) = delete;

		void push(const _Ty& data, bool& res)
		{
			using namespace std;

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
			using namespace std;

			int i = 0;
			node* item = this->m_head.exchange(nullptr);

			while (item)
			{
				node* const next = item->m_next;
				
				shared_ptr<_Ty> data_ptr;
				data_ptr.swap(item->m_data_ptr);

				this->delete_later(item);
				
				item = next;
				++i;
			}

			this->clean_up_memory();
			return i;
		}

		bool is_active() const
		{
			return this->m_is_active;
		}

	private:
		void push_item(const _Ty& data)
		{
			node* const item = new node(data);
			item->m_next = this->m_head.load();
			while (!m_head.compare_exchange_weak(item->m_next, item));
		}

		std::shared_ptr<_Ty> pop_item()
		{
			using namespace std;

			atomic<void*>& hp = this->get_hazard_pointer();
			node* item = this->m_head.load();

			do
			{
				node* temp;

				do
				{
					temp = item;
					hp.store(item);
					item = this->m_head.load();
				} while (item != temp);
			} while (item && !this->m_head.compare_exchange_strong(item, item->m_next));

			hp.store(nullptr);
			shared_ptr<_Ty> res;

			if (item)
			{
				res.swap(item->m_data_ptr);

				if (this->has_hazard_pointers(item))
				{
					this->delete_later(item);
				}
				else
				{
					delete item;
				}

				this->clean_up_memory();
			}

			return res;
		}

		std::atomic<node*> m_head;
		std::atomic<data_to_delete*> m_to_be_deleted;
		hp m_hazards[max_hazards];
		std::atomic<bool> m_is_active;
};
