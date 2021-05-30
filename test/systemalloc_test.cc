#include <stdio.h>
#include "../tcmalloc/system_alloc.hpp"
#include <limits>
#include <cassert>
#include <iostream>

void TryAllocAndRelease()
{
    size_t size = 8 * 1024;
    void *ptr = tcmalloc::TCMalloc_SystemAlloc(size);
    for (int i = 0; i < 2 * 1024; i++)
    {
        reinterpret_cast<int *>(ptr)[i] = i;
    }
    tcmalloc::TCMalloc_SystemRelease(ptr, size);
}
void TestBaseRetryFailTest()
{
    const size_t kHugeSize = (std::numeric_limits<size_t>::max)();
    void *ptr1 = tcmalloc::TCMalloc_SystemAlloc(kHugeSize);
    void *ptr2 = tcmalloc::TCMalloc_SystemAlloc(kHugeSize);
    // assert(ptr2 == NULL);
    if (ptr1 != NULL)
        tcmalloc::TCMalloc_SystemRelease(ptr1, kHugeSize);
    if (ptr2 != NULL)
        tcmalloc::TCMalloc_SystemRelease(ptr2, kHugeSize);
    void *ptr3 = tcmalloc::TCMalloc_SystemAlloc(1024);
    assert(ptr3 != NULL);
    tcmalloc::TCMalloc_SystemRelease(ptr3, kHugeSize);
}

int main(int argc, char const *argv[])
{
    TryAllocAndRelease();
    TestBaseRetryFailTest();
    printf("PASS\n");
    return 0;
}
