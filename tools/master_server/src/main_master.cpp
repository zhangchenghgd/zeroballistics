

#include "ConsoleApp.h"
#include "Exception.h"
#include "Log.h"
#include "ParameterManager.h"

#include "MasterServer.h"

//------------------------------------------------------------------------------
#ifdef _WIN32
int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
    LPWSTR lpCmdLineW = GetCommandLineW();
    int argc;
    LPWSTR * argvw = CommandLineToArgvW(lpCmdLineW, &argc);

    assert(argc != 0);
    
    std::vector<char*> argv_vec(argc);

    for (int c=0; c<argc; ++c)
    {
        int buf_size = WideCharToMultiByte(CP_ACP, 0, argvw[c], -1, argv_vec[c], 0, NULL, NULL);
        argv_vec[c] = new char[buf_size];
        WideCharToMultiByte(CP_ACP, 0, argvw[c], -1, argv_vec[c], buf_size, NULL, NULL);
    }

    char ** argv = (char**)&argv_vec[0];
    
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



