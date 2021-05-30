#if !defined(TCMALLOC_SPIN_LOCK)
#define TCMALLOC_SPIN_LOCK

#include <atomic>

namespace tcmalloc
{
    class SpinLock
    {
    private:
        std::atomic<bool> flag_;

    public:
        SpinLock() : flag_(false){};
        void lock()
        {
            bool expect = false;
            while (!flag_.compare_exchange_weak(expect, true))
            {
                expect = false;
            }
        }
        void unlock()
        {
            flag_.store(false);
        }
    };

    class SpinLockGuard
    {
    private:
        SpinLock *lock_;

    public:
        SpinLockGuard(SpinLock *lock) : lock_(lock)
        {
            lock_->lock();
        };
        ~SpinLockGuard()
        {
            lock_->unlock();
        };
    };
}

#endif // TCMALLOC_SPIN_LOCK
