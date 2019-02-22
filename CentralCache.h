#pragma once
#include "Common.h"

class CentralCache
{
public:
	static CentralCache* GetInstance(){
		return &_inst;
	}

	Span* GetOneSpan(SpanList* spanlist, size_t size);

	//从中心缓存获取一定数量的对象给threadcache
	size_t FetchRangeObj(void*& start, void*& end, size_t num, size_t size);
	//将一定数量的对象释放到span
	void ReleaseListToSpans(void* start, size_t size);
private:
	//中心缓存自由链表
	SpanList _spanList[NLISTS];		//管理内存块，要不然还回来的都是乱的，没法合并
private:
	CentralCache() = default;
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache&) = delete;
	static CentralCache _inst;
};