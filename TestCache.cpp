#include "ConcurrentAlloc.h"
#include <vector>
#include <Windows.h>
using std::vector;
using std::thread;

void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	vector<thread> vthread(nworks);
	size_t malloc_costtime = 0;
	size_t free_costtime = 0;
	for (size_t k = 0; k < nworks; k++){
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(malloc(1025));
				}
				size_t end1 = clock();
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					free(v[i]);
				}
				size_t end2 = clock();
				v.clear();
				malloc_costtime += end1 - begin1;
				free_costtime += end2 - begin2;
			}
		});
	}

	for (auto& t : vthread)
	{
		t.join();
	}
	printf("%u个线程并发执行%u轮次，每轮次BenchmarkMalloc alloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, malloc_costtime);
	printf("%u个线程并发执行%u轮次，每轮次BenchmarkMalloc dealloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, free_costtime);
	printf("%u个线程并发BenchmarkMalloc alloc&dealloc %u次，总计花费：%u ms\n",
		nworks, nworks*rounds*ntimes, malloc_costtime + free_costtime);
}

void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	size_t malloc_costtime = 0;
	size_t free_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		//以&的方式捕获外面的所有对象
		vthread[k] = std::thread([&]() {
			std::vector<void*> v;
			v.reserve(ntimes);
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					v.push_back(ConcurrentAlloc(1025));
				}
				size_t end1 = clock();
				size_t begin2 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
					ConcurrentFree(v[i]);
				}
				size_t end2 = clock();
				v.clear();
				malloc_costtime += end1 - begin1;
				free_costtime += end2 - begin2;
			}
		});
	}
	printf("%u个线程并发执行%u轮次，每轮次concurrent alloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, malloc_costtime);
	printf("%u个线程并发执行%u轮次，每轮次concurrent dealloc %u次: 花费：%u ms\n",
		nworks, rounds, ntimes, free_costtime);
	printf("%u个线程并发concurrent alloc&dealloc %u次，总计花费：%u ms\n",
		nworks, nworks*rounds*ntimes, malloc_costtime + free_costtime);
	for (auto& t : vthread)
	{
		t.join();
	}

}
void TestConcurrentAlloc()
{
	size_t n = 100000;
	std::vector<void*> v;
	for (size_t i = 0; i < n; ++i)
	{
		v.push_back(ConcurrentAlloc(10));
		cout << v.back() << endl;
	}
	cout << endl << endl;

	for (size_t i = 0; i < n; ++i)
	{
		ConcurrentFree(v[i]);
	}
	v.clear();

	for (size_t i = 0; i < n; ++i)
	{
		v.push_back(ConcurrentAlloc(10));
		//cout << v.back() << endl;
	}

	for (size_t i = 0; i < n; ++i)
	{
		ConcurrentFree(v[i]);
	}
	v.clear();
}

void TestThreadCache()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 11; i++){
		v.push_back(ConcurrentAlloc(12));
		cout << v.back() << endl;
	}
	v.clear();
	cout << endl;
	for (size_t i = 0; i < 11; i++){
		v.push_back(ConcurrentAlloc(12));
		cout << v.back() << endl;
	}
	for (size_t i = 0; i < 10; i++){
		ConcurrentFree(v[i]);
	}
}
void TestPageCache()
{
	//void* ptr = malloc((NPAGES - 1) << PAGE_SHIFT);
	void* ptr = ConcurrentAlloc(129 << 12);
	cout << ptr << endl;
	if (ptr == nullptr)
	{
		throw std::bad_alloc();
	}
	ConcurrentFree(ptr);
	PageID pageid = (PageID)ptr >> PAGE_SHIFT;
	cout << pageid << endl;

	void* shiftptr = (void*)(pageid << PAGE_SHIFT);
	cout << shiftptr << endl;
}

void TestFree()
{
	std::vector<void*> v;
	for (size_t i = 0; i < 3; i++){
		v.push_back(ConcurrentAlloc(4));
		cout << v.back() << endl;
	}
	cout << endl;
	for (size_t i = 0; i < 3; i++){
		ConcurrentFree(v[i]);
	}
	v.clear();
	cout << endl;
	for (size_t i = 0; i < 10; i++){
		v.push_back(ConcurrentAlloc(4));
		cout << v.back() << endl;
	}
}

int main()
{
	//TestConcurrentAlloc();
	//TestThreadCache();
	//TestPageCache();
	//TestFree();
	cout << "==========================================================" << endl;
	BenchmarkMalloc(10000, 10, 10);
	cout << endl << endl;
	BenchmarkConcurrentMalloc(10000, 10, 10);
	cout << "==========================================================" << endl;
	return 0;
}

//Lambda表达式
