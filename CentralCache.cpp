#include "CentralCache.h"
#include "PageCache.h"
CentralCache CentralCache::_inst;   //����һ�¾�̬��Ա����

//�����Ļ����ȡһ�������Ķ����threadcache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t num, size_t size)
{
	//����threadcache
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
	//����ǰͰ���� RAII
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

//sizeΪ�ҵ�ÿ������Ĵ�С
Span* CentralCache::GetOneSpan(SpanList* spanlist, size_t size)
{
	Span* span = spanlist->Begin();
	while (span != spanlist->End()){
		if (span->_objlist){
			return span;
		}
		span = span->_next;
	}
	//��pagecache����һ�����ʴ�С��span
	size_t npage = SizeClass::NumMovePage(size);
	Span* newspan = PageCache::GetInstance()->NewSpan(npage);
	//_id_centralspan_map[newspan->_pageid] = newspan;
	//��span���ڴ��и��һ����size��С�Ķ��������
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
	//���µ�span���ڶ�Ӧλ��
	spanlist->PushFront(newspan);
	return newspan;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	size_t index = SizeClass::Index(size);
	SpanList& spanlist = _spanList[index];

	std::unique_lock<std::mutex> lock(spanlist._mtx);
	//��ֹ���쳣���������
	//RAII���ݶ������������������ͷ���
	//������ͬλ�õ�spanlist����Ӱ��
	while (start){
		Span* span = PageCache::GetInstance()->MapObjectToPageSpan(start); //����
		//һ��һ��ͷ��
		void* next = NEXT_OBJ(start);
	
		//���ͷŶ���
		
		NEXT_OBJ(start) = span->_objlist;
		span->_objlist = start;
		//usecount == 0��ʾspan�г�ȥ�Ķ��󶼻�������
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
