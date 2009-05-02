

#undef assert
#ifdef ENABLE_DEV_FEATURES
#define assert(exp) assertFun(exp, #exp, __LINE__, __FILE__)
#else
#define assert(exp)
#endif


#ifndef TOOLBOX_ASSERT_INCLUDED
#define TOOLBOX_ASSERT_INCLUDED

#include <iostream>
#include <sstream>

#include "Datatypes.h" // used for windows headers MsgBox

//------------------------------------------------------------------------------
inline void assertFun(bool exp, const char * exp_str, unsigned line, const char * file)
{
    if (!exp)
    {

        std::stringstream assertion;
        assertion << "Assertion failed: \""
              << exp_str
              << "\" at line "
              << line
              << " in file "
              << file
              << ".\n";


#ifdef s_log        
        s_log << Log::error
#else
        std::cout
#endif
        << assertion;

        // Bail to debugger...
#ifdef _DEBUG
#ifdef _WIN32
        _asm { int 3 };
#else
        __asm__("int3");
#endif 

#else // in Release mode, win32 with enabled dev features show MsgBox
#ifdef ENABLE_DEV_FEATURES
#ifdef _WIN32
        // show msgbox for assertions in release build
        MessageBox( NULL, 
                    assertion.str().c_str(),
                    "Assertion", 
                    MB_OK | MB_ICONERROR | MB_TASKMODAL );    
#endif
#endif
#endif
    }
}


#endif
