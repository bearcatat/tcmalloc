#include "../tcmalloc/tcmalloc.hpp"
#include <cassert>
#include <iostream>
#include <pthread.h>
#include "../tcmalloc/page_heap.hpp"
#include "../tcmalloc/span.hpp"
#include "../tcmalloc/size_class.hpp"

void TestSmallMalloc(size_t n)
{
    // std::cout << "Alloc" << std::endl;
    char *p = reinterpret_cast<char *>(tcmalloc::tcmalloc(n));
    // std::cout << "Write" << std::endl;
    for (int i = 0; i < n; i++)
    {
        p[i] = '1';
    }
    // std::cout << "Write Pass" << std::endl;

    for (int i = 0; i < n; i++)
    {
        assert(p[i] == '1');
    }
    // std::cout << "Read Pass" << std::endl;
    tcmalloc::Span *span = tcmalloc::PageHeap::Instance()->PageIDToSpan(reinterpret_cast<tcmalloc::PageID>(p) >> tcmalloc::PageShitf);
    // assert(span->location == tcmalloc::Span::IN_USE);
    // assert(span->sizeclass > 0 && span->sizeclass < tcmalloc::SmallObjectMaxClass);
    tcmalloc::tcfree(p);

    return;
}
void TestALLSmallObject()
{
    std::cout << "Small Test" << std::endl;
    int k = 1;
    for (int i = 1, cnt = 0; i < tcmalloc::SmallObjectMaxSize; i = i + k, cnt++)
    {
        // std::cout << i << std::endl;
        TestSmallMalloc(i);
        if (cnt % 1000 == 0)
        {
            // std::cout << i << " " << cnt << std::endl;
            k *= 2;
        }
    }
}

void TestAllLargeObject()
{
    std::cout << "Small Test" << std::endl;
    int k = 1;
    const size_t Mb = 1024 * 1024;
    const size_t largeMaxSize = Mb * 1024 * 2;
    for (int i = 1, cnt = 0; i < largeMaxSize; i = i + k, cnt++)
    {
        // std::cout << i << std::endl;
        TestSmallMalloc(i);
        if (cnt % 2 == 0)
        {
            // std::cout << i << " " << cnt << std::endl;
            k *= 2;
        }
    }
}

void TestHugeThreadCache()
{
    const int hugeNum = 70000;
    std::cout << "HugeTest" << std::endl;
    std::cout << sizeof(char *) * hugeNum << std::endl;
    char **array = (char **)(tcmalloc::tcmalloc(sizeof(char *) * hugeNum));
    u_int32_t sc;
    if (tcmalloc::SizeMap::Instance()->GetSizeClass(sizeof(char *) * hugeNum, &sc))
    {
        tcmalloc::Span *span = tcmalloc::PageHeap::Instance()->PageIDToSpan(reinterpret_cast<tcmalloc::Length>(array) >> tcmalloc::PageShitf);
        std::cout << array << std::endl;
        std::cout << span->sizeclass << " " << sc << std::endl;
        assert(span->sizeclass == sc);
    }

    for (int i = 0; i < hugeNum; i++)
    {
        array[i] = (char *)(tcmalloc::tcmalloc(sizeof(char) * 10));
        tcmalloc::Span *span = tcmalloc::PageHeap::Instance()->PageIDToSpan(reinterpret_cast<tcmalloc::Length>(array[i]) >> tcmalloc::PageShitf);
        assert(span->location == tcmalloc::Span::IN_USE);
        // std::cout << &(array[0]) << std::endl;
        for (int j = 0; j < 10; j++)
        {
            array[i][j] = '0' + j;
            // std::cout << array[i][j] << std::endl;
        }
    }
    // 140736664710592
    for (int i = 0; i < hugeNum; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            array[i][j] = '0' + j;
        }
        tcmalloc::Span *span = tcmalloc::PageHeap::Instance()->PageIDToSpan(reinterpret_cast<tcmalloc::Length>(array[i]) >> tcmalloc::PageShitf);
        assert(span->location == tcmalloc::Span::IN_USE);
        tcmalloc::tcfree(array[i]);
    }
    tcmalloc::tcfree(array);
}

void *TestThread(void *args)
{
    TestALLSmallObject();
    TestHugeThreadCache();
    TestAllLargeObject();

    return 0;
}
int main(int argc, char const *argv[])
{
    // TestSmallMalloc(10);
    // TestALLSmallObject();
    TestHugeThreadCache();
    // TestAllLargeObject();
    std::cout << "One Thread PASS" << std::endl;

    // const int NUM_THREAD = 20;
    // pthread_t tids[NUM_THREAD];
    // for (int i = 0; i < NUM_THREAD; i++)
    // {
    //     int ret = pthread_create(&tids[i], NULL, TestThread, NULL);
    //     if (ret != 0)
    //     {
    //         std::cout << "pthread create error" << ret << std::endl;
    //     }
    // }
    // for (int i = 0; i < NUM_THREAD; i++)
    // {
    //     pthread_join(tids[i], NULL);
    // }
    // std::cout << "Threads PASS" << std::endl;
    // pthread_exit(NULL);
    return 0;
}
