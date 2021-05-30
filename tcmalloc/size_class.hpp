#if !defined(TCMALLOC_SIZE_CLASS)
#define TCMALLOC_SIZE_CLASS

#include <cassert>
#include "config.hpp"

namespace tcmalloc
{
    class SizeMap
    {
    private:
        static size_t SmallSizeClass(size_t s)
        {
            return (static_cast<uint32_t>(s) + 7) >> 3;
        }
        static size_t LargeSizeClass(size_t s)
        {
            return (static_cast<uint32_t>(s) + 127 + (120 << 7)) >> 7;
        }
        static bool ClassIndexMaybe(size_t s, uint32_t *idx)
        {
            if (s <= SmallObjectSmallSize)
            {
                *idx = (static_cast<uint32_t>(s) + 7) >> 3;
                return true;
            }
            else if (s <= SmallObjectMaxSize)
            {
                *idx = (static_cast<uint32_t>(s) + 127 + (120 << 7)) >> 7;
                return true;
            }
            return false;
        }
        static uint32_t ClassIndex(size_t s)
        {
            assert(s >= 0);
            assert(s <= SmallObjectMaxSize);
            if (s <= SmallObjectSmallSize)
                return (static_cast<uint32_t>(s) + 7) >> 3;
            else
                return (static_cast<uint32_t>(s) + 127 + (120 << 7)) >> 7;
        }

    public:
        int SizeClass(size_t size)
        {
            return ClassIndex(size);
        }
        bool GetSizeClass(size_t size, uint32_t *sc)
        {
            uint32_t idx;
            if (!ClassIndexMaybe(size, &idx))
            {
                return false;
            }
            *sc = IndexToClass[idx];
            return true;
        }
        size_t ByteSizeForClass(uint32_t idx)
        {
            return SizeClasses[idx].size;
        }
        size_t class_to_size(uint32_t idx)
        {
            return SizeClasses[idx].size;
        }
        Length class_to_pages(uint32_t idx)
        {
            return SizeClasses[idx].pages;
        }
        Length num_objects_to_move(uint32_t idx)
        {
            return SizeClasses[idx].num_to_move;
        }
        static SizeMap *Instance()
        {
            static SizeMap size_map_;
            return &size_map_;
        }
    };

}

#endif // TCMALLOC_SIZE_CLASS
