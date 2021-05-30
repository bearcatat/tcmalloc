#if !defined(TCMALLOC_PAGEMAP)
#define TCMALLOC_PAGEMAP

#include "config.hpp"
#include "page_heap_allocator.hpp"
#include <cstring>

namespace tcmalloc
{
    template <int BITS>
    class PageMap
    {
    private:
        static const int INTERIOR_BITS = (BITS + 2) / 3;
        static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS;
        static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS;
        static const int LEAF_LENGTH = 1 << LEAF_BITS;
        struct Node
        {
            void **ptr[INTERIOR_LENGTH];
        };

        struct Leaf
        {
            void *left[INTERIOR_LENGTH];
        };

        void ***root[INTERIOR_LENGTH];
        PageHeapAllocator<Node> node_allocator;
        PageHeapAllocator<Leaf> left_allocator;

    public:
        PageMap()
        {
            memset(root, 0, sizeof(root));
        }
        bool Set(PageID pageid, void *value)
        {
            int k1 = pageid >> (INTERIOR_BITS + LEAF_BITS);
            int k2 = (pageid >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
            int k3 = pageid & (LEAF_LENGTH - 1);
            if (root[k1] == NULL)
            {
                root[k1] = reinterpret_cast<void ***>(node_allocator.New());
                if (root[k1] != NULL)
                {
                    memset(root[k1], 0, sizeof(Node));
                }
                else
                {
                    return false;
                }
            }
            if (root[k1][k2] == NULL)
            {
                root[k1][k2] = reinterpret_cast<void **>(left_allocator.New());
                if (root[k1][k2] != NULL)
                {
                    memset(root[k1][k2], 0, sizeof(Leaf));
                }
                else
                {
                    return false;
                }
            }
            root[k1][k2][k3] = value;
            return true;
        }
        void *Get(PageID pageid)
        {
            int k1 = pageid >> (INTERIOR_BITS + LEAF_BITS);
            int k2 = (pageid >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
            int k3 = pageid & (LEAF_LENGTH - 1);
            if (root[k1] == NULL || root[k1][k2] == NULL)
                return NULL;
            return root[k1][k2][k3];
        }
        void *GetMayFail(PageID pageid)
        {
            int k1 = pageid >> (INTERIOR_BITS + LEAF_BITS);
            int k2 = (pageid >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
            int k3 = pageid & (LEAF_LENGTH - 1);
            return root[k1][k2][k3];
        }
    };
} // namespace std

#endif // TCMALLOC_PAGEMAP
