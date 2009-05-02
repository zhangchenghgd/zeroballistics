
#ifndef LIB_LOG_INCLUDED
#define LIB_LOG_INCLUDED



#include <fstream>
#include <string>

#include <time.h>

#include "Singleton.h"

#include "Exception.h"
#include "Utils.h"
#include "TimeStructs.h"
#include "Console.h"


class Log;

typedef Log & (Log::*LogFuncVoid)();
typedef Log & (Log::*LogFuncChar)(char);



template<class T> Log & operator<<(Log & log, const T & msg);

 
//------------------------------------------------------------------------------
/**
 *  Encapsulates a member function pointer of Log with the corresponding
 *  argument, which is restricted to be an unsigned.
 */
class LogManip
{
    template<class T>
    friend Log & operator<< (Log & log, const T & msg);

 public:
    LogManip(LogFuncChar f, unsigned i) : func_(f), arg_(i) {}
 private:
    LogFuncChar func_;
    unsigned arg_;
};


#define s_log Loki::SingletonHolder<Log, Loki::CreateUsingNew, SingletonLogLifetime >::Instance()
//------------------------------------------------------------------------------
class Log
{
    DECLARE_SINGLETON(Log);
 public:
    virtual ~Log();

    void open(const std::string & path, const std::string & application_section);
    
    Log & logMessage(const std::string & msg);

    const TimeValue & getStartTime() const;

    const std::string & getDebugClasses() const;
    void setDebugClasses(const std::string & classes);

    void appendCr(bool a);
    
    static LogFuncVoid time;
    static LogFuncVoid millis;
    static LogFuncVoid date;
    static LogFuncVoid warning;
    static LogFuncVoid error;

     
    static LogManip debug(char ch);
 
 protected:

    Log & logTime();
    Log & logMillis();
    Log & logDate();
    Log & logWarning();
    Log & logError();
    Log & logDebug(char type);
    
    std::ofstream out_;

    TimeValue start_time_;

    bool enabled_;
    bool append_cr_;
    bool always_flush_;

    std::string * debug_classes_;
    std::string application_section_;
    std::string log_file_;
};
     

//------------------------------------------------------------------------------
/*
 *  Ordinary arguments are just forwarded to our ofstream
 */
template<class T>
inline Log & operator<< (Log & log, const T & msg)
{
    std::ostringstream str;
    str << msg;
    return log.logMessage(str.str());
}
     
//------------------------------------------------------------------------------
/*
 *  Executes the Log member function with the argument encapsulated in manip.
 */
template<>
inline Log & operator<<(Log & log, const LogManip & manip)
{
    return (log.*manip.func_)(manip.arg_);
}
     
//------------------------------------------------------------------------------
/*
 *  Exectues the Log member function pointed to by func.
 */
template<>
inline Log & operator<<(Log & log, const LogFuncVoid & func)
{
    return (log.*func)();
}


#endif // #ifndef LIB_LOG_INCLUDED
