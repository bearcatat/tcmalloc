#include "../tcmalloc/tcmalloc.hpp"
#include <cassert>
#include <iostream>
#include <pthread.h>
#include "../tcmalloc/page_heap.hpp"
#include "../tcmalloc/span.hpp"
#include "../tcmalloc/size_class.hpp"

using std::cout;
using std::endl;

void TryAllocExpectFail(size_t size)
{
    void *p1 = tcmalloc::tcmalloc(size);
    assert(p1 == NULL);
    void *p2 = tcmalloc::tcmalloc(1);
    assert(p2 != NULL);
    free(p2);
}

int main(int argc, char const *argv[])
{
    void *p_small = tcmalloc::tcmalloc(4 * 1024 * 1024);
    assert(p_small != NULL);

    cout << "Test malloc(0 - N)\n";
    const size_t zero = 0;
    static const size_t kMinusNTimes = 16384;
    for (size_t i = 1; i < kMinusNTimes; ++i)
    {
        TryAllocExpectFail(zero - i);
    }

    cout << "Test malloc(0-1048576-N)" << endl;
    static const size_t kMinusMBMinusNTimes = 16384;
    for (size_t i = 0; i < kMinusMBMinusNTimes; ++i)
    {
        TryAllocExpectFail(zero - 1048576 - i);
    }
    return 0;
}
