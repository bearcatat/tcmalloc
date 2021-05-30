#if !defined(TCMALLOC_THREADCACHE_FREELIST)
#define TCMALLOC_THREADCACHE_FREELIST

#include <stddef.h>
#include <stdint.h>
#include <cassert>

namespace tcmalloc
{
    class ThreadCacheFreeList
    {
    private:
        void *list_;
        uint32_t length_;
        uint32_t lowater_;
        uint32_t max_length_;
        uint32_t length_overages_;
        size_t size_;

    public:
        void Init(size_t size)
        {
            list_ = NULL;
            length_ = 0;
            lowater_ = 0;
            max_length_ = 1;
            length_overages_ = 0;
            size_ = size;
        }

        size_t length() const
        {
            return length_;
        }
        size_t object_size() const
        {
            return size_;
        }
        size_t max_length() const
        {
            return max_length_;
        }
        void set_max_length(size_t new_max)
        {
            max_length_ = new_max;
        }
        size_t length_overages() const
        {
            return length_overages_;
        }
        void set_length_overages(size_t new_count)
        {
            length_overages_ = new_count;
        }
        bool empty() const
        {
            return list_ == NULL;
        }
        int lowatermark() const { return lowater_; }
        void clear_lowatermark() { lowater_ = length_; }
        uint32_t Push(void *object)
        {
            // std::cout << "Push:" << length_ << std::endl;
            *(reinterpret_cast<void **>(object)) = list_;
            list_ = object;
            length_++;
            return length_;
        }
        void *Pop()
        {
            assert(list_ != NULL);
            length_--;
            // std::cout << "Pop:" << length_ << std::endl;
            if (length_ < lowater_)
            {
                lowater_ = length_;
            }
            void *object = list_;
            list_ = *(reinterpret_cast<void **>(object));
            return object;
        }
        bool TryPop(void **obj_ptr)
        {
            if (list_ == NULL)
            {
                return false;
            }
            assert(list_ != NULL);
            length_--;
            // std::cout << "Pop:" << length_ << std::endl;
            if (length_ < lowater_)
            {
                lowater_ = length_;
            }
            *obj_ptr = list_;
            list_ = *(reinterpret_cast<void **>(list_));
            return true;
        }
        void *Next()
        {
            return *(reinterpret_cast<void **>(list_));
        }
        void PushRange(int N, void *start, void *end)
        {
            length_ += N;
            // std::cout << "pushrange:" << length_ << std::endl;
            *(reinterpret_cast<void **>(end)) = list_;
            list_ = start;
        }
        void PopRange(int N, void **start, void **end)
        {
            assert(N > 0);
            // std::cout << length_ << " " << N << std::endl;
            assert(length_ >= N);
            length_ -= N;
            // std::cout << "Poprange:" << length_ << std::endl;
            if (length_ < lowater_)
                lowater_ = length_;
            void *cur = list_, *prev = NULL;
            int num = 0;
            while (num < N && cur != NULL)
            {
                prev = cur;
                // tcmalloc::Span *span = tcmalloc::PageHeap::Instance()->PageIDToSpan(reinterpret_cast<tcmalloc::Length>(prev) >> tcmalloc::PageShitf);
                // assert(span->location == tcmalloc::Span::IN_USE);
                cur = *(reinterpret_cast<void **>(cur));
                num++;
            }
            *(start) = list_;
            *(end) = prev;
            *(reinterpret_cast<void **>(prev)) = NULL;
            list_ = cur;
        }
    };
}

#endif // TCMALLOC_THREADCACHE_FREELIST
