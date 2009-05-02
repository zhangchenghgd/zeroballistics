#ifndef LIB_EXCEPTION_INCLUDED
#define LIB_EXCEPTION_INCLUDED

#include <string>
#include <iostream>
#include <sstream>



//------------------------------------------------------------------------------
/**
 *  Used by console, scheduler, inputhandler etc to notify anybody who
 *  is interested of caught excpetions.
 */
enum ExceptionEvent
{
    EE_EXCEPTION_CAUGHT
};


//------------------------------------------------------------------------------
class Exception 
{
 public:
    Exception();
    Exception(const std::string & msg);
    Exception(const Exception & exception);
    ~Exception();

    std::string getMessage() const;
    std::string getTotalErrorString() const;
    
    void addHistory(const std::string & function);

    // forward everything to our stringstream
    template<class T> Exception & operator<<(const T & t)
        {
            stream_ << t;
            return *this;
        }
    
 protected:
    std::ostringstream stream_;
};


std::ostream & operator<<(std::ostream & out, const Exception & exception);




#ifdef _WIN32

#include <exception>
#include "Datatypes.h" // includes windows.h in the right order

//------------------------------------------------------------------------------
/**
 *  Class that registers a handler to catch win32 internal
 *  exceptions and handles them as C++ exceptions
 **/
class Win32Exception: public std::exception
{
public:
    static void install_handler();

    virtual const char * what() const;
    const void* where() const;
    unsigned code() const;

protected:
    Win32Exception(const EXCEPTION_RECORD & info);
    static void translate(unsigned code, EXCEPTION_POINTERS * info);

private:
    const char* info_;
    const void* location_;
    unsigned code_;
};

#endif // _WIN32

#endif // ifndef LIB_EXCEPTION_INCLUDED
