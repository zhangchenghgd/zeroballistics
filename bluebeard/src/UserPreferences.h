
#ifndef BLUEBEARD_USER_PREFERENCES_INCLUDED
#define BLUEBEARD_USER_PREFERENCES_INCLUDED

#include <string>


const std::string CONFIG_SUPERSECTION              = "config";
const std::string DEFAULT_KEYMAP_SUPERSECTION      = "default_keymap";
const std::string CONFIGURABLE_KEYMAP_SUPERSECTION = "configurable_keymap";


std::string getUserConfigFile();
void redirectStdoutToFile();

#endif
