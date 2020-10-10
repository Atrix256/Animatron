// Content addressable storage to cache off data that is expensive to create

#pragma once

#include <unordered_map>
#include <omp.h>
#include <vector>

class CAS
{
public:

	CAS();
	~CAS();

	bool Init()
	{
		return Get(0) == nullptr;
	}

	void* Get(size_t key);
	void Set(size_t key, const void* data, size_t size);

	template <typename T>
	static void Set(size_t key, const T& data)
	{
		Get().Set(key, &data, sizeof(data));
	}

	template <typename T>
	static void Set(size_t key, const std::vector<T>& data)
	{
		Get().Set(key, data.data(), data.size() * sizeof(data[0]));
	}

	// Happens when destructor is called, but you can call it manually if you want to
	void FlushToDisk();

	static CAS& Get()
	{
		static CAS cas;
		return cas;
	}

private:

	struct Storage
	{
		void* data = nullptr;
		size_t size = 0;
	};

	omp_lock_t m_lock;

	std::unordered_map<size_t, Storage> m_storage;
	std::vector<void*> m_orphanedMemory;
};