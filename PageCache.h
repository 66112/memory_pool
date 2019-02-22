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
	//释放空闲的span回到PageCache，并合并相邻的页
	void ReleaseSpanToPageCache(Span* span);  
	//获取对象到span的映射
	Span* MapObjectToPageSpan(void* start);
	//向系统释放内存
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