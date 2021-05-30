#if !defined(TCMALLOC_TCMALLOC)
#define TCMALLOC_TCMALLOC

#include "config.hpp"
#include "size_class.hpp"
#include "thread_cache.hpp"
#include "page_heap.hpp"
#include "span.hpp"
#include "iostream"

namespace tcmalloc
{
    void *tcmalloc(size_t size)
    {
        uint32_t sc;
        // std::cout << size << std::endl;
        if (SizeMap::Instance()->GetSizeClass(size, &sc))
        {

            ThreadCache *cache = ThreadCache::Current();
            void *obj = cache->New(sc);
            return obj;
        }
        Length npages = (size + PageSize - 1) / PageSize;
        Span *span = PageHeap::Instance()->New(npages);
        span->sizeclass = 0;
        return reinterpret_cast<void *>(span->start << PageShitf);
    }

    void tcfree(void *obj)
    {
        PageID pid = reinterpret_cast<Length>(obj) >> PageShitf;
        Span *span = PageHeap::Instance()->PageIDToSpan(pid);
        if (span->sizeclass != 0 && span->sizeclass < SmallObjectMaxClass)
        {
            ThreadCache *cache = ThreadCache::Current();
            cache->Delete(obj, span->sizeclass);
            return;
        }
        assert(reinterpret_cast<void *>(span->start << PageShitf) == obj);
        PageHeap::Instance()->Delete(span);
        return;
    }
}

#endif // TCMALLOC_TCMALLOC
