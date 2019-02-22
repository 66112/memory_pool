#pragma once
#include "Common.h"
#include "CentralCache.h"

/*
	TLS thread local storage
	线程本地存储  
	保证每个线程都有自己的全局变量，线程间不共享
*/
class ThreadCache
{
public:
	void* Allocate(size_t size);
	void Deallocate(void* ptr,size_t size);
	//从中心缓存获取对象
	void* FetchFromCentralCache(size_t size, size_t index);
	//链表中对象太多，回收
	void ListTooLong(FreeList& freelist, size_t size);
private:
	FreeList _freelist[NLISTS];
};

//表示tls_threadcache每个线程都有一份
static _declspec(thread) ThreadCache* tls_threadcache = nullptr;

