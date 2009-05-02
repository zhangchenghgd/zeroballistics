

#include "UserPreferences.h"


#include <boost/filesystem.hpp>

#include "Paths.h"
#include "ParameterManager.h"


/// XXX this is app specific, needs to be moved from engine to app
/// how to get user config file name? s_params , static var?
const std::string USER_CONFIG_FILE          = "config.xml";
const std::string USER_CONFIG_TEMPLATE_FILE = "user_config_template.xml";




//------------------------------------------------------------------------------
/**
 *  Test whether the user config file already exists. If not, copy the
 *  template config file.
 *  Finally, load the config section from the user config file.
 *  Redirect stdout to file
 */
void initUserConfigFile()
{
    using namespace boost::filesystem;
    path user_config_filepath;
    try
    {
        user_config_filepath = path(getOrCreateUserDataDir()) / USER_CONFIG_FILE;        
        if (!exists(user_config_filepath))
        {
            std::cout << "Creating user config file "
                      << user_config_filepath
                      << "\n";
            copy_file(path(CONFIG_PATH) / USER_CONFIG_TEMPLATE_FILE, user_config_filepath);
        }
    } catch (basic_filesystem_error<path> & be)
    {
        Exception e("Unable to save user preferences in ");
        e << user_config_filepath << ".";
        throw e;
    }
}



//------------------------------------------------------------------------------
/**
 *  Return either the location of the user config file or the template
 *  file if it doesn't exist.
 */
std::string getUserConfigFile()
{
    using namespace boost::filesystem;
    path user_config_filepath;    
    try
    {
         user_config_filepath = path(getOrCreateUserDataDir()) / USER_CONFIG_FILE;
        
        if (!exists(user_config_filepath)) initUserConfigFile();
        if (!exists(user_config_filepath)) throw Exception("Could not find user settings.");
    } catch (basic_filesystem_error<path> & be)
    {
        throw Exception("Could not find user settings.");
    }

    return user_config_filepath.string();
}

//------------------------------------------------------------------------------
void redirectStdoutToFile()
{
    using namespace boost::filesystem;

#ifdef _WIN32
#ifndef _DEBUG
    // redirect stdout to file, if this fails direct to console
    if(!freopen((path(getOrCreateUserDataDir()) / "stdout.txt").string().c_str(), "w", stdout)) freopen("CON", "w", stdout);
    if(!freopen((path(getOrCreateUserDataDir()) / "stderr.txt").string().c_str(), "w", stderr)) freopen("CON", "w", stderr);
#endif
#endif
}
