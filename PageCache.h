#include "Common.h"

class PageCache
{
public:
	static PageCache* GetInstance()
	{
		return &_inst;
	}    
	Span* NewSpan(size_t npage);
	Span* _NewSpan(size_t npage);
	//�ͷſ��е�span�ص�PageCache�����ϲ����ڵ�ҳ
	void ReleaseSpanToPageCache(Span* span);  
	//��ȡ����span��ӳ��
	Span* MapObjectToPageSpan(void* start);
	//��ϵͳ�ͷ��ڴ�
	void ReleaseMemoryToSystem();
	void ReleaseOnce(SpanList* spanlist);
private:
	SpanList _pagelist[NPAGES];
private:
	PageCache() = default;
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;

	static PageCache _inst;
	std::mutex _mtx;

	//std::unorder_map<PageID,Span*> _id_span_unordermap;
	std::map<PageID, Span*> _id_page_span_map;
};