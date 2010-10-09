
#include <stdio.h>
#include <stdlib.h>
/*
#ifdef _WIN32
#include "vld.h"
#endif
*/

#include <CEGUI/CEGUIExceptions.h>

#include "ParameterManager.h"
#include "SdlApp.h"
#include "UserPreferences.h"
#include "Paths.h"

#include "ParticleViewer.h"
#include "Exception.h"

const std::string APP_NAME = "Zero Ballistics";


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
    
    Win32Exception::install_handler();
    Win32Exception::set_dump_location(getOrCreateUserDataDir(),"ParticleViewer");

#else
    int main( int argc, char **argv )
        {
#endif
    try
    {
        USER_DATA_SUBDIR = "QuantiCode/" + APP_NAME + "/";

        s_params.loadParameters("config_client.xml"); // XXX rework, scene manager needs that in init
                                                      // shadows and graphics section
        s_params.loadParameters("config_common.xml");
        s_params.loadParameters(getUserConfigFile(), CONFIG_SUPERSECTION);
        s_params.loadParameters(VIEWER_CONFIG_FILE);

        s_log.open(getOrCreateUserDataDir(), "particle_viewer");

        s_app.init(argc, argv, "Particle Viewer", "viewer");
        
        ParticleViewer viewer_task;
        s_app.run(&viewer_task);
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

    } catch (CEGUI::Exception & e)
    {
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
