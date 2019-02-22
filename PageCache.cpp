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
			//��һ��128ҳλ�õ� pageid=0
			Span* span = pagelist.PopFront();
			Span* split = new Span;
			split->_pageid = span->_pageid + span->_npage - npage;
			split->_npage = npage;
			span->_npage -= npage;
			_pagelist[span->_npage].PushFront(span);
			for (size_t i = 0; i < split->_npage; i++){  
				//����value������
				_id_page_span_map[split->_pageid + i] = split;
			}
			return split;
		}
	}
	//��Ҫ��ϵͳ�����ڴ�,Ҫ��MEM_COMMIT
	void* ptr = SystemMemory(NPAGES - 1);
	Span* largespan = new Span;
	largespan->_pageid = ((PageID)ptr) >> PAGE_SHIFT;
	largespan->_npage = NPAGES - 1;
	_pagelist[NPAGES - 1].PushFront(largespan);
	for (size_t i = 0; i < largespan->_npage; i++){
		//����value������
		_id_page_span_map[largespan->_pageid + i] = largespan;
	}
	//���������и�
	return _NewSpan(npage);
}

//��ȡ����span��ӳ��
Span* PageCache::MapObjectToPageSpan(void* obj)
{
	PageID pageid = (PageID)obj >> PAGE_SHIFT;
	auto it = _id_page_span_map.find(pageid);
	assert(it != _id_page_span_map.end());
	return it->second;
}
//�ͷſ��е�span�ص�PageCache�����ϲ�����span
void PageCache::ReleaseSpanToPageCache(Span* span)
{
	std::unique_lock<std::mutex> lock(_mtx);
	if (span->_npage >= NPAGES){
		void* ptr = (void*)(span->_pageid >> PAGE_SHIFT);
		VirtualFree(ptr, 0, MEM_RELEASE);
		return;
	}
	//��ǰ�ϲ�
	auto previt = _id_page_span_map.find(span->_pageid - 1);
	while (previt != _id_page_span_map.end()){
		Span* prevspan = previt->second;
		//���ǿ��еģ���ֱ������
		if (prevspan->_usecount != 0){
			break;
		}
		//����ϲ�����NPAGESҳ��span,�򲻺ϲ�������û�취����
		if (prevspan->_npage + span->_npage >= NPAGES){
			break;
		}
		_pagelist[prevspan->_npage].Erase(prevspan);
		prevspan->_npage += span->_npage;
		delete span;
		span = prevspan;
		previt = _id_page_span_map.find(span->_pageid - 1);
	}
	//���ϲ�
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
	//�޸�ӳ���ϵ
	for (size_t i = 0; i < span->_npage; i++){
		_id_page_span_map[span->_pageid + i] = span;
	}
	//����freelist
	_pagelist[span->_npage].PushFront(span);
}

