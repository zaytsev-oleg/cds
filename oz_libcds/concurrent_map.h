#pragma once

#include <utility>
#include <memory>
#include <algorithm>
#include <list>
#include <vector>
#include <map>
#include <shared_mutex>
#include <atomic>

template<typename _Key, typename _Val, typename _Hash = std::hash<_Key>, int num_of_buckets = 17> class concurrent_map
{
	class bucket
	{
		public:
			using data_item = std::pair<_Key, _Val>;
			using container = std::list<data_item>;
		
			decltype(auto) find_entry(const _Key& key)
			{
				using namespace std;

				return find_if(this->m_data.begin(), this->m_data.end(),
					[&](const data_item& item)->bool { return item.first == key; });
			}

			decltype(auto) find_entry(const _Key& key) const
			{
				using namespace std;

				return find_if(this->m_data.cbegin(), this->m_data.cend(),
					[&](const data_item& item)->bool { return item.first == key; });
			}

			_Val get_value(const _Key& key, const _Val& default_value) const
			{
				using namespace std;

				shared_lock<shared_mutex> lock(this->m_mutex);
				auto entry = this->find_entry(key);

				return entry == this->m_data.end()
					? default_value : entry->second;
			}

			bool get_value(const _Key& key, _Val& value) const
			{
				using namespace std;

				shared_lock<shared_mutex> lock(this->m_mutex);
				auto entry = this->find_entry(key);

				if (entry == this->m_data.end())
				{
					return false;
				}

				value = entry->second;
				return true;
			}

			bool add_or_update(const _Key& key, const _Val& value)
			{
				using namespace std;

				unique_lock<shared_mutex> lock(this->m_mutex);
				auto entry = this->find_entry(key);

				if (entry == this->m_data.end())
				{
					this->m_data.push_back(data_item(key, value));
					return true;
				}
				else
				{
					entry->second = value;
					return false;
				}
			}

			bool remove(const _Key& key)
			{
				using namespace std;

				unique_lock<shared_mutex> lock(this->m_mutex);
				auto entry = this->find_entry(key);

				if (entry == this->m_data.end())
					return false;

				this->m_data.erase(entry);
				return true;
			}

			int clear()
			{
				using namespace std;

				unique_lock<shared_mutex> lock(this->m_mutex);

				int res = this->m_data.size();
				this->m_data.clear();

				return res;
			}

			container m_data;
			mutable std::shared_mutex m_mutex;
	};

	public:
		concurrent_map(const _Hash& hasher = _Hash()) 
			: m_hasher{ hasher }
		{
			using namespace std;

			for (int i = 0; i < num_of_buckets; i++)
			{
				this->m_buckets[i] = move(make_unique<bucket>());
			}
		}

		~concurrent_map()
		{
			using namespace std;

			for (int i = 0; i < num_of_buckets; i++)
			{
				unique_ptr<bucket> ptr = move(this->m_buckets[i]);
			}
		}

		concurrent_map(const concurrent_map&) = delete;
		concurrent_map& operator=(const concurrent_map&) = delete;

		_Val get_value(const _Key& key, const _Val& default_value = _Val()) const
		{
			return this->get_bucket(key).get_value(key, default_value);
		}

		bool get_value(const _Key& key, _Val& value) const
		{
			return this->get_bucket(key).get_value(key, value);
		}

		bool add_or_update(const _Key& key, const _Val& value)
		{
			return this->get_bucket(key).add_or_update(key, value);
		}

		bool remove(const _Key& key)
		{
			return this->get_bucket(key).remove(key);
		}

		std::map<_Key, _Val> get_map() const
		{
			using namespace std;

			map<_Key, _Val> res;
			vector<shared_lock<shared_mutex>> locks(num_of_buckets);

			for (int i = 0; i < num_of_buckets; i++)
			{
				locks.push_back(shared_lock<shared_mutex>(this->m_buckets[i]->m_mutex));
			}

			for (int i = 0; i < num_of_buckets; i++)
			{
				auto it     = this->m_buckets[i]->m_data.begin();
				auto it_end = this->m_buckets[i]->m_data.end();

				while (it != it_end)
				{
					res.insert(*it);
					++it;
				}
			}

			return move(res);
		}

		int clear()
		{
			int res = 0;

			for (int i = 0; i < num_of_buckets; i++)
			{
				res += this->m_buckets[i]->clear();
			}

			return res;
		}

	private:
		bucket& get_bucket(const _Key& key) const
		{
			const int j = this->m_hasher(key) % num_of_buckets;
			return *this->m_buckets[j];
		}
		
		std::unique_ptr<bucket> m_buckets[num_of_buckets];
		_Hash m_hasher;
};
