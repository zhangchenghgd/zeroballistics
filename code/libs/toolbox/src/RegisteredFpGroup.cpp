#include "RegisteredFpGroup.h"


#include "Log.h"


//------------------------------------------------------------------------------
RegisteredFpGroup::RegisteredFpGroup()
{
}


//------------------------------------------------------------------------------
/**
 *  We don't want to copy the function pointers...
 */
RegisteredFpGroup::RegisteredFpGroup(const RegisteredFpGroup & other)
{
}


//------------------------------------------------------------------------------
RegisteredFpGroup::~RegisteredFpGroup()
{
    for (std::vector<RegisteredFp*>::iterator it = fp_.begin();
         it != fp_.end();
         ++it)
    {
        (*it)->deregisterPointer();
        delete *it;
    }
}

//------------------------------------------------------------------------------
/**
 *  Adds a new callback manager object to this group. Upon deletion of
 *  the group, its deregisterPointer method is called, allowing it to
 *  deregister the callback it represents.
 */
void RegisteredFpGroup::addFunctionPointer(RegisteredFp * fp)
{
#ifdef _DEBUG
    // see if fp was already registered
    for (unsigned i=0; i<fp_.size(); ++i)
    {
        if (*fp_[i] == *fp)
        {
            s_log << Log::error
                  << fp->toString()
                  << " added twice in RegisteredFpGroup::addFunctionPointer.\n";
            return;
        }
    }
#endif

    fp_.push_back(fp);
}

//------------------------------------------------------------------------------
/**
 *  Checks whether a callback manager object already
 *  exists. operator== must be implemented accordingly for the derived
 *  RegisteredFp class.
 */
bool RegisteredFpGroup::existsFp(const RegisteredFp & fp) const
{
    for (std::vector<RegisteredFp*>::const_iterator it = fp_.begin();
         it != fp_.end();
         ++it)
    {
        if (fp == **it)
        {
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------------
/**
 *  \return Whether any functionfp objects have been registered.
 */
bool RegisteredFpGroup::isEmpty() const
{
    return fp_.empty();
}


//------------------------------------------------------------------------------
/**
 *  Used for manually deregistering a callback. Since the
 *  RegisteredFpGroup must be notified of any deregistrations, they
 *  must always happen via this function, and are made protected in
 *  the classes calling the callbacks (e.g. Scheduler).
 */
void RegisteredFpGroup::deregister(const RegisteredFp & fp)
{
    for (std::vector<RegisteredFp*>::iterator it = fp_.begin();
         it != fp_.end();
         ++it)
    {
        if (fp == **it)
        {
            (*it)->deregisterPointer();
            delete *it;
            fp_.erase(it);            
            return;
        }
    }

    assert(false);
}


//------------------------------------------------------------------------------
/**
 *  Deregisters all callbacks contained within the group.
 */
void RegisteredFpGroup::deregisterAllOfType(const RegisteredFp & type)
{
    for (std::vector<RegisteredFp*>::iterator it = fp_.begin();
         it != fp_.end();
         /* do nothing */)
    {
        if (typeid(type) == typeid(**it))
        {
            (*it)->deregisterPointer();
            delete *it;
            it = fp_.erase(it);
        } else  ++it;
    }
}


