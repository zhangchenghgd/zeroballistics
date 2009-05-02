
#ifdef _WIN32
#include <tchar.h>
#endif


#include "Log.h"
#include "PatcherApp.h"

#include "VersionInfo.h"
#include "Exception.h"
#include "ParameterManager.h"


VersionInfo g_version = VERSION_PATCH_CLIENT;


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

        Win32Exception::install_handler();
        Win32Exception::set_dump_location(".","patch_client");
#else
    int main( int argc, char **argv )
    {
#endif
        try
        {
            s_params.loadParameters(CONFIG_FILE);
    
            s_log.open("./", "autopatcher_client");

            PatcherApp application;
            application.init(argc,argv);

            application.handle(NULL, FXSEL(SEL_COMMAND, PatcherApp::ID_PATCH), NULL);
        
            return application.run();
            
        } catch (Exception & e)
        {
            // dummy app to display message box
            FXApp app;
            app.init(argc, argv);
            app.create();
            FXMessageBox::error(&app, MBOX_OK, "Error", e.getMessage().c_str());
        }

        return 0;
    }



