// ExporterDLL.cpp : Defines the entry point for the DLL application.
//

#include "ImpBbm.h"

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	// We need the handle to the module to show the options dialog.
	g_hInstance = (HINSTANCE)hModule;
	return TRUE;
}

