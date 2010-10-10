/*
NAME
    SimpleSymbolEngine

DESCRIPTION
    Simple symbol engine functionality.
    This is demonstration code only - it is non. thread-safe and single instance.

COPYRIGHT
    Copyright (C) 2004 by Roger Orr <rogero@howzatt.demon.co.uk>

    This software is distributed in the hope that it will be useful, but
    without WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    Permission is granted to anyone to make or distribute verbatim
    copies of this software provided that the copyright notice and
    this permission notice are preserved, and that the distributor
    grants the recipent permission for further distribution as permitted
    by this notice.

    Comments and suggestions are always welcome.
    Please report bugs to rogero@howzatt.demon.co.uk.
*/

#include "SimpleSymbolEngine.h"

#include <psapi.h>

#include <iostream>
#include <sstream>
#include <cstddef>

#pragma comment( lib, "dbghelp" )

static char const szRCSID[] = "$Id: SimpleSymbolEngine.cpp,v 1.4 2005/05/04 21:52:05 Eleanor Exp $";

//////////////////////////////////////////////////////////////////////////////////////
// Singleton for the engine (SymInitialize doesn't support multiple calls)
SimpleSymbolEngine& SimpleSymbolEngine::instance()
{
static SimpleSymbolEngine theEngine;
    return theEngine;
}

/////////////////////////////////////////////////////////////////////////////////////
SimpleSymbolEngine::SimpleSymbolEngine()
{
    hProcess = GetCurrentProcess();

    DWORD dwOpts = SymGetOptions();
    dwOpts |= SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS;
    SymSetOptions ( dwOpts );

    ::SymInitialize( hProcess, 0, true );
}

/////////////////////////////////////////////////////////////////////////////////////
SimpleSymbolEngine::~SimpleSymbolEngine()
{
    ::SymCleanup( hProcess );
}

/////////////////////////////////////////////////////////////////////////////////////
std::string SimpleSymbolEngine::addressToString( PVOID address )
{
    std::ostringstream oss;

    // First the raw address
    oss << "0x" << address;

    // Then any name for the symbol
    struct tagSymInfo
    {
        IMAGEHLP_SYMBOL symInfo;
        char nameBuffer[ 4 * 256 ];
    } SymInfo = { { sizeof( IMAGEHLP_SYMBOL ) } };

    IMAGEHLP_SYMBOL * pSym = &SymInfo.symInfo;
    pSym->MaxNameLength = sizeof( SymInfo ) - offsetof( tagSymInfo, symInfo.Name );

    DWORD dwDisplacement;
    if ( SymGetSymFromAddr( hProcess, (DWORD)address, &dwDisplacement, pSym) )
    {
        oss << " " << pSym->Name;
        if ( dwDisplacement != 0 )
            oss << "+0x" << std::hex << dwDisplacement << std::dec;
    }
        
    // Finally any file/line number
    IMAGEHLP_LINE lineInfo = { sizeof( IMAGEHLP_LINE ) };
    if ( SymGetLineFromAddr( hProcess, (DWORD)address, &dwDisplacement, &lineInfo ) )
    {
        char const *pDelim = strrchr( lineInfo.FileName, '\\' );
        oss << " at " << ( pDelim ? pDelim + 1 : lineInfo.FileName ) << "(" << lineInfo.LineNumber << ")";
    }
    return oss.str();
}

/////////////////////////////////////////////////////////////////////////////////////
// StackTrace: try to trace the stack to the given output
void SimpleSymbolEngine::StackTrace( PCONTEXT pContext, std::ostream & os )
{
    os << "  Frame       Code address\n";

    STACKFRAME stackFrame = {0};

    stackFrame.AddrPC.Offset = pContext->Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;

    stackFrame.AddrFrame.Offset = pContext->Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;

    stackFrame.AddrStack.Offset = pContext->Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;

    while ( ::StackWalk(
       IMAGE_FILE_MACHINE_I386,
       hProcess,
       GetCurrentThread(), // this value doesn't matter much if previous one is a real handle
       &stackFrame, 
       pContext,
       NULL,
       ::SymFunctionTableAccess,
       ::SymGetModuleBase,
       NULL ) )
    {
        os << "  0x" << (PVOID) stackFrame.AddrFrame.Offset << "  " << addressToString( (PVOID)stackFrame.AddrPC.Offset ) << "\n";
    }

    os.flush();
}
