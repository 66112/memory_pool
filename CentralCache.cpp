#include "CentralCache.h"
#include "PageCache.h"
CentralCache CentralCache::_inst;   //定义一下静态成员变量

//从中心缓存获取一定数量的对象给threadcache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t num, size_t size)
{
	//测试threadcache
	//start = malloc(num*size);
	//end = (char*)start + size*(num - 1);
	//void* cur = start;
	//while (cur < end){
	//	void* next = (char*)cur + size;
	//	NEXT_OBJ(cur) = next;
	//	cur = next;
	//}
	//NEXT_OBJ(end) = nullptr;
	//return num;

	size_t index = SizeClass::Index(size);
	SpanList& spanlist = _spanList[index];
	//给当前桶加锁 RAII
	std::unique_lock<std::mutex> lock(spanlist._mtx);

	Span* span = GetOneSpan(&spanlist, size);
	void* cur = span->_objlist;
	void* prev = cur;
	size_t fetchnum = 0;
	while (cur && fetchnum < num){
		prev = cur;
		cur = NEXT_OBJ(cur);
		++fetchnum;
	}
	start = span->_objlist;
	end = prev;
	NEXT_OBJ(end) = nullptr;

	span->_objlist = cur;
	span->_usecount += fetchnum;
	return fetchnum;
}

//size为挂的每个对象的大小
Span* CentralCache::GetOneSpan(SpanList* spanlist, size_t size)
{
	Span* span = spanlist->Begin();
	while (span != spanlist->End()){
		if (span->_objlist){
			return span;
		}
		span = span->_next;
	}
	//向pagecache申请一个合适大小的span
	size_t npage = SizeClass::NumMovePage(size);
	Span* newspan = PageCache::GetInstance()->NewSpan(npage);
	//_id_centralspan_map[newspan->_pageid] = newspan;
	//将span的内存切割成一个个size大小的对象挂起来
	char* start = (char*)(newspan->_pageid << PAGE_SHIFT);
	char* end = start + (newspan->_npage << PAGE_SHIFT);
	char* cur = start;
	char* next = cur + size;
	while (next < end){
		NEXT_OBJ(cur) = next;
		cur = next;
		next = cur + size;
	}
	NEXT_OBJ((void*)cur) = nullptr;
	newspan->_objlist = start;
	newspan->_objsize = size;
	newspan->_usecount = 0;
	//将新的span挂在对应位置
	spanlist->PushFront(newspan);
	return newspan;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	SpanList& spanlist = _spanList[index];

	std::unique_lock<std::mutex> lock(spanlist._mtx);
	//防止抛异常，造成死锁
	//RAII根据对象的生命周期申请和释放锁
	//这样不同位置的spanlist互不影响
	while (start){
		Span* span = PageCache::GetInstance()->MapObjectToPageSpan(start); //错误
		//一个一个头插
		void* next = NEXT_OBJ(start);
	
		//当释放对象
		
		NEXT_OBJ(start) = span->_objlist;
		span->_objlist = start;
		//usecount == 0表示span切出去的对象都还回来了
		if (--span->_usecount == 0){
			spanlist.Erase(span);
			span->_objlist = nullptr;
			span->_objsize = 0;
			span->_prev = span->_next = nullptr;
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		}
		start = next;
	}
}
