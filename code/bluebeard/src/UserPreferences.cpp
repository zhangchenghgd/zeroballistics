

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
 *  Makes sure that all parameters from USER_CONFIG_FILE are present
 *  in the user config file, then returns the path to it.
 *
 *
 *  This entire function is basically a single hack. For every super
 *  section in the user config file:
 *
 *  -) Load template into params
 *  -) load current config into params
 *  -) copy template to make sure everything of interest is present in the user config file
 *  -) save params to copied file
 *
 *  This way, new params are added and old ones removed.
 */
std::string initUserConfigFile()
{
    using namespace boost::filesystem;

    path user_config_filepath = path(getOrCreateUserDataDir()) / USER_CONFIG_FILE;
    
    try
    {
        // XXXX super sections must be enumerated here
        std::vector<std::string> super_section;
        super_section.push_back("config");
        super_section.push_back("default_keymap");
        super_section.push_back("configurable_keymap");

        LocalParameters params;
    
        for (unsigned ss=0; ss<super_section.size(); ++ss)
        {
            params.loadParameters((path(CONFIG_PATH) / USER_CONFIG_TEMPLATE_FILE).string(), super_section[ss]);
            if (exists(user_config_filepath))
            {
                params.loadParameters(user_config_filepath.string(), super_section[ss]);
            }
        }

        // copy doesn't overwrite...  For dev mode, don't replace our
        // custom config.xml, so need to manually add new variables
#ifdef ENABLE_DEV_FEATURES
        try
        {
            copy_file(path(CONFIG_PATH) / USER_CONFIG_TEMPLATE_FILE, user_config_filepath);
        } catch(basic_filesystem_error<path> & be) {}
#else
        remove(user_config_filepath);
        copy_file(path(CONFIG_PATH) / USER_CONFIG_TEMPLATE_FILE, user_config_filepath);
#endif
    
        for (unsigned ss=0; ss<super_section.size(); ++ss)
        {
            params.saveParameters((user_config_filepath).string(), super_section[ss]);
        }

        return user_config_filepath.string();
        
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
std::string getUserConfigFile(bool init)
{
    if (init) return initUserConfigFile();
    
    using namespace boost::filesystem;
    path user_config_filepath;    
    try
    {
         user_config_filepath = path(getOrCreateUserDataDir()) / USER_CONFIG_FILE;

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
