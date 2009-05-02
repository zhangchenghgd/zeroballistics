

#ifndef RACING_IOBSERVABLE_INCLUDED
#define RACING_IOBSERVABLE_INCLUDED

#include <vector>
#include <iostream>

#include <loki/Functor.h>


#include "RegisteredFpGroup.h"

class Observable;


typedef Loki::Functor<void, LOKI_TYPELIST_3(Observable*, void*, unsigned)> ObserverCallbackFunUserData;
typedef Loki::Functor<void, LOKI_TYPELIST_2(Observable*, unsigned)>        ObserverCallbackFun2;
typedef Loki::Functor<void>                                                ObserverCallbackFun0;


//------------------------------------------------------------------------------
/**
 *  \see RegisteredFpGroup.
 */
class ObserverFp : public RegisteredFp
{
 public:
    ObserverFp(RegisteredFpGroup * group,
               Observable * observable,
               unsigned event);

    virtual const std::string & toString() const;
    virtual bool operator==(const RegisteredFp & other) const;
    virtual void deregisterPointer() const;

    
 protected:
    RegisteredFpGroup * group_; ///< Needed to remove observable from
                                ///group should it be deleted, and for
                                ///manual removal of observers, which
                                ///are identified by their fp group.
    Observable * observable_;   ///< The observable where the
                                ///represented callback is registered.
    unsigned event_;
};


//------------------------------------------------------------------------------
class RegisteredObserver
{
 public:
    RegisteredObserver(RegisteredFpGroup * group, unsigned event) :
        group_(group), event_(event) {}
    virtual ~RegisteredObserver() {}
    
    virtual void operator()(Observable*, void*, unsigned) = 0;
    
    RegisteredFpGroup * group_; ////< Needed to identify the observer
                                ////for manual removals.
    unsigned event_;
};


//------------------------------------------------------------------------------
class RegisteredObserver0 : public RegisteredObserver
{
 public:
    RegisteredObserver0(RegisteredFpGroup * group, ObserverCallbackFun0 fun, unsigned event) :
        RegisteredObserver(group, event), fun_(fun) {}
    virtual ~RegisteredObserver0() {}

    virtual void operator()(Observable*, void * user_data, unsigned)
        {
            fun_();
        }
    
    ObserverCallbackFun0 fun_;
};

//------------------------------------------------------------------------------
class RegisteredObserver2 : public RegisteredObserver
{
 public:
    RegisteredObserver2(RegisteredFpGroup * group, ObserverCallbackFun2 fun, unsigned event) :
        RegisteredObserver(group, event), fun_(fun) {}
    virtual ~RegisteredObserver2() {}


    virtual void operator()(Observable* observable, void * user_data, unsigned event)
        {
            fun_(observable, event);
        }
    
    ObserverCallbackFun2 fun_;
    unsigned event_;
};

//------------------------------------------------------------------------------
class RegisteredObserverUserData : public RegisteredObserver
{
 public:
    RegisteredObserverUserData(RegisteredFpGroup * group, ObserverCallbackFunUserData fun, unsigned event) :
        RegisteredObserver(group, event), fun_(fun) {}
    virtual ~RegisteredObserverUserData() {}


    virtual void operator()(Observable* observable, void * user_data, unsigned event)
        {
            fun_(observable, user_data, event);
        }
    
    ObserverCallbackFunUserData fun_;
    unsigned event_;
};

//------------------------------------------------------------------------------
class Observable
{
    friend class ObserverFp;
 public:
    Observable();
    virtual ~Observable();

    void addObserver(ObserverCallbackFun0        fun, unsigned event, RegisteredFpGroup * group);
    void addObserver(ObserverCallbackFun2        fun, unsigned event, RegisteredFpGroup * group);
    void addObserver(ObserverCallbackFunUserData fun, unsigned event, RegisteredFpGroup * group);

    void emit(unsigned event, void * user_data = NULL);
    
 protected:
    void removeObserver(RegisteredFpGroup * group, unsigned event);
    
    
 private:
    typedef std::vector<RegisteredObserver*> ObserverContainer;
    
    ObserverContainer observer_;
};

#endif // #ifndef RACING_IOBSERVABLE_INCLUDED
