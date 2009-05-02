

#include "SdlApp.h"
#include "TankAppServer.h"

#include "UserPreferences.h"
#include "Paths.h"
#include "VersionInfo.h"

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
        s_params.loadParameters("config_server.xml");
        s_params.loadParameters("config_client.xml");
        s_params.loadParameters("config_common.xml");
        s_params.loadParameters(getUserConfigFile(true), CONFIG_SUPERSECTION);

        s_log.open(getOrCreateUserDataDir(), "server");
        s_log << "Version " << g_version << "\n";
        
        s_app.init(argc, argv, "Tank Server", "server");
        
        TankAppServer server_task(true);
        
        s_app.run(&server_task);

        
    } catch (Exception & e)
    {
        e.addHistory("main()");
        s_log << Log::error << e << "\n";

#ifdef _WIN32
        // show fatal error
        MessageBox( NULL, 
                    e.getTotalErrorString().c_str(),
                    "Fatal Error", 
                    MB_OK | MB_ICONERROR | MB_TASKMODAL );
#endif

    }

    return 0;
}


    
