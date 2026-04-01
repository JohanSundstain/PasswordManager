#pragma once 
#include <windows.h>
#include <stdexcept>
#include <tuple>

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

		DWORD old_protect;
		VirtualProtect(ptr, data_size, PAGE_READWRITE, &old_protect);

		SecureZeroMemory(ptr, data_size);
	
		void* base_ptr = reinterpret_cast<char*>(ptr) - page_size;

		VirtualUnlock(ptr, data_size);

		VirtualFree(base_ptr, 0,  MEM_RELEASE);
	}

	bool operator==(const GuardAllocator&) const { return true; }

	bool operator!=(const GuardAllocator&) const { return false; }
};


template <typename... TContainers>
class Bottleneck
{
private:
	std::tuple<TContainers&...> _args;
public:
	Bottleneck(TContainers&... args)
		: _args(args...)
	{
		std::apply([](auto&... containers)
			{
				(containers.get_allocator().unlock(containers.data(), containers.capacity()) , ...);
			},
			_args);
	}

	~Bottleneck()
	{
		std::apply(
			[](auto&... containers)
			{
				(containers.get_allocator().lock(containers.data(), containers.capacity()) , ...);
			},
			_args);
	}

	Bottleneck(const Bottleneck&) = delete;
	Bottleneck& operator=(const Bottleneck&) = delete;
};

template <typename TContainer>
using SafeVector = std::vector<TContainer, GuardAllocator<TContainer>>;

template <typename... TContainers>
inline void EmergencyRemoval(TContainers&... args)
{
	Bottleneck lock(args...);
	(SecureZeroMemory(args.data(), args.capacity() * sizeof(typename TContainers::value_type)), ...);
};


template <typename TContainer>
inline SafeVector<TContainer> CreateSafeVector(size_t n)
{
	SafeVector<TContainer> sv;
	sv.reserve(n);
	sv.get_allocator().lock(sv.data(), sv.capacity());
	return sv;
}

