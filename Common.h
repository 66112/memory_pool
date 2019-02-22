#pragma once
#include <iostream>
#include <assert.h>
#include <thread>
#include <vector>
#include <mutex>	
#include <unordered_map>
#include <map>

//不能用lock,unlock因为在锁住后，到系统申请内存，如果失败，会抛异常，导致死锁
#ifdef _WIN32
#include <windows.h>
#endif // _WIN32
using std::cout;
using std::endl;

//管理对象自由链表的长度
const size_t NLISTS = 240;
const size_t MAXBYTES = 64 * 1024;   //64K
const size_t PAGE_SHIFT = 12;
const size_t NPAGES = 129;

static inline void*& NEXT_OBJ(void* obj)
{
	return *((void**)obj);
}

typedef size_t PageID;		 //64位，页就多了，要变
struct Span
{
	PageID _pageid = 0;			//页号
	size_t _npage = 0;			//页的数量
	Span* _next = nullptr;		//下一个Span
	Span* _prev = nullptr;

	void* _objlist = nullptr;	//自由链表
	size_t _objsize = 0;		//挂的对象大小
	size_t _usecount = 0;		//使用计数  
};

class SpanList
{
public:
	//双向循环链表
	SpanList(){
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}
	Span*& Begin()
	{
		return _head->_next;
	}
	Span* End()
	{
		return _head;
	}
	bool Empty()
	{
		return _head->_next == _head;
	}
	void Insert(Span* cur, Span* newspan)
	{
		assert(cur);
		Span* prev = cur->_prev;
		prev->_next = newspan;
		newspan->_prev = prev;
		newspan->_next = cur;
		cur->_prev = newspan;
	}
	void Erase(Span* cur)
	{
		assert(cur && cur != _head);
		Span* prev = cur->_prev;
		Span* next = cur->_next;
		prev->_next = next;
		next->_prev = prev;
	}
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}
	Span* PopFront()
	{
		Span* span = Begin();
		Erase(span);
		return span;
	}
	void PushBack(Span* span)
	{
		Insert(End(), span);
	}
	Span* PopBack()
	{
		Span* tail = _head->_prev;
		Erase(tail);
		return tail;
	}
public:
	std::mutex _mtx;	 
	//每一个spanlist都要加锁，不能给整个CentralCache加锁，会影响效率
private:
	Span* _head;
};

class FreeList
{
public:
	bool Empty()
	{
		return _ptr == nullptr;
	}
	void* pop()    //往出给
	{
		void* obj = _ptr;
		_ptr = NEXT_OBJ(_ptr);
		--_size;
		return obj;
	}
	void push(void* obj)   //往里挂
	{
		//相当于头插一个节点
		NEXT_OBJ(obj) = _ptr;  
		_ptr = obj;
		++_size;
	}
	void PushRange(void* start, void* end, size_t num)		//批量插入num个节点
	{
		NEXT_OBJ(end) = _ptr;
		_ptr = start;
		_size += num;
	}
	void* Clear()
	{
		_size = 0;			//不用置_maxsize
		void* list = _ptr;
		_ptr = nullptr;     //清空  
		return list;
	}
	size_t Size()
	{
		return _size;
	}
	size_t MaxSize()
	{
		return _maxsize;
	}
	void SetMaxSize(size_t size)
	{
		_maxsize = size;
	}
private:
	void* _ptr = nullptr;
	size_t _maxsize = 1;   //最多挂的节点数
	size_t _size = 0;      //挂的节点数
};

//管理内存对齐的映射
class SizeClass
{
	// 控制在12%左右的内碎片浪费
	// [1,128]				8byte对齐 freelist[0,16)
	// [129,1024]			16byte对齐 freelist[16,72)    //申请129个字节，浪费了15个
	// [1025,8*1024]		128byte对齐 freelist[72,128)
	// [8*1024+1,64*1024]	1024byte对齐 freelist[128,240)
public:
	static inline size_t _Roundup(size_t size, size_t align)
	{
		return (size + align - 1)&~(align - 1);
	}
	static inline size_t Roundup(size_t size)   //对齐取整，返回对齐后，对象需要空间的大小
	{
		if (size <= 128){
			return _Roundup(size, 8);
		}
		if (size <= 1024){
			return _Roundup(size, 16);
		}
		if (size <= 8192){
			return _Roundup(size, 128);
		}
		if (size <= 65536){
			return _Roundup(size, 1024);
		}
		return -1;
	}
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}
	//返回对象应该放在哪个位置
	static inline size_t Index(size_t bytes)
	{
		//每个区间都有多少个自由链表，区间按对齐数分
		static int group_array[4] = { 16, 56, 56, 128 };
		if (bytes <= 128){
			return _Index(bytes, 3);
		}
		if (bytes <= 1024){
			return _Index(bytes - 128, 4) + group_array[0];
		}
		if (bytes <= 8192){
			return _Index(bytes - 1024, 7) + group_array[0] + group_array[1];
		}
		if (bytes <= 65536){
			return _Index(bytes - 8192, 10) + group_array[0] + group_array[1] + group_array[2];
		}
		return -1;
	}
	//计算给多少个size大小的对象
	static size_t NumMoveSize(size_t size)
	{
		int num = (int)(MAXBYTES / size);
		if (num < 2){
			num = 2;
		}
		if (num>512){
			num = 512;
		}
		return num;
	}
	
	//计算一次性向系统获取几个页
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num*size;
		npage >>= PAGE_SHIFT;
		if (npage == 0)
			npage = 1;
		return npage;
	}
};