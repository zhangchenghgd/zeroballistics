
#include "Log.h"

#include "ParameterManager.h"

LogFuncVoid Log::date;   
LogFuncVoid Log::time; 
LogFuncVoid Log::millis;
LogFuncVoid Log::warning;
LogFuncVoid Log::error;


const char * BUILD_DATE = __DATE__;
const char * BUILD_TIME = __TIME__;


//------------------------------------------------------------------------------
/**
 *
 */
Log::Log() : 
    enabled_(true),
    append_cr_(false),
    always_flush_(true),
    debug_classes_(NULL),
    application_section_("undefined")
{
    getCurTime(start_time_);

    date    = &Log::logDate;
    time    = &Log::logTime;
    millis  = &Log::logMillis;
    warning = &Log::logWarning;
    error   = &Log::logError;
}

//------------------------------------------------------------------------------
Log::~Log()
{   
    if (out_.is_open())
    {
        *this << Log::debug('d') << "Log destructor\n";
        *this << Log::millis << " Closing log.\n";
        *this << "--------------------------------------------------------------------------------\n";

        out_.close();
    }
}

//------------------------------------------------------------------------------
void Log::open(const std::string & path, const std::string & application_section)
{
    application_section_ = application_section;

    if (out_.is_open()) return;

    bool append = true;
    try
    {
        log_file_      = path + s_params.get<std::string>(application_section_ + ".log.filename");
        append         = s_params.get<bool>(application_section_ + ".log.append");
        debug_classes_ = s_params.getPointer<std::string>(application_section_ + ".log.debug_classes");
        always_flush_  = s_params.get<bool>(application_section_ + ".log.always_flush");
    } catch (Exception & e)
    {
        e.addHistory("Log::open");
        s_log << Log::error
              << e
              << "\n";
        log_file_ = path + "log_" + application_section_ + ".txt";
    }

    out_.open(log_file_.c_str(), append ? std::ios_base::app : std::ios_base::trunc);
    
    if (out_.fail())
    {
        std::cout << "Warning: unable to open " 
                  << ("log_" + application_section_ + ".txt")
                  << " for writing.\n";

        return;
    }

    
    *this << "\n--------------------------------------------------------------------------------\n"
          << Log::date << ", " << Log::time << "log session started\n"
          << "Build " << BUILD_DATE << " - " << BUILD_TIME << "\n"
          << "Debug classes: " << (debug_classes_ ? *debug_classes_ : "not specified") << "\n";
}


//------------------------------------------------------------------------------
Log & Log::logMessage(const std::string & msg)
{
    if (!enabled_)
    {
        // If logging has been disabled because of a debug message,
        // enable it again if the last character is a newline.
        if (!msg.empty() && *msg.rbegin() == '\n') enabled_ = true;
        return *this;
    }

    if (append_cr_) std::cout << addCr(msg);
    else            std::cout << msg;

    s_console.print(msg);
    
    if (!out_.is_open()) return *this;

    out_ << msg;
    
    if (!out_)
    {
        Exception e("Logging failed for ");
        e << msg;
        throw e;
    }
    
    if (always_flush_)
    {
        out_.flush();
        out_.close();
        out_.open(log_file_.c_str(), std::ios_base::app);
    }
    
    return *this;
}


//------------------------------------------------------------------------------
const TimeValue & Log::getStartTime() const
{
    return start_time_;
}


//------------------------------------------------------------------------------
const std::string & Log::getDebugClasses() const
{
    assert(debug_classes_);
    return *debug_classes_;
}

//------------------------------------------------------------------------------
void Log::setDebugClasses(const std::string & classes)
{
    assert(debug_classes_);
    *debug_classes_ = classes;
}


//------------------------------------------------------------------------------
/**
 *  Whether to append a carriage return to every logged message when
 *  it is printed to cout. Used for output to a raw terminal
 *  (e.g. dedicated server)
 */
void Log::appendCr(bool a)
{
    append_cr_ = a;
}



//------------------------------------------------------------------------------
Log & Log::logDate()
{
    time_t cur_time = ::time(NULL);
    tm * cur_time_split = localtime(&cur_time);

    *this << cur_time_split->tm_mday  << "."
          << cur_time_split->tm_mon+1 << "."
          << cur_time_split->tm_year+1900;
    return *this;
}

//------------------------------------------------------------------------------
Log & Log::logTime()
{
    time_t cur_time = ::time(NULL);
    tm * cur_time_split = localtime(&cur_time);


    *this << cur_time_split->tm_hour << ":"
          << cur_time_split->tm_min  << ":"
          << cur_time_split->tm_sec  << " : ";

	return *this;
}


//------------------------------------------------------------------------------
Log & Log::logMillis()
{
    TimeValue cur_time;
    getCurTime(cur_time);

    float passed_millis = getTimeDiff(cur_time, start_time_);
    
    *this << (int) passed_millis;

    return *this;
}

//------------------------------------------------------------------------------
Log & Log::logWarning()
{
    *this << "\tWarning : ";
    return *this;
}


//------------------------------------------------------------------------------
Log & Log::logError()
{
    *this << "\t\tERROR : ";
    return *this;
}

//------------------------------------------------------------------------------
/**
 *  Debug classes:
 *
 *  - i Initialization
 *  - d destructors, shutdown code
 *  - s serializer
 *  - r resources
 *  - n networking code
 *  - b bulk messages which are expected to be logged every frame
 *  - t Tasks
 *  - N numerical stuff
 *  - m Master server
 *  - M MetaTasks
 *  - l game logic
 *  - o Observable events
 *  - p collision handling & physics
 *  - B Beacon boundary visualization
 *  - H hud notifications
 *  - S Sound (Manager) stuff
 *  - c config file warnings
 *  - k key handling (InputHandler)
 *  - f FP group stuff
 */
Log & Log::logDebug(char type)
{
    if(!debug_classes_) return *this;

    if (((*debug_classes_).find(type) == std::string::npos &&
         (*debug_classes_).find('+') == std::string::npos) ||
         (*debug_classes_).find('-') != std::string::npos)
    {
        enabled_ = false;
    } else
    {
        *this << type << " " << millis << "\t";
    }
    
    return *this;
}

//------------------------------------------------------------------------------
LogManip Log::debug(char ch)
{
    return LogManip(&Log::logDebug, ch);
}

