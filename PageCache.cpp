#include "PageCache.h"

PageCache PageCache::_inst;
void* SystemMemory(size_t npage)
{
	void* ptr = VirtualAlloc(NULL, (npage << PAGE_SHIFT), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (ptr == nullptr){
		throw std::bad_alloc();
	}
	return ptr;
}
Span* PageCache::NewSpan(size_t npage)
{
	std::unique_lock<std::mutex> lock(_mtx);
	if (npage >= NPAGES){
		void* ptr = SystemMemory(npage); 
		Span* span = new Span;
		span->_pageid = (PageID)ptr >> PAGE_SHIFT;
		span->_npage = npage;
		span->_objsize = npage << PAGE_SHIFT;
		_id_page_span_map[span->_pageid] = span;
		return span;
	}
	return _NewSpan(npage);
}

Span* PageCache::_NewSpan(size_t npage)
{
	if (!_pagelist[npage].Empty()){
		return _pagelist[npage].PopFront();
	}
	for (size_t i = npage + 1; i < NPAGES; i++){
		SpanList& pagelist = _pagelist[i];
		if (!pagelist.Empty()){
			//第一次128页位置的 pageid=0
			Span* span = pagelist.PopFront();
			Span* split = new Span;
			split->_pageid = span->_pageid + span->_npage - npage;
			split->_npage = npage;
			span->_npage -= npage;
			_pagelist[span->_npage].PushFront(span);
			for (size_t i = 0; i < split->_npage; i++){  
				//返回value的引用
				_id_page_span_map[split->_pageid + i] = split;
			}
			return split;
		}
	}
	//需要向系统申请内存,要加MEM_COMMIT
	void* ptr = SystemMemory(NPAGES - 1);
	Span* largespan = new Span;
	largespan->_pageid = ((PageID)ptr) >> PAGE_SHIFT;
	largespan->_npage = NPAGES - 1;
	_pagelist[NPAGES - 1].PushFront(largespan);
	for (size_t i = 0; i < largespan->_npage; i++){
		//返回value的引用
		_id_page_span_map[largespan->_pageid + i] = largespan;
	}
	//添加完后再切割
	return _NewSpan(npage);
}

//获取对象到span的映射
Span* PageCache::MapObjectToPageSpan(void* obj)
{
	PageID pageid = (PageID)obj >> PAGE_SHIFT;
	auto it = _id_page_span_map.find(pageid);
	assert(it != _id_page_span_map.end());
	return it->second;
}
//释放空闲的span回到PageCache，并合并相邻span
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	std::unique_lock<std::mutex> lock(_mtx);
	if (span->_npage >= NPAGES){
		void* ptr = (void*)(span->_pageid >> PAGE_SHIFT);
		VirtualFree(ptr, 0, MEM_RELEASE);
		return;
	}
	//向前合并
	auto previt = _id_page_span_map.find(span->_pageid - 1);
	while (previt != _id_page_span_map.end()){
		Span* prevspan = previt->second;
		//不是空闲的，则直接跳出
		if (prevspan->_usecount != 0){
			break;
		}
		//如果合并超出NPAGES页的span,则不合并，否则没办法管理
		if (prevspan->_npage + span->_npage >= NPAGES){
			break;
		}
		_pagelist[prevspan->_npage].Erase(prevspan);
		prevspan->_npage += span->_npage;
		delete span;
		span = prevspan;
		previt = _id_page_span_map.find(span->_pageid - 1);
	}
	//向后合并
	auto nextit = _id_page_span_map.find(span->_pageid + span->_npage);
	while (nextit != _id_page_span_map.end()){
		Span* nextspan = nextit->second;
		if (nextspan->_usecount != 0 || span->_npage+nextspan->_npage>=NPAGES){
			break;
		}
		_pagelist[nextspan->_npage].Erase(nextspan);
		span->_npage += nextspan->_npage;
		delete nextspan;
		nextit = _id_page_span_map.find(span->_pageid + span->_npage);
	}
	//修改映射关系
	for (size_t i = 0; i < span->_npage; i++){
		_id_page_span_map[span->_pageid + i] = span;
	}
	//重置freelist
	_pagelist[span->_npage].PushFront(span);
}

