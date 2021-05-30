#if !defined(TCMALLOC_STATIC_VAR)
#define TCMALLOC_STATIC_VAR

#include "config.hpp"
// #include "size_class.hpp"
// #include "page_heap.hpp"
// #include "page_heap_allocator.hpp"

namespace tcmalloc
{
    class Static
    {
    public:
        // static SizeMap size_map;
        // static PageHeap page_heap;
        static void InitStaticVar()
        {
        }
    };
    // SizeMap Static::size_map;
    // PageHeap Static::page_heap;
}

#endif // TCMALLOC_STATIC_VAR
