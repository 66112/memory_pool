#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"

void* ConcurrentAlloc(size_t size)
{
	//������64K
	if (size > MAXBYTES){
		//��ҳΪ��λ����
		size_t roundsize = SizeClass::_Roundup(size,1 << PAGE_SHIFT);
		size_t npage = roundsize >> PAGE_SHIFT;
		Span* span = PageCache::GetInstance()->NewSpan(npage);
		void* ptr = span->_objlist;
		return ptr;
	}
	else{
		//ͨ��tls��ȡ�߳��Լ���threadcache
		if (tls_threadcache == nullptr){
			tls_threadcache = new ThreadCache;
		}
		return tls_threadcache->Allocate(size);
	}
}

void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToPageSpan(ptr);
	size_t size = span->_objsize;
	if (size > MAXBYTES){
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
	}
	else{
		return tls_threadcache->Deallocate(ptr, size);
	}
}