#include "ThreadCache.h"
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAXBYTES);
	//对齐取整，换算成对齐后的大小
	size = SizeClass::Roundup(size);
	size_t index = SizeClass::Index(size);
	FreeList& freelist = _freelist[index];
	if (!freelist.Empty()){
		return freelist.pop();
	}
	else{
		return FetchFromCentralCache(size, index);
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	assert(size <= MAXBYTES);
	size_t index = SizeClass::Index(size);
	FreeList& freelist = _freelist[index];
	freelist.push(ptr);

	//当自由链表的数量超过一次批量从中心缓存移动的数量时，开始回收到中心缓存
	if (freelist.Size() >= freelist.MaxSize()){
		ListTooLong(freelist, size);
	}
	//当freelist超过2M时回收
}
void ThreadCache::ListTooLong(FreeList&	freelist,size_t size)
{
	void* start = freelist.Clear();
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}

void* ThreadCache::FetchFromCentralCache(size_t size, size_t index)
{
	FreeList& freelist = _freelist[index];
	//size_t num = 10;		//慢增长的策略
	size_t num = min(SizeClass::NumMoveSize(size), freelist.MaxSize());
	void* start, *end;
	size_t fetchnum = CentralCache::GetInstance()->FetchRangeObj(start, end, num, size);
	if (fetchnum > 1){
		freelist.PushRange(NEXT_OBJ(start), end, fetchnum - 1);
	}
	if (num == freelist.MaxSize()){
		freelist.SetMaxSize(num + 1);
	}
	//申请了一个对象的空间
	//if (fetchnum == 1){
	//	return start;
	//}
	////它要返回一块内存，把start的下一个以后的压进去
	//freelist.PushRange(NEXT_OBJ(start), end, fetchnum - 1);  
	return start;
}