#include <stdio.h>
#include <memory>
#include "../tcmalloc/config.hpp"
#include "../tcmalloc/page_heap.hpp"
#include "../tcmalloc/system_alloc.hpp"
#include "../tcmalloc/span.hpp"

namespace
{
    static void CheckStats(const tcmalloc::PageHeap *ph,
                           uint64_t system_pages,
                           uint64_t free_pages,
                           uint64_t unmapped_pages)
    {
        const tcmalloc::PageHeap::State stat = ph->state();
        assert(stat.system_bytes >> tcmalloc::PageShitf == system_pages);
        assert(stat.normal_bytes >> tcmalloc::PageShitf == free_pages);
        assert(stat.returned_bytes >> tcmalloc::PageShitf == unmapped_pages);
    }
}

static void TestPageHeap_Stats()
{
    tcmalloc::PageHeap *ph = tcmalloc::PageHeap::Instance();
    ph->CheckExpensive();
    CheckStats(ph, 0, 0, 0);

    printf("Allocate a span s1\n");
    tcmalloc::Span *s1 = ph->New(256);
    ph->CheckExpensive();
    CheckStats(ph, 256, 0, 0);

    //Delete span1
    printf("Delete span1\n");
    ph->Delete(s1);
    ph->CheckExpensive();
    CheckStats(ph, 256, 256, 0);

    //Carve span2;
    printf("Carve span2;\n");
    s1 = ph->New(129);
    ph->CheckExpensive();
    CheckStats(ph, 256, 127, 0);

    // release one span
    printf("release one span\n");
    ph->ReleaseAtLeastSpan(1);
    ph->CheckExpensive();
    CheckStats(ph, 256, 0, 127);

    //delete a span
    printf("Delete span1\n");
    ph->Delete(s1);
    ph->CheckExpensive();
    CheckStats(ph, 256, 129, 127);

    // release one span
    printf("release one span\n");
    ph->ReleaseAtLeastSpan(1);
    ph->CheckExpensive();
    CheckStats(ph, 256, 0, 256);

    //Carve span2;
    printf("new span2;\n");
    s1 = ph->New(521);
    ph->CheckExpensive();
    CheckStats(ph, 256 + 521, 0, 256);

    //delete a span
    printf("Delete span1\n");
    ph->Delete(s1);
    ph->CheckExpensive();
    CheckStats(ph, 256 + 521, 521, 256);

    printf("new span:127;\n");
    s1 = ph->New(512);
    ph->CheckExpensive();
    CheckStats(ph, 256 + 521, 521 - 512, 256);

    //delete a span
    printf("Delete span1\n");
    ph->Delete(s1);
    ph->CheckExpensive();
    CheckStats(ph, 256 + 521, 521, 256);
}

static constexpr int kNumberMaxPagesSpans = 10;

// ptr 0x7ffff7ee9000
// start 17179852660
static void AllocateAllPageTables()
{
    tcmalloc::PageHeap *ph = tcmalloc::PageHeap::Instance();
    tcmalloc::Span *span[kNumberMaxPagesSpans * 128];
    for (int i = 0; i < kNumberMaxPagesSpans * 2; ++i)
    {
        span[i] = ph->New(tcmalloc::kMaxPages);
        for (int j = 0; j < (tcmalloc::kMaxPages << tcmalloc::PageShitf) / 4; j++)
        {
            char *start = reinterpret_cast<char *>(span[i]->start << tcmalloc::PageShitf);

            reinterpret_cast<int *>(start)[j] = 199;
        }
        assert(span[i] != NULL);
    }
    for (int i = 0; i < kNumberMaxPagesSpans * 2; ++i)
    {
        ph->Delete(span[i]);
    }
    for (int i = 0; i < kNumberMaxPagesSpans * 2; ++i)
    {
        span[i] = ph->New(tcmalloc::kMaxPages >> 1);
        assert(span[i] != NULL);
    }
    for (int i = 0; i < kNumberMaxPagesSpans * 2; ++i)
    {
        ph->Delete(span[i]);
    }
    for (int i = 0; i < kNumberMaxPagesSpans * 128; ++i)
    {
        span[i] = ph->New(i % 128 + 1);
    }
    for (int i = 0; i < kNumberMaxPagesSpans * 128; ++i)
    {
        ph->Delete(span[i]);
    }
}

static void TestPageHeap_Limit()
{
    std::unique_ptr<tcmalloc::PageHeap> ph(new tcmalloc::PageHeap());
}

int main(int argc, char const *argv[])
{
    /* code */
    // TestPageHeap_Stats();
    AllocateAllPageTables();
    printf("PASS\n");
    return 0;
}
