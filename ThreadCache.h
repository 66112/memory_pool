#pragma once
#include "Common.h"
#include "CentralCache.h"

/*
	TLS thread local storage
	�̱߳��ش洢  
	��֤ÿ���̶߳����Լ���ȫ�ֱ������̼߳䲻����
*/
class ThreadCache
{
public:
	void* Allocate(size_t size);
	void Deallocate(void* ptr,size_t size);
	//�����Ļ����ȡ����
	void* FetchFromCentralCache(size_t size, size_t index);
	//�����ж���̫�࣬����
	void ListTooLong(FreeList& freelist, size_t size);
private:
	FreeList _freelist[NLISTS];
};

//��ʾtls_threadcacheÿ���̶߳���һ��
static _declspec(thread) ThreadCache* tls_threadcache = nullptr;

