#pragma once
#include "Common.h"

class CentralCache
{
public:
	static CentralCache* GetInstance(){
		return &_inst;
	}

	Span* GetOneSpan(SpanList* spanlist, size_t size);

	//�����Ļ����ȡһ�������Ķ����threadcache
	size_t FetchRangeObj(void*& start, void*& end, size_t num, size_t size);
	//��һ�������Ķ����ͷŵ�span
	void ReleaseListToSpans(void* start, size_t size);
private:
	//���Ļ�����������
	SpanList _spanList[NLISTS];		//�����ڴ�飬Ҫ��Ȼ�������Ķ����ҵģ�û���ϲ�
private:
	CentralCache() = default;
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache&) = delete;
	static CentralCache _inst;
};