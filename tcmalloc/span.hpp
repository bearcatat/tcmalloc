#if !defined(TCMALLOC_SPAN)
#define TCMALLOC_SPAN

#include <set>
#include <cassert>
#include <stdint.h>
#include <cstring>

#include "config.hpp"
#include "page_heap_allocator.hpp"

namespace tcmalloc
{
    struct Span
    {
        PageID start;
        Length length;
        Span *next;
        Span *prev;
        void *free_objects;

        enum Location
        {
            IN_USE,
            IN_NORMAL,
            IN_RETURNED
        };

        uint64_t ref_count;
        uint64_t all_count;
        uint64_t location;
        uint32_t sizeclass;
    };
    static PageHeapAllocator<Span> span_allocator;

    Span *NewSpan(PageID start, Length length)
    {
        Span *new_span = span_allocator.New();
        memset(new_span, 0, sizeof(*new_span));
        new_span->start = start;
        new_span->length = length;
        new_span->next = NULL;
        new_span->prev = NULL;
        new_span->ref_count = 0;
        new_span->sizeclass = 0;
        
        return new_span;
    }
    void DeleteSpan(Span *span)
    {
        span_allocator.Delete(span);
    }
    void DLLInit(Span *list)
    {
        list->prev = list;
        list->next = list;
    }
    bool DLLEmpty(Span *list)
    {
        return list->next == list;
    }
    void DLLInsert(Span *before, Span *span)
    {
        assert(before != NULL);
        assert(span != NULL);
        span->prev = before;
        span->next = before->next;
        before->next->prev = span;
        before->next = span;
    }
    void DLLRemove(Span *span)
    {
        span->prev->next = span->next;
        span->next->prev = span->prev;
        span->prev = NULL;
        span->next = NULL;
    }
    struct SpanBestFitLess
    {
        bool operator()(Span *a, Span *b)
        {
            if (a->length < b->length)
                return true;
            if (a->length > b->length)
                return false;
            return a->start < b->start;
        }
    };
    typedef std::set<Span *, SpanBestFitLess, STLPageHeapAllocator<Span *, 0>> SpanSet;
}

#endif // TCMALLOC_SPAN
