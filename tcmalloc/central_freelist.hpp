#if !defined(TCMALLOC_CENTRAL_FREELIST)
#define TCMALLOC_CENTRAL_FREELIST
#include <algorithm>
#include "span.hpp"
#include "config.hpp"
#include <mutex>
#include "page_heap.hpp"
#include "size_class.hpp"
#include <iostream>
#include <cassert>
#include <iomanip>

namespace tcmalloc
{
    class CentralFreeList
    {
    private:
        struct TCEntry
        {
            void *head;
            void *tail;
        };
        std::mutex lock;
        uint32_t sizeclass;
        Span empty;
        Span non_empty;
        size_t num_spans;
        size_t counter; //number of free objects

        TCEntry tc_slots_[CentralFreelistMaxEntries];

        int32_t used_slots;
        int32_t cache_size;
        int32_t max_cache_size;

        void ReleaseToSpans(void *object)
        {
            Span *span = PageHeap::Instance()->PageIDToSpan(reinterpret_cast<size_t>(object) >> PageShitf);
            if (span->free_objects == NULL)
            {
                DLLRemove(span);
                DLLInsert(&non_empty, span);
            }
            counter++;
            span->ref_count--;
            // std::cout << counter << std::endl;
            if (span->ref_count == 0)
            {
                DLLRemove(span);
                num_spans--;
                counter -= span->all_count;
                PageHeap::Instance()->Delete(span);
                // std::cout << "Span To PageHeap" << std::endl;
            }
            else
            {
                *(reinterpret_cast<void **>(object)) = span->free_objects;
                span->free_objects = object;
            }
        }
        void ReleaseListToSpans(void *start)
        {
            while (start)
            {
                void *next = *(reinterpret_cast<void **>(start));
                ReleaseToSpans(start);
                start = next;
            }
        }
        bool MakeCacheSpace()
        {
            if (used_slots < cache_size)
                return true;
            if (cache_size == max_cache_size)
                return false;
            cache_size++;
            return true;
        }
        void Populate()
        {
            Length npages = SizeMap::Instance()->class_to_pages(sizeclass);
            Span *span = PageHeap::Instance()->New(npages);
            if (span == NULL)
                return;
            span->location = Span::IN_USE;
            PageHeap::Instance()->RegisterSizeClass(span, sizeclass);

            span->free_objects = NULL;
            const size_t byte = SizeMap::Instance()->class_to_size(sizeclass);
            char *start = reinterpret_cast<char *>(span->start << PageShitf);
            const char *limit = start + (span->length << PageShitf);
            int num = 0;
            for (; start + byte <= limit; start += byte)
            {
                *(reinterpret_cast<void **>(start)) = span->free_objects;
                span->free_objects = reinterpret_cast<void *>(start);
                num++;
            }
            span->all_count = num;
            DLLInsert(&non_empty, span);
            num_spans++;
            counter += num;
        }
        int FetchFromOneSpan(int N, void **start, void **end)
        {
            if (DLLEmpty(&non_empty))
                return 0;
            Span *span = non_empty.next;
            int num = 0;
            void *curr = span->free_objects, *prev = NULL;
            while (num < N && curr != NULL)
            {
                prev = curr;
                curr = *(reinterpret_cast<void **>(curr));
                num++;
            }
            *start = span->free_objects;
            *end = prev;
            *(reinterpret_cast<void **>(prev)) = NULL;
            counter -= num;
            span->ref_count += num;
            span->free_objects = curr;
            if (span->free_objects == NULL)
            {
                assert(span->prev != NULL && span->next != NULL);
                DLLRemove(span);
                DLLInsert(&empty, span);
            }
            return num;
        }
        int FetchFromOneSpanSafe(int N, void **start, void **end)
        {
            int num = FetchFromOneSpan(N, start, end);
            if (num == 0)
            {
                Populate();
                num = FetchFromOneSpan(N, start, end);
            }
            return num;
        }

    public:
        void Init(uint32_t sc)
        {
            sizeclass = sc;
            DLLInit(&empty);
            DLLInit(&non_empty);
            num_spans = 0;
            counter = 0;
            used_slots = 0;
            cache_size = 16;
            max_cache_size = CentralFreelistMaxEntries;
            assert(sc > 0);

            int32_t bytes = SizeMap::Instance()->class_to_size(sc);
            int32_t objs_to_move = SizeMap::Instance()->num_objects_to_move(sc);
            max_cache_size = std::min(max_cache_size, std::max(1, 1024 * 1024 / (bytes * objs_to_move)));
            cache_size = std::min(cache_size, max_cache_size);
            assert(cache_size <= max_cache_size);
        }
        void InsertRange(void *start, void *end, int N)
        {
            std::lock_guard<std::mutex> guard(lock);
            if (N == SizeMap::Instance()->num_objects_to_move(sizeclass) && MakeCacheSpace())
            {
                // std::cout << "cache" << std::endl;
                // std::cout << used_slots << " " << cache_size << " " << std::endl;
                // std::cout << used_slots << std::endl;
                int slot_idx = used_slots++;
                assert(slot_idx >= 0);
                assert(slot_idx < max_cache_size);
                tc_slots_[slot_idx].head = start;
                tc_slots_[slot_idx].tail = end;
                return;
            }
            ReleaseListToSpans(start);
        }
        int RemoveRange(void **start, void **end, int N)
        {
            assert(N > 0);
            std::lock_guard<std::mutex> guard(lock);
            // std::cout << N << " " << SizeMap::Instance()->num_objects_to_move(sizeclass) << std::endl;
            if (N == SizeMap::Instance()->num_objects_to_move(sizeclass) && used_slots > 0)
            {
                // std::cout << used_slots << std::endl;
                int slot_idx = --used_slots;
                assert(slot_idx >= 0);
                assert(slot_idx < max_cache_size);
                *start = tc_slots_[slot_idx].head;
                *end = tc_slots_[slot_idx].tail;
                return N;
            }
            *start = NULL;
            *end = NULL;
            int num = FetchFromOneSpanSafe(N, start, end);
            if (num)
            {
                while (num < N)
                {
                    void *head, *tail;
                    int n = FetchFromOneSpan(N - num, &head, &tail);
                    if (n == 0)
                        break;
                    num += n;
                    *(reinterpret_cast<void **>(tail)) = *start;
                    *start = head;
                }
            }

            return num;
        }
        bool CheckState()
        {
            // std::cout << counter << " "
            //           << num_spans << std::endl;
            assert(used_slots <= cache_size);
            assert(cache_size <= max_cache_size);
            size_t free_objects_num = 0;
            size_t test_num_spans = 0;
            // free_objects_num = SizeMap::Instance()->num_objects_to_move(sizeclass) * used_slots;
            for (Span *span = empty.next; span != &empty; span = span->next)
            {
                assert(span->location == Span::IN_USE);
                assert(span->free_objects == NULL);
                assert(span->sizeclass == sizeclass);
                assert(span->ref_count >= 0);
                test_num_spans++;
            }
            for (Span *span = non_empty.next; span != &non_empty; span = span->next)
            {
                assert(span->location == Span::IN_USE);
                assert(span->free_objects != NULL);
                assert(span->sizeclass == sizeclass);
                assert(span->ref_count >= 0);
                test_num_spans++;
                for (void *start = span->free_objects; start != NULL; start = *(reinterpret_cast<void **>(start)))
                {
                    free_objects_num++;
                }
            }
            assert(free_objects_num == counter);
            assert(test_num_spans == num_spans);
            return true;
        }
    };
}

#endif // TCMALLOC_CENTRAL_FREELIST
