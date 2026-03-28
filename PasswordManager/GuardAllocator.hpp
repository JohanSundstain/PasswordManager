#pragma once 
#include <windows.h>
#include <stdexcept>

template <class T>
class GuardAllocator
{		
public:

	using value_type = T;

	GuardAllocator() noexcept {}

	template <class U>
	GuardAllocator(const GuardAllocator<U>&) noexcept {}

	T* allocate(size_t n)
	{
		SYSTEM_INFO sys_info;
		GetSystemInfo(&sys_info);

		size_t page_size = sys_info.dwPageSize;
		size_t data_size = ((n * sizeof(T) + page_size - 1) / page_size) * page_size;
		size_t size = (page_size * 2) + data_size;

		void* base_ptr = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);

		if (base_ptr == NULL) throw std::bad_alloc();

		void* data_ptr = reinterpret_cast<char*>(base_ptr) + page_size;
		if (VirtualAlloc(data_ptr, data_size,  MEM_COMMIT, PAGE_READWRITE) == NULL)
		{
			VirtualFree(base_ptr, 0, MEM_RELEASE);
			throw std::bad_alloc();
		}

		VirtualLock(data_ptr, data_size);

		return reinterpret_cast<T*>(data_ptr);
	}

	void lock(T* ptr, size_t n)
	{
		if (ptr == NULL) return;

		SYSTEM_INFO sys_info;
		GetSystemInfo(&sys_info);

		size_t page_size = sys_info.dwPageSize;
		size_t data_size = ((n * sizeof(T) + page_size - 1) / page_size) * page_size;

		DWORD old_protect;

		VirtualProtect(ptr, data_size, PAGE_NOACCESS, &old_protect);
	}

	void unlock(T* ptr, size_t n)
	{
		if (ptr == NULL) return;

		SYSTEM_INFO sys_info;
		GetSystemInfo(&sys_info);

		size_t page_size = sys_info.dwPageSize;
		size_t data_size = ((n * sizeof(T) + page_size - 1) / page_size) * page_size;

		DWORD old_protect;

		VirtualProtect(ptr, data_size, PAGE_READWRITE, &old_protect);
	}

	void deallocate(T* ptr, size_t n)
	{
		if (ptr == NULL) return;

		SYSTEM_INFO sys_info;
		GetSystemInfo(&sys_info);

		size_t page_size = sys_info.dwPageSize;
		size_t data_size = ((n * sizeof(T) + page_size - 1) / page_size) * page_size;
	
		void* base_ptr = reinterpret_cast<char*>(ptr) - page_size;

		VirtualUnlock(ptr, data_size);

		VirtualFree(base_ptr, 0,  MEM_RELEASE);
	}

	bool operator==(const GuardAllocator&) const { return true; }

	bool operator!=(const GuardAllocator&) const { return false; }
};