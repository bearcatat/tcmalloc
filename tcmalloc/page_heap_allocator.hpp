#ifndef TCMALLOC_PAGE_HEAP_ALLOCATOR
#define TCMALLOC_PAGE_HEAP_ALLOCATOR

#include <cassert>

#include "config.hpp"
#include "system_alloc.hpp"

namespace tcmalloc
{
    template <class T>
    class PageHeapAllocator
    {
    private:
        void *free_list_;
        char *free_area_;
        size_t avail_area_; // size of free_area_;
        int inuse_;         //Number of allocated but unfreeed objects
        bool is_inited = false;

    public:
        void Init()
        {
            free_list_ = NULL;
            free_area_ = NULL;
            avail_area_ = 0;
            inuse_ = 0;
            is_inited = true;
        }
        T *New()
        {
            if (!is_inited)
            {
                Init();
            }
            void *result;
            if (free_list_ != NULL) // if free_list_ is not empty, new from free_list_
            {
                result = free_list_;
                free_list_ = *(reinterpret_cast<void **>(result));
            }
            else // if free_list_ is empty, new from free_area
            {
                if (avail_area_ < sizeof(T)) // if free_area is too small, alloc a new area from system
                {
                    free_area_ = reinterpret_cast<char *>(TCMalloc_SystemAlloc(PageHeapAllocSize));
                    if (free_area_ == NULL)
                    {
                        return NULL;
                    }
                    avail_area_ = PageHeapAllocSize;
                }
                result = free_area_;
                free_area_ += sizeof(T);
                avail_area_ -= sizeof(T);
            }
            inuse_++;
            return reinterpret_cast<T *>(result);
        }
        void Delete(T *p)
        {
            inuse_--;
            *(reinterpret_cast<void **>(p)) = free_list_;
            free_list_ = reinterpret_cast<void *>(p);
        }
    };
    // I guess this is used for span allocator for STL/set
    template <class T, int static_token>
    class STLPageHeapAllocator
    {
    private:
        static PageHeapAllocator<T> allocator;
        static bool initialized;

    public:
        typedef size_t size_type;
        typedef ptrdiff_t different_type;
        typedef T *pointer;
        typedef const T *const_pointer;
        typedef T &reference;
        typedef const T &const_reference;
        typedef T value_type;

        template <class T1>
        struct rebind
        {
            typedef STLPageHeapAllocator<T1, static_token> other;
        };

        STLPageHeapAllocator()
        {
        }
        //
        STLPageHeapAllocator(const STLPageHeapAllocator &)
        {
        }
        template <class T1>
        STLPageHeapAllocator(const STLPageHeapAllocator<T1, static_token> &) {}

        //
        pointer address(reference x) const { return &x; }
        const_pointer address(const_reference x) const { return &x; }
        //
        size_type max_size() const { return size_t(-1) / sizeof(value_type); }
        //
        void construct(pointer p, const T &val) { ::new (p) T(val); }
        void construct(pointer p) { ::new (p) T(); }
        void destroy(pointer p) { p->~T(); }
        //
        bool operator==(const STLPageHeapAllocator &) const { return true; }
        bool operator!=(const STLPageHeapAllocator &) const { return false; }
        //
        pointer allocate(size_type n, const void * = 0)
        {
            assert(n == 1);
            if (!initialized)
            {
                allocator.Init();
                initialized = true;
            }
            return allocator.New();
        }
        void deallocate(pointer p, size_type n)
        {
            assert(n == 1);
            allocator.Delete(p);
        }
    };

    template <typename T, int static_token>
    PageHeapAllocator<T> STLPageHeapAllocator<T, static_token>::allocator; // For initialize the static allocator

    template <typename T, int static_token>
    bool STLPageHeapAllocator<T, static_token>::initialized=false;

}

#endif