
#ifndef TOOLBOX_VERSION_INFO_INCLUDED
#define TOOLBOX_VERSION_INFO_INCLUDED

#include "Datatypes.h"

//------------------------------------------------------------------------------
class VersionInfo
{
 public:

    VersionInfo() : game_(0), major_(0), minor_(0) {}
    
    VersionInfo(uint8_t g, uint8_t major, uint8_t minor) :
        game_(g), major_(major), minor_(minor) {}
    
    uint8_t game_;
    uint8_t major_;
    uint8_t minor_;

    bool operator==(const VersionInfo & other) const 
        {
            return (game_  == other.game_  &&
                    major_ == other.major_ &&
                    minor_ == other.minor_);
        }

    bool operator!=(const VersionInfo & other) const { return !(*this == other); }
};


//------------------------------------------------------------------------------
inline std::ostream & operator<<(std::ostream & out, const VersionInfo & info)
{
    out << info.game_ << (unsigned)info.major_ << "." << std::setw(2) << std::setfill('0') << (unsigned)info.minor_;
    return out;
}


// must be defined by the game
extern VersionInfo g_version;

#endif
