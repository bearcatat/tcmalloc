#include <iostream>
#include <cassert>
#include <vector>

#include "../tcmalloc/config.hpp"
#include "../tcmalloc/page_map.hpp"

using std::vector;

static void Permute(vector<intptr_t> *elements)
{
    if (elements->empty())
        return;
    const size_t num_elements = elements->size();
    for (size_t i = num_elements - 1; i > 0; --i)
    {
        const size_t newpos = rand() % (i + 1);
        const intptr_t tmp = (*elements)[i]; // swap
        (*elements)[i] = (*elements)[newpos];
        (*elements)[newpos] = tmp;
    }
}

void TestMap(int limit)
{
    {
        tcmalloc::PageMap<tcmalloc::PageIDBIT> map;
        for (intptr_t i = 0; i < static_cast<intptr_t>(limit); i++)
        {
            map.Set(i, (void *)(i + 1));
            assert(map.Get(i) == (void *)(i + 1));
        }
        for (intptr_t i = 0; i < static_cast<intptr_t>(limit); i++)
        {
            assert(map.Get(i) == (void *)(i + 1));
        }
    }

    {
        srand(404);
        vector<intptr_t> elements;
        for (intptr_t i = 0; i < static_cast<intptr_t>(limit); i++)
            elements.push_back(i);
        Permute(&elements);
        tcmalloc::PageMap<tcmalloc::PageIDBIT> map;
        for (intptr_t i = 0; i < static_cast<intptr_t>(limit); i++)
        {
            map.Set(elements[i], (void *)(elements[i] + 1));
            assert(map.Get(elements[i]) == (void *)(elements[i] + 1));
        }
        for (intptr_t i = 0; i < static_cast<intptr_t>(limit); i++)
        {
            assert(map.Get(elements[i]) == (void *)(elements[i] + 1));
        }
    }
}

int main(int argc, char const *argv[])
{
    TestMap(1000);
    std::cout << "PASS" << std::endl;
    return 0;
}
