#include "ThreadCache.h"
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAXBYTES);
	//����ȡ��������ɶ����Ĵ�С
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

	//�������������������һ�����������Ļ����ƶ�������ʱ����ʼ���յ����Ļ���
	if (freelist.Size() >= freelist.MaxSize()){
		ListTooLong(freelist, size);
	}
	//��freelist����2Mʱ����
}
void ThreadCache::ListTooLong(FreeList&	freelist,size_t size)
{
	void* start = freelist.Clear();
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}

void* ThreadCache::FetchFromCentralCache(size_t size, size_t index)
{
	FreeList& freelist = _freelist[index];
	//size_t num = 10;		//�������Ĳ���
	size_t num = min(SizeClass::NumMoveSize(size), freelist.MaxSize());
	void* start, *end;
	size_t fetchnum = CentralCache::GetInstance()->FetchRangeObj(start, end, num, size);
	if (fetchnum > 1){
		freelist.PushRange(NEXT_OBJ(start), end, fetchnum - 1);
	}
	if (num == freelist.MaxSize()){
		freelist.SetMaxSize(num + 1);
	}
	//������һ������Ŀռ�
	//if (fetchnum == 1){
	//	return start;
	//}
	////��Ҫ����һ���ڴ棬��start����һ���Ժ��ѹ��ȥ
	//freelist.PushRange(NEXT_OBJ(start), end, fetchnum - 1);  
	return start;
}