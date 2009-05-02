

#include "Observable.h"


#include "Log.h"



const unsigned OE_INTERNAL_DELETE = (unsigned)-2; ///< Used to flag an
                                                  ///observer as to be
                                                  ///removed


//------------------------------------------------------------------------------
ObserverFp::ObserverFp(RegisteredFpGroup * group,
                       Observable * observable,
                       unsigned event) :
    group_(group),
    observable_(observable),
    event_(event)
{
}

//------------------------------------------------------------------------------
const std::string & ObserverFp::toString() const
{
    const static std::string ret("ObserverFp");
    return ret;
}


//------------------------------------------------------------------------------
bool ObserverFp::operator==(const RegisteredFp & other) const
{
    const ObserverFp * other_of = dynamic_cast<const ObserverFp*>(&other);
    if (!other_of) return false;

    return (group_      == other_of->group_ &&
            observable_ == other_of->observable_ &&
            event_      == other_of->event_);
}


//------------------------------------------------------------------------------
void ObserverFp::deregisterPointer() const
{
    observable_->removeObserver(group_, event_);
}


//------------------------------------------------------------------------------
Observable::Observable()
{
}

//------------------------------------------------------------------------------
/**
 *  When this observable is killed, it has to be removed from all
 *  RegisteredFpGroups it is contained in. Traverse all registered
 *  observers (respectively their fp groups) and manually remove this
 *  observable's registration.
 */
Observable::~Observable()
{
    while (!observer_.empty())
    {
        RegisteredObserver * cur_observer = *observer_.begin();

        // don't remove if already flagged for removal
        if (cur_observer->event_ != OE_INTERNAL_DELETE)
        {
            // This just flags for removal, but removes from the group
            cur_observer->group_->deregister(ObserverFp(cur_observer->group_,
                                                        this,
                                                        cur_observer->event_));
            assert(cur_observer->event_ == OE_INTERNAL_DELETE);            
        }

        delete cur_observer;
        observer_.erase(observer_.begin());
    }
}

//------------------------------------------------------------------------------
/**
 *  Adds a new observer. fun will be called upon notification of
 *  event. The pointer to the registered observer is needed for
 *  deregistration.
 */
void Observable::addObserver(ObserverCallbackFun0 fun, unsigned event, RegisteredFpGroup * group)
{
    group->addFunctionPointer(new ObserverFp(group, this, event));
    observer_.push_back(new RegisteredObserver0(group, fun, event));
}

//------------------------------------------------------------------------------
/**
 *  Adds a new observer. fun will be called upon notification of
 *  event. The pointer to the registered observer is needed for
 *  deregistration.
 */
void Observable::addObserver(ObserverCallbackFun2 fun, unsigned event, RegisteredFpGroup * group)
{
    group->addFunctionPointer(new ObserverFp(group, this, event));
    observer_.push_back(new RegisteredObserver2(group, fun, event));
}

//------------------------------------------------------------------------------
/**
 *  Adds a new observer. fun will be called upon notification of
 *  event. The pointer to the registered observer is needed for
 *  deregistration.
 */
void Observable::addObserver(ObserverCallbackFunUserData fun, unsigned event, RegisteredFpGroup * group)
{
    group->addFunctionPointer(new ObserverFp(group, this, event));
    observer_.push_back(new RegisteredObserverUserData(group, fun, event));
}



//------------------------------------------------------------------------------
/**
 *  Called by the subclass to notify all registered observers of the
 *  specified event.
 */
void Observable::emit(unsigned event, void * user_data)
{
    // Clear observables marked for deletion
    ObserverContainer::iterator it = observer_.begin();
    while (it != observer_.end())
    {
        if ((*it)->event_ == OE_INTERNAL_DELETE)
        {
            delete *it;
            it = observer_.erase(it);
        } else
        {
            ++it;
        }
    }

    // In case observers get added during traversal
    ObserverContainer container_copy = observer_;
    
    for (ObserverContainer::iterator it = container_copy.begin();
         it != container_copy.end();
         ++it)
    {
        if ((*it)->event_ == event)
        {
            (*it)->operator()(this, user_data, event);
        }
    }
}

//------------------------------------------------------------------------------
/**
 *  Removes a registered observer.
 *
 *  We cannot delete observers here directly since an emit could be in
 *  progress. Instead, mark observers for deletion.
 */
void Observable::removeObserver(RegisteredFpGroup * group, unsigned event)
{    
    for (ObserverContainer::iterator it = observer_.begin();
         it != observer_.end();
         ++it)
    {
        if ((*it)->group_ == group &&
            (*it)->event_ == event)
        {
            (*it)->event_ = OE_INTERNAL_DELETE;
            return;
        }
    }

    assert(false);
}



