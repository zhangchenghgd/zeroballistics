
#include "VersionInfo.h"


#include <raknet/BitStream.h>

//------------------------------------------------------------------------------
VersionInfo::VersionInfo() : type_(0), major_(0), minor_(0)
{
}
    
//------------------------------------------------------------------------------
VersionInfo::VersionInfo(uint8_t t, uint8_t major, uint8_t minor) :
    type_(t), major_(major), minor_(minor)
{
}


//------------------------------------------------------------------------------
void VersionInfo:: writeToBitstream (RakNet::BitStream & stream) const
{
    stream.Write(type_);
    stream.Write(major_);
    stream.Write(minor_);
}


//------------------------------------------------------------------------------
bool VersionInfo::readFromBitstream(RakNet::BitStream & stream)
{
    bool ret = true;
    
    ret &= stream.Read(type_);
    ret &= stream.Read(major_);
    ret &= stream.Read(minor_);

    return ret;
}



//------------------------------------------------------------------------------
bool VersionInfo::operator==(const VersionInfo & other) const 
{
    return (type_  == other.type_  &&
            major_ == other.major_ &&
            minor_ == other.minor_);
}

//------------------------------------------------------------------------------
bool VersionInfo::operator!=(const VersionInfo & other) const
{
    return !(*this == other);
}


//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const VersionInfo & info)
{
    out << info.type_
        << (unsigned)info.major_
        << "." << std::setw(2) << std::setfill('0') << (unsigned)info.minor_;
    return out;
}

