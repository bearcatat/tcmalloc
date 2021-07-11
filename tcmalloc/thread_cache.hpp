#if !defined(TCMALLOC_THREAD_CACHE)
#define TCMALLOC_THREAD_CACHE

#include "config.hpp"
#include "central_freelist.hpp"
#include "threadcache_freelist.hpp"
#include "page_heap_allocator.hpp"
#include "size_class.hpp"
#include <pthread.h>
#include <mutex>
namespace tcmalloc
{
    class ThreadCache
    {
    private:
        // record the cache size from all of cache
        static size_t overall_thread_cache;
        static size_t per_thread_cache_size;
        static size_t unclaimed_cache_space;

        // about thread
        static __thread ThreadCache *thread_cache_;
        static pthread_key_t spec_key;
        static bool global_is_inited;
        static std::mutex global_lock;

        static ThreadCache cache_list;
        static int cache_size;
        static ThreadCache *next_cache_steal;
        static CentralFreeList central_free_lists_[SmallObjectMaxClass];
        static PageHeapAllocator<ThreadCache> cache_allocator;

        static void CacheListInit(ThreadCache *cache)
        {
            cache->prev = cache;
            cache->next = cache;
        }

        static bool CacheListEmpty(ThreadCache *cache)
        {
            return cache->next == cache;
        }

        static void CacheListRemove(ThreadCache *cache)
        {
            cache_size--;
            cache->prev->next = cache->next;
            cache->next->prev = cache->prev;
            cache->prev = NULL;
            cache->next = NULL;
        }

        static void CacheListInsert(ThreadCache *before, ThreadCache *cache)
        {
            cache_size++;
            cache->next = before->next;
            before->next->prev = cache;
            cache->prev = before;
            before->next = cache;
        }

    public:
        ThreadCache *next;
        ThreadCache *prev;
        ThreadCacheFreeList free_list_[SmallObjectMaxClass];
        size_t max_size;
        size_t free_size;
        size_t alloc_size;
        // must lock
        void IncreaseCacheLimitLocked()
        {
            if (unclaimed_cache_space > ThreadCacheStealSize)
            {
                max_size += ThreadCacheStealSize;
                return;
            }
            int k = ThreadCacheStealNum;
            while (k-- && next_cache_steal != thread_cache_ && next_cache_steal != &cache_list)
            {
                if (next_cache_steal->max_size < ThreadCacheStealSize)
                {
                    continue;
                }
                max_size += ThreadCacheStealSize;
                next_cache_steal -= ThreadCacheStealSize;
                return;
            }
        }

        void FetchFromCentralCache(ThreadCacheFreeList &fl, CentralFreeList &cfl, uint32_t sc)
        {
            Length batch_size = SizeMap::Instance()->num_objects_to_move(sc);
            batch_size = batch_size > fl.max_length() ? fl.max_length() : batch_size;
            void *start, *end;
            int fetched_num = cfl.RemoveRange(&start, &end, batch_size);
            fl.PushRange(fetched_num, start, end);
            free_size += fetched_num * fl.object_size();

            if (fl.max_length() < batch_size)
            {
                fl.set_max_length(fl.max_length() + 1);
            }
            else
            {
                int new_max_length = fl.max_length() + batch_size;
                if (new_max_length > ThreadCacheFreeListMaxLength)
                {
                    new_max_length = ThreadCacheFreeListMaxLength;
                }
                new_max_length -= new_max_length % batch_size;
                fl.set_max_length(new_max_length);
            }
        }

        void *New(uint32_t sc)
        {
            assert(sc > 0 && sc < SmallObjectMaxClass);
            ThreadCacheFreeList &fl = free_list_[sc];
            void *object;
            // std::cout << fl.length() << " " << fl.max_length() << std::endl;
            if (!fl.TryPop(&object))
            {
                // std::cout << "False" << std::endl;
                FetchFromCentralCache(fl, central_free_lists_[sc], sc);
                fl.TryPop(&object);
            }
            free_size -= fl.object_size();
            alloc_size += fl.object_size();
            return object;
        }

        void Scavenge()
        {
            for (int sc = 0; sc < SmallObjectMaxClass; sc++)
            {
                ThreadCacheFreeList &fl = free_list_[sc];
                int batch_size = SizeMap::Instance()->num_objects_to_move(sc);
                if (fl.lowatermark() > 0)
                {
                    int to_release = fl.lowatermark() / 2;
                    if (to_release == 0)
                        to_release = 1;
                    ReleaseToCentralList(free_list_[sc], central_free_lists_[sc], sc, to_release);
                    int new_max_length = fl.max_length() - batch_size;
                    if (new_max_length < batch_size)
                    {
                        new_max_length = batch_size;
                    }
                    fl.set_max_length(new_max_length);
                }
                fl.clear_lowatermark();
            }
            global_lock.lock();
            IncreaseCacheLimitLocked();
            global_lock.unlock();
        }

        void ListToLong(ThreadCacheFreeList &fl, uint32_t sc)
        {
            int batch_size = SizeMap::Instance()->num_objects_to_move(sc);
            ReleaseToCentralList(fl, central_free_lists_[sc], sc, batch_size);

            if (fl.max_length() < batch_size)
            {
                fl.set_max_length(fl.max_length() + 1);
            }
            else
            {
                fl.set_length_overages(fl.length_overages() + 1);
                if (fl.length_overages() > ThreadCacheFreeListMaxOverage)
                {
                    fl.set_max_length(fl.max_length() - batch_size);
                }
                fl.set_length_overages(0);
            }
            if (free_size > max_size)
            {
                Scavenge();
            }
        }

        void Delete(void *ptr, uint32_t sc)
        {
            assert(sc > 0 && sc < SmallObjectMaxClass);
            ThreadCacheFreeList &fl = free_list_[sc];
            int length = fl.Push(ptr);
            free_size += fl.object_size();
            alloc_size -= fl.object_size();

            if (length > fl.max_length())
            {
                ListToLong(fl, sc);
                return;
            }
            if (free_size > max_size)
            {
                Scavenge();
            }
        }

        // need lock
        void Init()
        {
            // std::cout << "Initing Thread Cache" << std::endl;
            alloc_size = 0;
            free_size = 0;
            max_size = 0;
            IncreaseCacheLimitLocked();
            if (max_size == 0)
            {
                max_size = ThreadCacheMinSize;
                unclaimed_cache_space -= ThreadCacheMinSize;
                assert(unclaimed_cache_space < 0);
            }
            next_cache_steal = thread_cache_;
            for (uint32_t cs = 0; cs < SmallObjectMaxClass; cs++)
            {
                free_list_[cs].Init(SizeMap::Instance()->class_to_size(cs));
            }
        }
        // need lock
        void ReleaseToCentralList(ThreadCacheFreeList &fl, CentralFreeList &cfl, uint32_t sc, int N)
        {
            assert(N > 0);
            int batch_size = SizeMap::Instance()->num_objects_to_move(sc);
            if (fl.length() < batch_size)
            {
                batch_size = fl.length();
            }
            free_size -= N * fl.object_size();
            void *start, *end;
            while (N > batch_size)
            {
                fl.PopRange(batch_size, &start, &end);
                cfl.InsertRange(start, end, batch_size);
                N -= batch_size;
            }
            fl.PopRange(N, &start, &end);
            cfl.InsertRange(start, end, N);
        }
        void clear()
        {
            for (uint32_t sc = 0; sc < SmallObjectMaxClass; sc++)
            {
                if (free_list_[sc].length() > 0)
                {
                    ReleaseToCentralList(free_list_[sc], central_free_lists_[sc], sc, free_list_[sc].length());
                }
            }
        }
        static void DeleteCache(ThreadCache *cache)
        {
            cache->clear();
            thread_cache_ = NULL;
            global_lock.lock();
            if (next_cache_steal == cache)
            {
                next_cache_steal = cache->next;
            }
            CacheListRemove(cache);
            unclaimed_cache_space += cache->max_size;
            cache_allocator.Delete(cache);
            global_lock.unlock();
        }
        static void DestroyThreadCache(void *ptr)
        {
            // std::cout << "Destroying Thread Cache" << std::endl;
            ThreadCache *cache = reinterpret_cast<ThreadCache *>(ptr);
            DeleteCache(cache);
        }
        static void GlobalInit()
        {
            // std::cout << "Initing Global Env" << std::endl;
            assert(!global_is_inited);
            for (uint32_t cs = 1; cs < SmallObjectMaxClass; cs++)
            {
                central_free_lists_[cs].Init(cs);
            }
            // register pthread spec data
            pthread_key_create(&spec_key, DestroyThreadCache);
            CacheListInit(&cache_list);
            next_cache_steal = &cache_list;
            global_is_inited = true;
        }
        static ThreadCache *Current()
        {
            if (thread_cache_)
                return thread_cache_;
            global_lock.lock();
            if (!global_is_inited)
            {
                GlobalInit();
            }
            assert(global_is_inited);
            ThreadCache *cache = cache_allocator.New();
            cache->Init();
            thread_cache_ = cache;
            global_lock.unlock();
            pthread_setspecific(spec_key, cache);
            global_lock.lock();
            CacheListInsert(&cache_list, cache);
            global_lock.unlock();
            return thread_cache_;
        }
    };
    __thread ThreadCache *ThreadCache::thread_cache_ = NULL;
    pthread_key_t ThreadCache::spec_key;
    bool ThreadCache::global_is_inited = false;
    std::mutex ThreadCache::global_lock;
    ThreadCache ThreadCache::cache_list;
    int ThreadCache::cache_size = 0;
    ThreadCache *ThreadCache::next_cache_steal;
    CentralFreeList ThreadCache::central_free_lists_[SmallObjectMaxClass];
    PageHeapAllocator<ThreadCache> ThreadCache::cache_allocator;
    size_t ThreadCache::overall_thread_cache = ThreadCacheDefaultOverallSize;
    size_t ThreadCache::per_thread_cache_size = ThreadCacheMaxSize;
    size_t ThreadCache::unclaimed_cache_space = ThreadCacheDefaultOverallSize;

}
#endif // TCMALLOC_THREAD_CACHE
