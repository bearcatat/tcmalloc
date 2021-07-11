#if !defined(TCMALLOC_PAGE_HEAP)
#define TCMALLOC_PAGE_HEAP

#include <mutex>

#include "config.hpp"
#include "span.hpp"
#include "page_map.hpp"
#include <iostream>

namespace tcmalloc
{
  class PageHeap
  {

  public:
    struct State
    {
      size_t system_bytes;
      size_t in_used_bytes;
      size_t normal_bytes;
      size_t returned_bytes;

      size_t small_normal_bytes;
      size_t small_returned_bytes;
      size_t large_normal_bytes;
      size_t large_returned_bytes;
    };

    PageHeap()
    {
      // std::cout << "Initing PageHeap" << std::endl;
      for (int i = 0; i < kMaxPages; i++)
      {
        free_size_[i].normal = 0;
        free_size_[i].returned = 0;
        DLLInit(&(free_[i].normal));
        DLLInit(&(free_[i].returned));
      }
      stat_.system_bytes = 0;
      stat_.in_used_bytes = 0;
      stat_.normal_bytes = 0;
      stat_.returned_bytes = 0;
      stat_.small_normal_bytes = 0;
      stat_.small_returned_bytes = 0;
      stat_.large_normal_bytes = 0;
      stat_.large_returned_bytes = 0;
      release_index_ = kMaxPages;
      release_rate_ = 100;
    }

    Span *New(Length n)
    {
      assert(n > 0);
      std::lock_guard<std::mutex> guaid(lock);
      Span *span = NULL;
      span = SearchSmallAndLarge(n);
      if (span != NULL)
      {
        // std::cout<<"Found"<<std::endl;
        return Carve(span, n);
      }
      if (stat_.normal_bytes > 0 && stat_.returned_bytes > 0 && stat_.normal_bytes + stat_.returned_bytes > stat_.system_bytes / 4)
      {
        // std::cout<<"Release and Finding"<<std::endl;
        ReleaseAtLeastSpan(0x7fffffff);
        span = SearchSmallAndLarge(n);
        if (span != NULL)
        {
          return Carve(span, n);
        }
      }
      if (GrowHeap(n))
      {
        // std::cout<<"New and Found"<<std::endl;
        assert(stat_.in_used_bytes + stat_.normal_bytes + stat_.returned_bytes == stat_.system_bytes);
        span = SearchSmallAndLarge(n);
        if (span != NULL)
        {
          return Carve(span, n);
        }
      }
      // std::cout << stat_.system_bytes << " "
      //           << stat_.in_used_bytes << " "
      //           << stat_.normal_bytes << " "
      //           << stat_.returned_bytes << std::endl;
      return span;
    }

    void Delete(Span *span)
    {
      std::lock_guard<std::mutex> guard(lock);
      span->location = Span::IN_NORMAL;
      Length old_pages = span->length;
      stat_.in_used_bytes -= (span->length << PageShitf);
      MergeIntoFreeList(span);
      IncrementalScavenge(old_pages);
    }

    void RegisterSizeClass(Span *span, uint32_t sc)
    {
      assert(span->location == Span::IN_USE);
      assert(PageIDToSpan(span->start) == span);
      assert(PageIDToSpan(span->start + span->length - 1) == span);
      span->sizeclass = sc;
      for (PageID i = span->start + 1; i < span->start + span->length - 1; i++)
      {
        page_map_.Set(i, span);
      }
    }

    Span *PageIDToSpan(PageID p)
    {
      return reinterpret_cast<Span *>(page_map_.GetMayFail(p));
    }

    bool CheckExpensive()
    {
      // std::cout << stat_.system_bytes << " "
      //           << stat_.in_used_bytes << " "
      //           << stat_.normal_bytes << " "
      //           << stat_.returned_bytes << std::endl;
      // std::cout << large_normal_.size() << " " << large_returned_.size() << std::endl;
      std::lock_guard<std::mutex> guard(lock);
      assert(release_index_ <= kMaxPages);
      assert(release_rate_ >= 0);
      assert(stat_.system_bytes == stat_.in_used_bytes + stat_.normal_bytes + stat_.returned_bytes);

      size_t small_normal_bytes = 0;
      size_t small_returned_bytes = 0;
      size_t large_normal_bytes = 0;
      size_t large_returned_bytes = 0;

      size_t normal_bytes = 0;
      size_t returned_bytes = 0;

      for (int i = 0; i < kMaxPages; i++)
      {
        int count = free_size_[i].normal;
        for (Span *span = free_[i].normal.next; span != &(free_[i].normal); span = span->next)
        {
          count--;
          assert(count >= 0);
          assert(span->location == Span::IN_NORMAL);
          assert(span->length == i + 1);
          assert(page_map_.Get(span->start) == span);
          assert(page_map_.Get(span->start + span->length - 1) == span);
          small_normal_bytes += (span->length << PageShitf);
          normal_bytes += (span->length << PageShitf);
        }
        count = free_size_[i].returned;
        for (Span *span = free_[i].returned.next; span != &(free_[i].returned); span = span->next)
        {
          count--;
          assert(count >= 0);
          assert(span->location == Span::IN_RETURNED);
          assert(span->length == i + 1);
          assert(page_map_.Get(span->start) == span);
          assert(page_map_.Get(span->start + span->length - 1) == span);
          small_returned_bytes += span->length << PageShitf;
          returned_bytes += span->length << PageShitf;
        }
      }
      for (auto it = large_normal_.begin(); it != large_normal_.end(); it++)
      {
        Span *span = *it;
        assert(span->location == Span::IN_NORMAL);
        assert(page_map_.Get(span->start) == span);
        assert(page_map_.Get(span->start + span->length - 1) == span);
        large_normal_bytes += span->length << PageShitf;
        normal_bytes += span->length << PageShitf;
      }
      for (auto it = large_returned_.begin(); it != large_returned_.end(); it++)
      {
        Span *span = *it;
        assert(span->location == Span::IN_RETURNED);
        assert(page_map_.Get(span->start) == span);
        assert(page_map_.Get(span->start + span->length - 1) == span);
        large_returned_bytes += span->length << PageShitf;
        returned_bytes += span->length << PageShitf;
      }
      // std::cout << normal_bytes << " "
      //           << returned_bytes << std::endl;
      assert(stat_.normal_bytes == normal_bytes);
      assert(stat_.returned_bytes == returned_bytes);
      assert(stat_.small_normal_bytes == small_normal_bytes);
      assert(stat_.small_returned_bytes == small_returned_bytes);
      assert(stat_.large_normal_bytes == large_normal_bytes);
      assert(stat_.large_returned_bytes == large_returned_bytes);
      return true;
    }
    Length ReleaseAtLeastSpan(Length n)
    {
      Length release_pages = 0;
      while (release_pages <= n && stat_.normal_bytes > 0)
      {
        for (int i = 0; i <= kMaxPages; i++, release_index_++)
        {
          Span *span = NULL;
          if (release_index_ > kMaxPages)
            release_index_ = 0;
          if (release_index_ == kMaxPages)
          {
            if (large_normal_.empty())
              continue;
            span = *(large_normal_.begin());
          }
          else
          {
            if (DLLEmpty(&(free_[release_index_].normal)))
              continue;
            span = free_[release_index_].normal.next;
          }
          Length release_length = ReleaseSpan(span);
          if (release_length == 0)
            return 0; // To avoid dead loop;
          release_pages += release_length;
        }
      }
      return release_pages;
    }

    PageHeap::State state() const
    {
      return stat_;
    }

    static PageHeap *Instance()
    {
      static PageHeap page_heap_;
      return &page_heap_;
    }

  private:
    void PrependToFreeList(Span *span)
    {
      assert(span->location != Span::IN_USE);
      size_t span_size = (span->length << PageShitf);
      if (span->location == Span::IN_NORMAL)
        stat_.normal_bytes += span_size;
      else
        stat_.returned_bytes += span_size;
      if (span->length > kMaxPages)
      {
        if (span->location == Span::IN_NORMAL)
        {
          auto p = large_normal_.insert(span);
          assert(p.second);
          stat_.large_normal_bytes += span_size;
        }
        else
        {
          auto p = large_returned_.insert(span);
          assert(p.second);
          stat_.large_returned_bytes += span_size;
        }
        return;
      }
      if (span->location == Span::IN_NORMAL)
      {
        DLLInsert(&(free_[span->length - 1].normal), span);
        stat_.small_normal_bytes += span_size;
        free_size_[span->length - 1].normal++;
      }
      else
      {
        DLLInsert(&(free_[span->length - 1].returned), span);
        stat_.small_returned_bytes += span_size;
        free_size_[span->length - 1].returned++;
      }
    }

    void RemoveFromFreeList(Span *span)
    {
      assert(span->location != Span::IN_USE);
      size_t span_size = (span->length << PageShitf);
      if (span->location == Span::IN_NORMAL)
        stat_.normal_bytes -= span_size;
      else
        stat_.returned_bytes -= span_size;
      if (span->length > kMaxPages)
      {
        SpanSet *set;
        if (span->location == Span::IN_NORMAL)
        {
          large_normal_.erase(span);
          stat_.large_normal_bytes -= span_size;
        }
        else
        {
          large_returned_.erase(span);
          stat_.large_returned_bytes -= span_size;
        }
      }
      else
      {
        if (span->location == Span::IN_NORMAL)
        {
          stat_.small_normal_bytes -= span_size;
          free_size_[span->length - 1].normal--;
        }
        else
        {
          stat_.small_returned_bytes -= span_size;
          free_size_[span->length - 1].returned--;
        }
        DLLRemove(span);
      }
    }

    Span *CheckAndHandlePreMerge(Span *span, Span *other)
    {
      if (other == NULL)
        return NULL;
      if (span->location != other->location)
        return NULL;
      RemoveFromFreeList(other);
      return other;
    }

    void MergeIntoFreeList(Span *span)
    {
      assert(span->location != Span::IN_USE);
      PageID p = span->start;
      Length n = span->length;
      Span *prev = CheckAndHandlePreMerge(span, PageIDToSpan(p - 1));
      if (prev != NULL)
      {
        assert(prev->start + prev->length == p);
        page_map_.Set(prev->start, span);
        span->length += prev->length;
        span->start = prev->start;
        DeleteSpan(prev);
      }
      Span *next = CheckAndHandlePreMerge(span, PageIDToSpan(p + n));
      if (next != NULL)
      {
        assert(next->start = p + n);
        page_map_.Set(next->start + next->length - 1, span);
        span->length += next->length;
        DeleteSpan(next);
      }
      PrependToFreeList(span);
    }

    void RecordNewSpan(Span *span)
    {
      page_map_.Set(span->start, span);
      page_map_.Set(span->start + span->length - 1, span);
    }

    Span *Carve(Span *span, Length n)
    {
      assert(n > 0);
      assert(span->length >= n);
      assert(span->location != Span::IN_USE);
      Length left_page_num = span->length - n;
      RemoveFromFreeList(span);
      if (left_page_num > 0)
      {
        Span *new_span = NewSpan(span->start + n, left_page_num);
        new_span->location = span->location;
        RecordNewSpan(new_span);
        PrependToFreeList(new_span);
        span->length = n;
        page_map_.Set(span->start + n - 1, span);
      }
      span->location = Span::IN_USE;
      stat_.in_used_bytes += (span->length << PageShitf);
      return span;
    }

    Span *SearchLarge(Length n)
    {
      assert(n > 0);
      Span bound;
      bound.length = n;
      bound.start = 0;
      Span *best = NULL;

      auto normal_it = large_normal_.upper_bound(&bound);
      if (normal_it != large_normal_.end())
        best = *normal_it;

      auto returned_it = large_returned_.upper_bound(&bound);
      if (returned_it != large_returned_.end())
      {
        if (best == NULL || best->length > (*returned_it)->length)
        {
          best = *returned_it;
        }
      }
      return best;
    }

    Span *SearchSmallAndLarge(Length n)
    {
      assert(n > 0);
      for (Length i = n; i <= kMaxPages; i++)
      {
        Span *ll = &(free_[i - 1].normal);
        if (!DLLEmpty(ll))
        {
          assert(ll->next->location == Span::IN_NORMAL);
          return ll->next;
        }
        ll = &(free_[i - 1].returned);
        if (!DLLEmpty(ll))
        {
          assert(ll->next->location == Span::IN_RETURNED);
          return ll->next;
        }
      }
      return SearchLarge(n);
    }

    Length ReleaseSpan(Span *span)
    {
      assert(span->location == Span::IN_NORMAL);
      const Length n = span->length;
      if (!TCMalloc_SystemRelease(reinterpret_cast<void *>(span->start << PageShitf), reinterpret_cast<size_t>(span->length << PageShitf)))
      {
        std::cout << "Release Failed" << std::endl;
        return 0;
      }
      // std::cout << "Release Span To sys" << std::endl;

      RemoveFromFreeList(span);
      span->location = Span::IN_RETURNED;
      MergeIntoFreeList(span);
      return n;
    }

    bool GrowHeap(Length n)
    {
      n = n > KMinAllocPage ? n : KMinAllocPage;
      size_t size = reinterpret_cast<size_t>(n << PageShitf);
      size_t alloc_size = ((size + PageSize - 1) / PageSize) * PageSize;
      void *ptr = TCMalloc_SystemAlloc(alloc_size);
      if (ptr == NULL)
        return false;
      Span *span = NewSpan(reinterpret_cast<Length>(ptr) >> PageShitf, alloc_size >> PageShitf);
      span->location = Span::IN_NORMAL;
      stat_.system_bytes += size;
      RecordNewSpan(span);
      PrependToFreeList(span);
      return true;
    }

    void IncrementalScavenge(Length n)
    {
      release_rate_ -= n;
      // std::cout << release_rate_ << std::endl;
      if (release_rate_ <= 0)
      {
        Length release_pages = ReleaseAtLeastSpan(1);
        if (release_pages >= 1)
        {
          release_rate_ = release_pages * ReleaseRate;
        }
        else
        {
          release_rate_ = ReleaseRate;
        }
      }
    }

  private:
    struct SpanList
    {
      Span normal;
      Span returned;
    };
    struct SmallSpanState
    {
      uint64_t normal;
      uint64_t returned;
    };

    SpanList free_[kMaxPages];
    SmallSpanState free_size_[kMaxPages];
    SpanSet large_normal_;
    SpanSet large_returned_;
    State stat_;
    uint16_t release_index_;
    int release_rate_;
    PageMap<PageIDBIT> page_map_;
    std::mutex lock;
  };

}

#endif // TCMALLOC_PAGE_HEAP
