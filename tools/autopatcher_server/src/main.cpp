
#ifdef _WIN32
#include <tchar.h>
#endif


#include "PatcherServer.h"
#include "ConsoleApp.h"
#include "ParameterManager.h"
#include "VersionInfo.h"

VersionInfo g_version = VERSION_PATCH_SERVER;


//------------------------------------------------------------------------------
#ifdef _WIN32
int _tmain(int argc, _TCHAR* argv[])
{
    Win32Exception::install_handler();
    Win32Exception::set_dump_location(".","patch_sv");
#else
int main( int argc, char **argv )
{
#endif


    try
    {
        s_params.loadParameters("config_patch_server.xml");
        s_log.open("./", "patcher");
        s_log.appendCr(true);

        network::patcher_server::PatcherServer server;
        server.start();
        
        ConsoleApp app;
        app.run();
        
    } catch (Exception & e)
    {
        e.addHistory("main()");
        s_log << Log::error << e << "\n";
    }


    return 0;
}



