

#include "Paths.h"


#include <iostream>

#include <boost/filesystem.hpp>

#ifdef _WIN32
#include <shlobj.h>
#else
#include <sys/types.h>
#include <pwd.h>
#endif


#include "Exception.h"


const boost::filesystem::path DATA_SUBDIR = "Quanticode/Zero Ballistics/";

//------------------------------------------------------------------------------
std::string getOrCreateUserDataDir()
{
    using namespace boost::filesystem;

    static path data_dir;
    if (!data_dir.empty()) return data_dir.string();
    
    path base_dir;
    
#ifdef _WIN32

    TCHAR app_data_path[MAX_PATH];

    // get user specific app data dir
    if(SUCCEEDED(SHGetFolderPath(NULL, 
                                 CSIDL_APPDATA, 
                                 NULL, 
                                 0, 
                                 app_data_path))) 
    {
        base_dir = app_data_path;
    }
    else
    {
        base_dir = "/";
    }

#else
    char * xdg_config_home = getenv("XDG_CONFIG_HOME");
    if (xdg_config_home)
    {
        // Respect XDG_CONFIG_HOME if it is set
        base_dir = xdg_config_home;
    } else
    {
        // else use $HOME/.config
        passwd * pwd = getpwuid(getuid());
        if (pwd)
        {
            base_dir = path(pwd->pw_dir) / ".config";
        } else
        {
            std::cout << "Could not locate home directory. Using temporary directory for settings.\n";
            char tmp_dir[] = "/tmp/zeroXXXXXX";
            if (mkdtemp(tmp_dir))
            {
                base_dir = tmp_dir;
            } else
            {
                std::cout << "Could not create temporary directory.\n";
                base_dir = "/";
            }
        }
    }
#endif


    try
    {
        data_dir = base_dir / DATA_SUBDIR;
        create_directories(data_dir);
    } catch (basic_filesystem_error<path> & )
    {
        Exception e("Could not create user data directory \"");
        e << data_dir
          << "\"";
        throw e;
    }
    
    std::cout << "User data directory is \""
              << data_dir.string()
              << "\"\n";
    
    return data_dir.string();
}
