
#ifndef TOOLBOX_REGISTERED_FP_GROUP_INCLUDED
#define TOOLBOX_REGISTERED_FP_GROUP_INCLUDED


#include <vector>
#include <string>


//------------------------------------------------------------------------------
class RegisteredFp
{
 public:
    virtual ~RegisteredFp() {}
    
    virtual const std::string & toString() const              = 0;
    virtual bool operator==(const RegisteredFp & other) const = 0;
    virtual void deregisterPointer() const                    = 0;
};


//------------------------------------------------------------------------------
/**
 *  This class exists to automatically deregister callbacks upon the
 *  destruction of the object implementing the callback, e.g. console
 *  functions, input handler functions, scheduler tasks etc...
 *
 *  A class derived from RegisteredFp knowing how to deregister the
 *  function is needed.
 */
class RegisteredFpGroup
{
 public:

    RegisteredFpGroup();
    RegisteredFpGroup(const RegisteredFpGroup & other);

    ~RegisteredFpGroup();
    
    void addFunctionPointer(RegisteredFp * fp);
    bool existsFp(const RegisteredFp & fp) const;
    bool isEmpty() const;
    void deregister(const RegisteredFp & fp);
    void deregisterAllOfType(const RegisteredFp & type);
    
 protected:

    // don't allow assignment...
    RegisteredFpGroup & operator=(const RegisteredFpGroup & other);

    
    std::vector<RegisteredFp*> fp_;
};


#endif // #ifndef TOOLBOX_REGISTERED_FP_GROUP_INCLUDED
