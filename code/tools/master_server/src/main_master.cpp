

#include "ConsoleApp.h"
#include "Exception.h"
#include "Log.h"
#include "ParameterManager.h"

#include "MasterServer.h"

#ifdef _WIN32
#include <tchar.h>
#endif

//------------------------------------------------------------------------------
#ifdef _WIN32
    int _tmain(int argc, _TCHAR* argv[])
    {     

    Win32Exception::install_handler();
    Win32Exception::set_dump_location(".","ranking_sv");


#else
    int main( int argc, char **argv )
        {
#endif
    try
    {
        s_params.loadParameters("config_master.xml");
        s_log.open("./", "master");
        s_log.appendCr(true);

        network::master::MasterServer server;
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



