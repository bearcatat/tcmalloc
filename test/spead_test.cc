#define NDEBUG

#include "../tcmalloc/tcmalloc.hpp"
#include <cassert>
#include <iostream>
#include <pthread.h>
#include "../tcmalloc/page_heap.hpp"
#include "../tcmalloc/span.hpp"
#include "../tcmalloc/size_class.hpp"
#include "sys/time.h"
#include "random"
#include <chrono>

using std::cout;
using std::endl;
using std::ostream;

struct ThreadArg
{
    uint32_t *size;
    uint32_t *n_size;
    int length;
};

struct TimeRes
{
    double malloc_time = 0;
    double free_time = 0;
    friend ostream &operator<<(ostream &os, const TimeRes &dt);

    TimeRes &operator+=(const TimeRes *t)
    {
        malloc_time += t->malloc_time;
        free_time += t->free_time;
        return *this;
    }
};

ostream &operator<<(ostream &os, const TimeRes &dt)
{
    os << "malloc time is " << dt.malloc_time << endl
       << "free time is " << dt.free_time << endl
       << "total time is " << dt.malloc_time + dt.free_time << endl;
    return os;
}

std::default_random_engine e;

void *TestTcMallocTime(void *arg)
{
    ThreadArg *t_arg = reinterpret_cast<ThreadArg *>(arg);
    TimeRes *time_res = new TimeRes();
    for (int i = 0; i < t_arg->length; i++)
    {
        auto beg_t = std::chrono::system_clock::now();
        unsigned **array = (unsigned **)(tcmalloc::tcmalloc(sizeof(unsigned *) * t_arg->n_size[i]));
        auto end_t = std::chrono::system_clock::now();
        std::chrono::duration<double> diff = end_t - beg_t;
        time_res->malloc_time += diff.count();
        for (int j = 0; j < t_arg->n_size[i]; j++)
        {
            beg_t = std::chrono::system_clock::now();
            array[j] = (unsigned *)(tcmalloc::tcmalloc(t_arg->size[i] * sizeof(unsigned)));
            end_t = std::chrono::system_clock::now();
            diff = end_t - beg_t;
            time_res->malloc_time += diff.count();
        }
        for (int j = 0; j < t_arg->n_size[i]; j++)
        {
            beg_t = std::chrono::system_clock::now();
            tcmalloc::tcfree(array[j]);
            end_t = std::chrono::system_clock::now();
            diff = end_t - beg_t;
            time_res->free_time += diff.count();
        }
        beg_t = std::chrono::system_clock::now();
        tcmalloc::tcfree(array);
        end_t = std::chrono::system_clock::now();
        diff = end_t - beg_t;
        time_res->free_time += diff.count();
    }
    return time_res;
}

void *TestSysMallocTime(void *arg)
{
    ThreadArg *t_arg = reinterpret_cast<ThreadArg *>(arg);
    TimeRes *time_res = new TimeRes();
    for (int i = 0; i < t_arg->length; i++)
    {
        auto beg_t = std::chrono::system_clock::now();
        unsigned **array = (unsigned **)(malloc(sizeof(unsigned *) * t_arg->n_size[i]));
        auto end_t = std::chrono::system_clock::now();
        std::chrono::duration<double> diff = end_t - beg_t;
        time_res->malloc_time += diff.count();
        for (int j = 0; j < t_arg->n_size[i]; j++)
        {
            beg_t = std::chrono::system_clock::now();
            array[j] = (unsigned *)(malloc(t_arg->size[i] * sizeof(unsigned)));
            end_t = std::chrono::system_clock::now();
            diff = end_t - beg_t;
            time_res->malloc_time += diff.count();
        }
        for (int j = 0; j < t_arg->n_size[i]; j++)
        {
            beg_t = std::chrono::system_clock::now();
            free(array[j]);
            end_t = std::chrono::system_clock::now();
            diff = end_t - beg_t;
            time_res->free_time += diff.count();
        }
        beg_t = std::chrono::system_clock::now();
        free(array);
        end_t = std::chrono::system_clock::now();
        diff = end_t - beg_t;
        time_res->free_time += diff.count();
    }
    return time_res;
}

const int MallocLength = 10000;
const int ThreadSize = 40;

int main(int argc, char const *argv[])
{
    std::uniform_int_distribution<uint32_t> u(1, (128 * 1024) / sizeof(unsigned));
    std::uniform_int_distribution<uint32_t> s(1, 16);
    ThreadArg arg;
    arg.length = MallocLength;
    arg.size = new unsigned[arg.length];
    arg.n_size = new unsigned[arg.length];
    for (int i = 0; i < arg.length; i++)
    {
        arg.size[i] = u(e);
        arg.n_size[i] = s(e);
    }
    cout<<"One thread test:"<<endl;
    cout << "Tc Malloc: " << *((TimeRes *)TestTcMallocTime((void *)&arg));
    cout << "Sys Malloc: " << *((TimeRes *)TestSysMallocTime((void *)&arg));
    delete[] arg.size;
    // return 0;
    cout << "Multi-threads test: " << endl;
    assert(false);
    pthread_t tc_tids[ThreadSize];
    pthread_t pt_tids[ThreadSize];
    ThreadArg **args = new ThreadArg *[ThreadSize];
    for (int i = 0; i < ThreadSize; i++)
    {
        args[i] = new ThreadArg;
        args[i]->length = MallocLength;
        args[i]->size = new unsigned[args[i]->length];
        args[i]->n_size = new unsigned[args[i]->length];
        for (int j = 0; j < args[i]->length; j++)
        {
            args[i]->size[j] = u(e);
            args[i]->n_size[j] = s(e);
        }
    }
    for (int i = 0; i < ThreadSize; i++)
    {
        pthread_create(&tc_tids[i], NULL, TestTcMallocTime, (void *)args[i]);
        pthread_create(&pt_tids[i], NULL, TestSysMallocTime, (void *)args[i]);
    }
    TimeRes time_res_tc, time_res_pt;
    for (int i = 0; i < ThreadSize; i++)
    {
        void *t_tc, *t_pt;
        pthread_join(tc_tids[i], &t_tc);
        pthread_join(pt_tids[i], &t_pt);

        time_res_tc += reinterpret_cast<TimeRes *>(t_tc);
        time_res_pt += reinterpret_cast<TimeRes *>(t_pt);
    }
    cout << "tcmalloc:\n"
         << time_res_tc << "malloc:\n"
         << time_res_pt;

    return 0;
}
