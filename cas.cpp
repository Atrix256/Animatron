#include "cas.h"
#include <direct.h>

void CAS::FlushToDisk()
{
	_mkdir("build/CAS");
	char fileName[1024];

	for (auto& pair : m_storage)
	{
		sprintf_s(fileName, "build/CAS/%zu.dat", pair.first);
		FILE* file = nullptr;
		fopen_s(&file, fileName, "wb");
		if (file)
		{
			fwrite(pair.second.data, pair.second.size, 1, file);
			fclose(file);
		}
	}
}

CAS::CAS()
{
	omp_init_lock(&m_lock);
}

CAS::~CAS()
{
	FlushToDisk();

	for (auto& pair : m_storage)
		delete[] pair.second.data;

	m_storage.clear();

	for (void* ptr : m_orphanedMemory)
		delete[] ptr;
	m_orphanedMemory.clear();

	omp_destroy_lock(&m_lock);
}

void* CAS::Get(size_t key)
{
	omp_set_lock(&m_lock);

	// if it's already in memory, return it
	auto it = m_storage.find(key);
	if (it != m_storage.end())
	{
		void* ret = it->second.data;
		omp_unset_lock(&m_lock);
		return ret;
	}

	// else try and load it from disk
	char fileName[1024];
	sprintf_s(fileName, "build/CAS/%zu.dat", key);
	FILE* file = nullptr;
	fopen_s(&file, fileName, "rb");
	if (!file)
	{
		omp_unset_lock(&m_lock);
		return nullptr;
	}

	// if we could open the file, read in the data and return it
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);
	unsigned char* newData = new unsigned char[size];
	fread(newData, size, 1, file);
	m_storage[key] = { newData, size };
	fclose(file);

	omp_unset_lock(&m_lock);
	return newData;
}

void CAS::Set(size_t key, const void* data, size_t size)
{
	omp_set_lock(&m_lock);

	// if the same data is already there, ignore this
	CAS::Storage existing;
	auto it = m_storage.find(key);
	if (it != m_storage.end())
		existing = it->second;

	if (existing.data && existing.size == size && memcmp(existing.data, data, size) == 0)
	{
		omp_unset_lock(&m_lock);
		return;
	}

	printf("Warning: CAS::Set() got an existing key but with new data. CAS may be stale and need to be deleted?\n");

	// nobody should be using this memory, but in case they are, stash it off to be destroyed on shutdown, instead of right now
	if (existing.data)
		m_orphanedMemory.push_back(existing.data);

	unsigned char* newData = new unsigned char[size];
	memcpy(newData, data, size);
	m_storage[key] = { newData, size };

	omp_unset_lock(&m_lock);
}
