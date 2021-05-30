#include <iostream>
#include <cassert>

#include "../tcmalloc/config.hpp"
#include "../tcmalloc/central_freelist.hpp"
#include "../tcmalloc/size_class.hpp"

struct TCEntry
{
    void *head;
    void *end;
};

void TestInitInsertAndRemove()
{
    tcmalloc::CentralFreeList central_free_lists_[tcmalloc::SmallObjectMaxClass];
    for (uint32_t cs = 1; cs < tcmalloc::SmallObjectMaxClass; cs++)
    {
        central_free_lists_[cs].Init(cs);
        // central_free_lists_[cs].CheckState();
    }
    for (uint32_t cs = 1; cs < tcmalloc::SmallObjectMaxClass; cs++)
    {
        // std::cout << cs << std::endl;
        tcmalloc::Length num_objects_to_move = tcmalloc::SizeMap::Instance()->num_objects_to_move(cs);
        const int test_batch = 100;
        TCEntry temp[test_batch];
        // std::cout << "Remove" << std::endl;
        for (int i = 0; i < test_batch; i++)
        {
            // std::cout << i << std::endl;
            central_free_lists_[cs].RemoveRange(&(temp[i].head), &(temp[i].end), num_objects_to_move);
            central_free_lists_[cs].CheckState();
        }
        // std::cout << "Insert" << std::endl;
        for (int i = 0; i < test_batch; i++)
        {
            central_free_lists_[cs].InsertRange(temp[i].head, temp[i].end, num_objects_to_move);
            central_free_lists_[cs].CheckState();
        }
        for (int i = 0; i < test_batch; i++)
        {
            // std::cout << i << std::endl;
            central_free_lists_[cs].RemoveRange(&(temp[i].head), &(temp[i].end), num_objects_to_move);
            central_free_lists_[cs].CheckState();
        }
        for (int i = 0; i < test_batch; i++)
        {
            central_free_lists_[cs].InsertRange(temp[i].head, temp[i].end, num_objects_to_move);
            central_free_lists_[cs].CheckState();
        }
    }
}

int main(int argc, char const *argv[])
{
    TestInitInsertAndRemove();
    std::cout << "PASS" << std::endl;
    return 0;
}
