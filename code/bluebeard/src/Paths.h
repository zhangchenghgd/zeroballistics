

#ifndef BLUEBEARD_PATHS_INCLUDED
#define BLUEBEARD_PATHS_INCLUDED

#include <string>

#include <boost/filesystem.hpp>

const std::string CONFIG_PATH           = "data/config/";
const std::string BASE_TEX_PATH         = "data/textures/";
const std::string MODEL_TEX_PATH        = "data/textures/models/";
const std::string HUD_TEXTURE_PATH      = "data/textures/hud/";
const std::string FONT_PATH             = "data/fonts/";
const std::string EFFECTS_PATH          = "data/effects/";
const std::string PARTICLE_EFFECT_PATH  = "data/particle_effects/";
const std::string PARTICLE_TEXTURE_PATH = "data/textures/particles/";
const std::string MODEL_PATH            = "data/models/";
const std::string SHADER_PATH           = "data/shaders/";
const std::string SOUND_PATH            = "data/sounds/";
const std::string MUSIC_PATH            = "data/sounds/music/";
const std::string LEVEL_PATH            = "data/levels/";


const std::string GUI_SCHEMES_PATH    = "data/gui/schemes/";
const std::string GUI_IMAGESETS_PATH  = "data/gui/imagesets/";
const std::string GUI_FONTS_PATH      = "data/fonts/";
const std::string GUI_LAYOUTS_PATH    = "data/gui/layouts/";
const std::string GUI_LOOKNFEEL_PATH  = "data/gui/looknfeel/";


extern boost::filesystem::path USER_DATA_SUBDIR;


std::string getOrCreateUserDataDir();


#endif
