
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
//#include "vld.h"
#endif

#include <CEGUI/CEGUIExceptions.h>

#include "ParameterManager.h"
#include "SdlApp.h"
#include "UserPreferences.h"
#include "Paths.h"

#include "MainMenu.h"
#include "VersionInfo.h"


VersionInfo g_version = VERSION_ZB_CLIENT;
const std::string APP_NAME = "Zero Ballistics";


#ifdef _WIN32
#include "Exception.h"
#include "IntroTask.h"


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
        USER_DATA_SUBDIR = "QuantiCode/" + APP_NAME + "/";


#ifdef _WIN32        
        Win32Exception::install_handler();
        Win32Exception::set_dump_location(getOrCreateUserDataDir(), APP_NAME.c_str());
#endif
        
        redirectStdoutToFile();

        s_params.loadParameters("config_client.xml");
        s_params.loadParameters("config_common.xml");
        s_params.loadParameters("config_server.xml");
        
        s_params.loadParameters(getUserConfigFile(true), CONFIG_SUPERSECTION);

        s_log.open(getOrCreateUserDataDir(), "client");
        s_log << "Version " << g_version << "\n";

#ifdef _WIN32
        if(s_params.get<bool>("client.intro.enable"))
        {
            IntroTask intro(hInstance, nCmdShow);
            intro.run();
        }
#endif        

        s_app.init(argc, argv, APP_NAME, "client");      
        MainMenu main_menu;
        s_app.run(&main_menu);

    } catch (Exception & e)
    {
#ifdef _WIN32


        // show fatal error
        MessageBox( NULL, 
                     e.getTotalErrorString().c_str(),
                    "Fatal Error", 
                    MB_OK | MB_ICONERROR | MB_TASKMODAL );
#endif
        e.addHistory("main()");
        s_log << Log::error << e << "\n";

    } catch (CEGUI::Exception & e)
    {
#ifdef _WIN32
        // show msgbox for cegui exception
        MessageBox( NULL, 
                    e.getMessage().c_str(),
                    "GUI - Fatal Error", 
                    MB_OK | MB_ICONERROR | MB_TASKMODAL );
#endif
	     s_log << "CEGUI Exception occured:" << e.getMessage().c_str() << "\n";
    } 
#ifdef _WIN32
    catch (const Win32Exception & e) 
    {
        std::stringstream err;
        err << e.what() << " (code " << std::hex << e.code()
            << ") at " << e.where() << std::endl << std::endl 
            << "A memory dump has been created and stored at: " << std::endl
            << Win32Exception::get_dump_location() << "\n";

        // show msgbox for win32 exception
        MessageBox( NULL, 
                    err.str().c_str(),
                    "Win32 - Exception", 
                    MB_OK | MB_ICONERROR | MB_TASKMODAL );
    }
#endif


#ifdef _WIN32
    for (int c=0; c<argc; ++c)
    {
        delete[] argv_vec[c];
    }
#endif
    

    return 0;
}
