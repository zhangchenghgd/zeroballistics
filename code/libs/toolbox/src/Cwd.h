
#ifndef TOOLBOX_CWD_INCLUDED
#define TOOLBOX_CWD_INCLUDED


#ifdef CWDEBUG

#include <string>
#include <vector>
#include "pthread.h"

#include "Log.h"

//------------------------------------------------------------------------------
class MyLock
{
 public:
    MyLock()
        {
            pthread_mutex_init(&mutex_, NULL);
        }

    ~MyLock()
        {
            pthread_mutex_destroy(&mutex_);
        }

    
    void lock()
        {
            pthread_mutex_lock(&mutex_);      
        }

    int trylock()
        {
            return pthread_mutex_trylock(&mutex_);
        }

    void unlock()
        {
            pthread_mutex_unlock(&mutex_); 
        }
    
 protected:
    pthread_mutex_t mutex_;
} lock;



libcwd::alloc_filter_ct own_filter(libcwd::show_allthreads | libcwd::show_objectfile);

//------------------------------------------------------------------------------
class InitCwd
{
 public:
    InitCwd()
        {
            Debug(
                libcw_do.set_ostream(&std::cerr, &lock);
                dc::bfd.on();
                dc::malloc.on();

//                own_filter.hide_unknown_locations();
                );
        }
} g_init_cwd;



//------------------------------------------------------------------------------
inline std::string togglePrintAllocs(const std::vector<std::string> & args)
{
    Debug(
        static bool on = false;
        on ^= 1;
        
        if (on)
    {
        make_all_allocations_invisible_except(NULL);
    } else
    {
        libcw_do.on();
        list_allocations_on(libcw_do, own_filter);
        libcw_do.off();
    }
        );

    return "";
}

//------------------------------------------------------------------------------
inline std::string printPrevAllocations(const std::vector<std::string> & args)
{

    TimeValue cur_time;
    getCurTime(cur_time);

    std::cerr << "\n" << getTimeDiff(cur_time, s_log.getStartTime())/1000.0f << " s after startup:\n";
    
    Debug(


        libcwd::alloc_filter_ct filter_secs(libcwd::show_allthreads | libcwd::show_objectfile);
        filter_secs.hide_unknown_locations();


//        filter_secs.hide_untagged_allocations();
        

        struct timeval start;
        struct timeval end;
        gettimeofday(&start, 0);
        gettimeofday(&end, 0);
        start.tv_sec -= 16;
        end.tv_sec   -= 15;
        filter_secs.set_time_interval(start, end);



        /*
         std::vector<std::string> masks; 

         masks.push_back("libc.so*"); 
         masks.push_back("libstdc++*"); 
      
         alloc_filter.hide_objectfiles_matching(masks); 
        */



        libcw_do.on();
        list_allocations_on(libcw_do, filter_secs );
        libcw_do.off();

        );

    return "";
}



#endif
#endif
