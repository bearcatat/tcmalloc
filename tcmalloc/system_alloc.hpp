#ifndef TCMALLOC_SYSTEM_ALLOC_HPP
#define TCMALLOC_SYSTEM_ALLOC_HPP

#include <sys/mman.h> // for mmap
#include <unistd.h>   // for sbrk
#include "config.hpp"

namespace tcmalloc
{
    // Mmap System Allocation
    void *TCMalloc_MmapSystemAlloc(size_t size)
    {
        size_t extra = 4 * 1024;
        void *res = mmap(nullptr, size + extra,
                         PROT_READ | PROT_WRITE, // read and write
                         MAP_PRIVATE | MAP_ANONYMOUS,
                         -1, 0);
        if (res == MAP_FAILED)
            return NULL;

        uintptr_t ptr = reinterpret_cast<uintptr_t>(res);
        uintptr_t start = ((ptr + PageSize - 1) / PageSize) * PageSize;
        size_t offset = reinterpret_cast<size_t>(start - ptr);
        if (offset > 0)
        {
            munmap(reinterpret_cast<void *>(ptr), offset);
        }
        if (offset < extra)
        {
            munmap(reinterpret_cast<void *>(ptr + offset + size), extra - offset);
        }

        return reinterpret_cast<void *>(start);
    }
    bool TCMalloc_MmapSystemRelease(void *ptr, size_t size)
    {
        return madvise(ptr, size, MADV_FREE) != -1;
    }
    //TODO SbrkSystemAlloc and Release
    //System Allocation
    void *TCMalloc_SystemAlloc(size_t size)
    {
        return TCMalloc_MmapSystemAlloc(size);
    }
    bool TCMalloc_SystemRelease(void *ptr, size_t size)
    {
        return TCMalloc_MmapSystemRelease(ptr, size);
    }
}

#endif
