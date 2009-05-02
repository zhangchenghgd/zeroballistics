
#ifndef RACING_SCHEDULER_INCLUDED
#define RACING_SCHEDULER_INCLUDED

#include <list>

#include <loki/Functor.h>

#include "Singleton.h"
#include "RegisteredFpGroup.h"
#include "Observable.h"

typedef unsigned hTask;
const unsigned INVALID_TASK_HANDLE=(unsigned)-1;

typedef Loki::Functor<void, LOKI_TYPELIST_1(float) > PeriodicTaskCallback;
typedef Loki::Functor<void, LOKI_TYPELIST_1(void*) > SingleEventCallback;

typedef Loki::Functor<void> RenderCallback;


//------------------------------------------------------------------------------
/**
 *  \see RegisteredFpGroup.
 */
class TaskFp : public RegisteredFp
{
 public:
    TaskFp(hTask htask = INVALID_TASK_HANDLE);

    virtual const std::string & toString() const;
    virtual bool operator==(const RegisteredFp & other) const;
    virtual void deregisterPointer() const;

 protected:

    hTask htask_;
};



//------------------------------------------------------------------------------
struct Task
{
    hTask id_;
    std::string name_;
    
    bool is_periodic_; ///< False if single event, true if periodic task.
    float next_time_;  ///< The time in seconds until this task is scheduled to execute next.
    
    PeriodicTaskCallback periodic_callback_; ///< The task's callback function for periodic tasks
    SingleEventCallback  single_callback_;   ///< Callback function for single events

    RegisteredFpGroup * fp_group_; ///< Needed to remove events after they have expired.
    
    union
    {
        float period_;     ///< The period of the task.
        void * user_data_; ///< User data pointer for single event.
    };

    bool operator<(const Task & other)
        {
            if (next_time_ < other.next_time_) return true;
            else if (next_time_ > other.next_time_) return false;
            else return name_.compare(other.name_) > 0;
        }
};



#define s_scheduler Loki::SingletonHolder<Scheduler, Loki::CreateUsingNew, SingletonSchedulerLifetime >::Instance()
//------------------------------------------------------------------------------
class Scheduler : public Observable
{
    friend class TaskFp;
    
    DECLARE_SINGLETON(Scheduler);
 public:
    virtual ~Scheduler();

    void frameMove(float dt);
    float getLastFrameDt() const;

    hTask addTask (PeriodicTaskCallback callback, float period,
                   const std::string & name,
                   RegisteredFpGroup * group);
    hTask addEvent(SingleEventCallback  callback, float delay,
                   void * user_data,
                   const std::string & name,
                   RegisteredFpGroup * group);
    hTask addFrameTask(PeriodicTaskCallback callback,
                       const std::string & name,
                       RegisteredFpGroup * group);
    
    void reschedule(hTask task, float new_time);
    float getExecutionDelay(hTask task) const;
    
    void * removeTask(hTask task, RegisteredFpGroup * fp_group);

    void setRenderCallback(RenderCallback callback);

    const std::string & getTaskName(hTask task) const;

    RegisteredFpGroup & getFpGroup();
    
    std::string printTasks(const std::vector<std::string>&args);

 protected:

    typedef std::list<Task> TaskContainer;

    void removeTask(hTask task);

    void reschedule(TaskContainer::iterator scheduled_task);

    void dummyRender() {}
    
    float last_frame_dt_;

    RenderCallback render_callback_;

    unsigned next_id_;
    
    TaskContainer task_;

    RegisteredFpGroup fp_group_;
};


#endif // #ifndef RACING_SCHEDULER_INCLUDED
