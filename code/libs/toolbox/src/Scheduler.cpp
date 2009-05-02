

#include "Scheduler.h"

#include <limits>

#include "Log.h"

#undef min
#undef max


/// Used to mark a task as "frame task", which is called after every
/// rendered frame.
const float FRAME_TASK_TIME = std::numeric_limits<float>::max();

//------------------------------------------------------------------------------
TaskFp::TaskFp(hTask task) :
    htask_(task)
{
}


//------------------------------------------------------------------------------
const std::string & TaskFp::toString() const
{
    return s_scheduler.getTaskName(htask_);
}


//------------------------------------------------------------------------------
bool TaskFp::operator==(const RegisteredFp & other) const
{
    const TaskFp * other_tf = dynamic_cast<const TaskFp*>(&other);
    if (!other_tf) return false;

    return htask_ == other_tf->htask_;
}


//------------------------------------------------------------------------------
void TaskFp::deregisterPointer() const
{
    s_scheduler.removeTask(htask_);
}



//------------------------------------------------------------------------------
Scheduler::Scheduler() :
    last_frame_dt_(0.0f),
    render_callback_(RenderCallback(this, &Scheduler::dummyRender)),
    next_id_(0)
{
    s_log << Log::debug('i') << "Scheduler constructor\n";

#ifdef ENABLE_DEV_FEATURES    
    s_console.addFunction("listTasks",
                          ConsoleFun(this, &Scheduler::printTasks),
                          &fp_group_);
#endif
}

//------------------------------------------------------------------------------
Scheduler::~Scheduler()
{
    fp_group_.deregisterAllOfType(TaskFp());
    
    s_log << Log::debug('d') << "Scheduler destructor\n";

    for (TaskContainer::const_iterator it = task_.begin();
         it != task_.end();
         ++it)
    {
        if (it->id_ != INVALID_TASK_HANDLE)
        {
            if (it->is_periodic_)
            {
                s_log << Log::warning << "Task " << it->name_
                      << " is still registered.\n";
            }
        }
    }
}

//------------------------------------------------------------------------------
/**
 *  Advances the simulation time dt seconds. All tasks in the interval
 *  cur_time_, cur_time_ + dt will be executed. The rendering task
 *  will be executed once after all other task updates if present.
 */
void Scheduler::frameMove(float dt)
{
    last_frame_dt_ = dt;
    unsigned tasks_per_frame = 0;    
    if (!task_.empty())
    {
        // Update task next_time_
        for (TaskContainer::iterator it = task_.begin();
             it != task_.end();
             ++it)
        {
            if (it->next_time_ != FRAME_TASK_TIME) it->next_time_ -= dt;
        }
        
        Task * cur_task;
        while (cur_task = &(*task_.begin()),
               cur_task->next_time_ < 0.0f)
        {            
            ++tasks_per_frame;
            
            // remove deregistered tasks
            if (cur_task->id_ == INVALID_TASK_HANDLE)
            {
                task_.erase(task_.begin());
                if (task_.size() == 0) return;
                continue;
            }

            try
            {
                if (cur_task->is_periodic_)
                {
                    // re-schedule periodic tasks
                    
                    // Do this before doing the callback, in case another task is inserted
                    // and comes first in the list.
                    cur_task->next_time_ += cur_task->period_;
                    reschedule(task_.begin()); 
                    
                    cur_task->periodic_callback_(cur_task->period_);
                } else
                {
                    // remove nonperiodic tasks
                    s_log << Log::debug('t')
                          << "Executing event "
                          << cur_task->name_
                          << "\n";

                    // Store callback and user data pointer to call
                    // task after erasing it. This avoids issues when
                    // the callback inserts tasks which come on first
                    // place.
                    void * user_data             = cur_task->user_data_;
                    SingleEventCallback callback = cur_task->single_callback_;

                    // Remove expired events from their respective fp
                    // group.
                    TaskFp tf(cur_task->id_);
                    cur_task->fp_group_->deregister(tf);
                    
                    task_.erase(task_.begin());
                    cur_task = NULL;
                    
                    callback(user_data);    
                }

                if (task_.size() == 0) break;
            } catch (Exception & e)
            {
                if (cur_task) e.addHistory(cur_task->name_);
                s_log << Log::error << e << "\n";
                emit(EE_EXCEPTION_CAUGHT, &e);
            }
        }
    }

    // Delete old frame tasks
    std::list<Task>::reverse_iterator it = task_.rbegin();
    while (it != task_.rend() && it->next_time_ == FRAME_TASK_TIME)
    {
        if (it->id_ == INVALID_TASK_HANDLE)
        {
            task_.erase(--it.base());
            it = task_.rbegin();
        } else ++it;
    }

    // Perform frame tasks
    it = task_.rbegin();
    while (it != task_.rend() && it->next_time_ == FRAME_TASK_TIME)
    {
        if (it->id_ != INVALID_TASK_HANDLE)
        {
            try
            {
                it->periodic_callback_(dt);
            } catch (Exception & e)
            {
                e.addHistory(it->name_);
                s_log << Log::error
                      << e
                      << "\n";
                emit(EE_EXCEPTION_CAUGHT, &e);
            }
        }
        ++it;
    }

    ADD_LOCAL_CONSOLE_VAR(unsigned, tasks_per_frame);
    
    render_callback_();
}

//------------------------------------------------------------------------------
float Scheduler::getLastFrameDt() const
{
    return last_frame_dt_;
}


//------------------------------------------------------------------------------
/**
 *  Adds a new task.
 *
 *  \param callback The callback to be scheduled.
 *
 *  \param period The period of the task in seconds. 
 */
hTask Scheduler::addTask (PeriodicTaskCallback callback, float period,
                          const std::string & name,
                          RegisteredFpGroup * group)
{
    s_log << Log::debug('t')
          << "Adding task "
          << name;
    if (equalsZero(period))
    {
        s_log << " with period 0.0\n";
    } else
    {
        s_log << " with FPS " << 1.0f / period <<"\n";
    }
    
    TaskContainer::iterator inserted = task_.insert(task_.begin(), Task());

    inserted->id_                = next_id_;
    inserted->is_periodic_       = true;
    inserted->periodic_callback_ = callback;
    inserted->next_time_         = 0.0f;
    inserted->period_            = period;
    inserted->name_              = name;
    inserted->fp_group_          = NULL;
    
    group->addFunctionPointer(new TaskFp(next_id_));

    return next_id_++;
}


//------------------------------------------------------------------------------
hTask Scheduler::addEvent(SingleEventCallback  callback, float delay,
                          void * user_data,
                          const std::string & name,
                          RegisteredFpGroup * group)
{
    s_log << Log::debug('t')
          << "Scheduling event "
          << name
          << " with delay "
          << delay
          << "\n";
    
    assert(delay >= 0.0f);
    
    TaskContainer::iterator inserted = task_.insert(task_.begin(), Task());
    
    inserted->id_              = next_id_;
    inserted->is_periodic_     = false;
    inserted->single_callback_ = callback;
    inserted->next_time_       = delay;
    inserted->user_data_       = user_data;
    inserted->name_            = name;
    inserted->fp_group_        = group;

    reschedule(task_.begin());

    group->addFunctionPointer(new TaskFp(next_id_));

    return next_id_++;
}


//------------------------------------------------------------------------------
/**
 *  Adds a task that will be called after the completion of each
 *  frame.
 */
hTask Scheduler::addFrameTask(PeriodicTaskCallback callback,
                              const std::string & name,
                              RegisteredFpGroup * group)
{
    s_log << Log::debug('t')
          << "Adding frame task "
          << name
          << "\n";

    task_.push_back(Task());
    Task & inserted = *task_.rbegin();

    inserted.id_                = next_id_;
    inserted.is_periodic_       = true;
    inserted.periodic_callback_ = callback;
    inserted.next_time_         = FRAME_TASK_TIME;
    inserted.period_            = 0;
    inserted.name_              = name;
    inserted.fp_group_          = NULL;

    
    group->addFunctionPointer(new TaskFp(next_id_));

    return next_id_++;
}

//------------------------------------------------------------------------------
/**
 *  hTask can either be an event or a task. For an event, new_time is
 *  the new delay before the event is triggered. For a periodic tasks,
 *  new_time is the new period time (not touching the current time
 *  until next execution).
 */
void Scheduler::reschedule(hTask task, float new_time)
{
    assert(task != INVALID_TASK_HANDLE);
    
    for (TaskContainer::iterator it = task_.begin();
         it != task_.end();
         ++it)
    {
        if (it->id_ == task)
        {
            s_log << Log::debug('t')
                  << "Rescheduling task " << it->name_ << "\n";

            if (it->is_periodic_)
            {
                it->period_ = new_time;
            } else
            {
                it->next_time_ = new_time;
                reschedule(it);
            }
            
            return;
        }
    }
    
    s_log << Log::error
          << "Tried to reschedule non-existing task "
          << task
          << "\n";
}

//------------------------------------------------------------------------------
/**
 *  Returns the time until the given task is executed.
 */
float Scheduler::getExecutionDelay(hTask task) const
{
    assert(task != INVALID_TASK_HANDLE);
    
    for (TaskContainer::const_iterator it = task_.begin();
         it != task_.end();
         ++it)
    {
        if (it->id_ == task) return it->next_time_;
    }
    
    s_log << Log::error
          << "getExecutionDelay called fo nonexisting task "
          << task
          << "\n";
    
    return 0.0f;
}



//------------------------------------------------------------------------------
/**
 *  Helper function to deregister a scheduler task. Ignores tasks
 *  which are INVALID_TASK_HANDLE.
 *
 *  \return The supplied user data pointer if this is a event, NULL
 *  otherwise.
 */
void * Scheduler::removeTask(hTask task, RegisteredFpGroup * fp_group)
{
    if (task == INVALID_TASK_HANDLE) return NULL;

    void * ret = NULL;
    for (TaskContainer::iterator it = task_.begin();
         it != task_.end();
         ++it)
    {
        if (it->id_ == task && !it->is_periodic_) ret = it->user_data_;
    }
    
    fp_group->deregister(TaskFp(task));

    return ret;
}


//------------------------------------------------------------------------------
/**
 *  If a render server is set, its update() function will be called
 *  once for every call to frameMove.
 */
void Scheduler::setRenderCallback(RenderCallback callback)
{
    render_callback_ = callback;
}

//------------------------------------------------------------------------------
const std::string & Scheduler::getTaskName(hTask task) const
{
    for (TaskContainer::const_iterator it = task_.begin();
         it != task_.end();
         ++it)
    {
        if (it->id_ == task) return it->name_;
    }

    const static std::string unknown = "Unknown task";
    return unknown;
}


//------------------------------------------------------------------------------
RegisteredFpGroup & Scheduler::getFpGroup()
{
    return fp_group_;
}


//------------------------------------------------------------------------------
std::string Scheduler::printTasks(const std::vector<std::string>&args)
{
    if (!args.empty()) return "Function takes no arguments\n";

    std::ostringstream strstr;
    for (TaskContainer::const_iterator it = task_.begin();
         it != task_.end();
         ++it)
    {
        if (it->id_ == INVALID_TASK_HANDLE)
        {
            strstr << "<defunct>";
        } else
        {
            strstr << it->id_;
        }
        strstr << ": "
               << it->name_
               << " ";

        if (it->is_periodic_)
        {
            if (equalsZero(it->period_))
            {
                strstr << "with period "
                       << it->period_
                       << " ";
            } else
            {
                strstr << "with FPS "
                       << 1.0f / it->period_
                       << " ";
            }
        } else
        {
            strstr << "in "
                   << it->next_time_
                   << " seconds.";
        }

        strstr << "\n";
    }

    return strstr.str();
}




//------------------------------------------------------------------------------
/**
 *  Removed tasks are not erased from the task list instantly because
 *  they could be just handled by framemove, instead they are marked
 *  for removal.
 */
void Scheduler::removeTask(hTask task)
{    
    for (TaskContainer::iterator it = task_.begin();
         it != task_.end();
         ++it)
    {
        if (it->id_ == task)
        {
            s_log << Log::debug('t') << "Removing task " << it->name_ << "\n";
            it->id_ = INVALID_TASK_HANDLE; // Flag for removal            
            return;
        }
    }


    s_log << Log::error
          << "Tried to remove non-existing task "
          << task
          << "\n";
}

//------------------------------------------------------------------------------
/**
 *  Moves the given item of the task list to its proper place based on
 *  next_time_.
 */
void Scheduler::reschedule(TaskContainer::iterator scheduled_task)
{
    // bail if list has only one element
    if (task_.size() == 1) return;
    
    TaskContainer::iterator it = task_.begin();

    // search element before which to insert task
    while (it != task_.end() &&
           (it == scheduled_task || *it < *scheduled_task))
    {
        ++it;
    }

    // reinsert task
    task_.splice(it, task_, scheduled_task);
}
