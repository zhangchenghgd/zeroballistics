
#include "Exception.h"

const std::string HISTORY_SEPARATOR = " ->\n\t\t\t";

//------------------------------------------------------------------------------
Exception::Exception()
{
}

//------------------------------------------------------------------------------
Exception::Exception(const std::string & msg)
{   
    stream_ << msg;
}


//------------------------------------------------------------------------------
/**
 *  Needed because stringstream cannot be copied.
 */
Exception::Exception(const Exception & exception) :
    stream_(exception.getTotalErrorString())
{
}

//------------------------------------------------------------------------------
Exception::~Exception()
{
}


//------------------------------------------------------------------------------
std::string Exception::getMessage() const
{
    // strip stack trace information
    std::string str = stream_.str();

    std::size_t p = str.rfind(HISTORY_SEPARATOR);
    if (p == std::string::npos) return str;
    else return str.substr(p + HISTORY_SEPARATOR.size());
}


//------------------------------------------------------------------------------
std::string Exception::getTotalErrorString() const
{
    return stream_.str();
}


//------------------------------------------------------------------------------
/*
 *  Add a function to the exception history (when passing it up the stack
 *  hierarchy)
 */
void Exception::addHistory(const std::string & function)
{
    std::string cur_string = stream_.str();

    stream_.seekp( 0 );
    stream_ << function << HISTORY_SEPARATOR << cur_string;
}

//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const Exception & exception)
{
    std::string final_string = exception.getTotalErrorString();

    if (final_string.compare("") == 0)
    {
        final_string = "Undefined error";
    }
    
    out << final_string;
    return out;
}



#ifdef _WIN32

#include <dbghelp.h>

std::string Win32Exception::dump_location_ = ".\\Appname.dmp";

//------------------------------------------------------------------------------
void Win32Exception::install_handler()
{
    _set_se_translator(Win32Exception::translate);
}

//------------------------------------------------------------------------------
void Win32Exception::set_dump_location(const std::string & path, const std::string & app_name)
{
    dump_location_ = path + "\\" + app_name + ".dmp";
}

//------------------------------------------------------------------------------
std::string Win32Exception::get_dump_location()
{
    return dump_location_;
}

//------------------------------------------------------------------------------
const char * Win32Exception::what() const
{ 
    return info_;
}

//------------------------------------------------------------------------------
const void* Win32Exception::where() const
{ 
    return location_;
}

//------------------------------------------------------------------------------
unsigned Win32Exception::code() const
{ 
    return code_;
}


//------------------------------------------------------------------------------
bool Win32Exception::generateDump(EXCEPTION_POINTERS* pExceptionPointers)
{
    bool minidump_successful;
    std::string dump_path; 

    HANDLE hDumpFile;

    MINIDUMP_EXCEPTION_INFORMATION ExpParam;

    hDumpFile = CreateFile(dump_location_.c_str(), GENERIC_READ|GENERIC_WRITE, 
                FILE_SHARE_WRITE|FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

    ExpParam.ThreadId = GetCurrentThreadId();
    ExpParam.ExceptionPointers = pExceptionPointers;
    ExpParam.ClientPointers = TRUE;

    minidump_successful = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), 
                    hDumpFile, MiniDumpWithDataSegs, &ExpParam, NULL, NULL);

    return minidump_successful;
}

//------------------------------------------------------------------------------
void Win32Exception::translate(unsigned code, EXCEPTION_POINTERS * info)
{
/*  /// XXX possibility to handle each exception differently here 
    /// based on code provided
    switch (code) 
    {
    case EXCEPTION_ACCESS_VIOLATION:
        throw access_violation(*(info->ExceptionRecord));
        break;
    default:
    }
*/

    if(generateDump(info))
    {
        std::cerr << "Generated memory dump at "
                  << dump_location_.c_str() 
                  << "\n";
    }
    else
    {
        std::cerr << "Failed to generate memory dump!!\n";
    }

    // Windows guarantees that *(info->ExceptionRecord) is valid
    throw Win32Exception(*(info->ExceptionRecord));
}

//------------------------------------------------------------------------------
Win32Exception::Win32Exception(const EXCEPTION_RECORD & info ) : 
    info_("undefined"), 
    location_(info.ExceptionAddress), 
    code_(info.ExceptionCode)
{

    switch (info.ExceptionCode) 
    {
    case EXCEPTION_ACCESS_VIOLATION:
        info_ = "Access violation";
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        info_ = "Array bounds exceeded";
        break;
    case EXCEPTION_STACK_OVERFLOW:
        info_ = "Stack overflow";
        break;    
    case EXCEPTION_IN_PAGE_ERROR:
        info_ = "Memory page error";
        break;  
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        info_ = "Illegal instruction";
        break;  
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        info_ = "Division by zero";
        break;
    default:
        info_ = "Undefined Win32 Exception";
        break;
    }


}

#endif
