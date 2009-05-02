
#include "ServerInfo.h"


#include <raknet/BitStream.h>



namespace network
{

namespace master
{


//------------------------------------------------------------------------------
ServerInfo::ServerInfo() :
    name_("??"),
    level_name_("??"),
    max_players_((uint8_t)-1),
    num_players_((uint8_t)-1),
    address_(UNASSIGNED_SYSTEM_ADDRESS),
    internal_port_(0)
{
}


//------------------------------------------------------------------------------    
void ServerInfo::writeToBitstream (RakNet::BitStream & stream) const
{
    writeString(stream, name_);
    writeString(stream, level_name_);

    writeString(stream, game_mode_);
    
    stream.Write(max_players_);
    stream.Write(num_players_);

    stream.Write(version_.type_);
    stream.Write(version_.major_);
    stream.Write(version_.minor_);

    stream.Write(address_);
    stream.Write(internal_port_);
}



//------------------------------------------------------------------------------
bool ServerInfo::readFromBitstream(RakNet::BitStream & stream)
{
    bool ret = true;
    
    ret &= readString(stream, name_);
    ret &= readString(stream, level_name_);

    ret &= readString(stream, game_mode_);

    ret &= stream.Read(max_players_);
    ret &= stream.Read(num_players_);

    ret &= stream.Read(version_.type_);
    ret &= stream.Read(version_.major_);
    ret &= stream.Read(version_.minor_);

    ret &= stream.Read(address_);
    ret &= stream.Read(internal_port_);

    return ret;
}


//------------------------------------------------------------------------------
/**
 *  String compressor doesn't work when communicating with openbsd ference??
 */
void ServerInfo::writeString(RakNet::BitStream & stream, std::string str)
{
    if (str.length() > 254) str.resize(254);
    stream.Write((uint8_t)str.length());

    if (str.empty()) return;
    
    stream.WriteAlignedBytes((unsigned char*)&str[0], str.length());
}



//------------------------------------------------------------------------------
/**
 *  String compressor doesn't work when communicating with openbsd ference??
 */
bool ServerInfo::readString (RakNet::BitStream & stream, std::string & str)
{
    bool ret = true;
    
    uint8_t len = 0;
    ret &= stream.Read(len);

    if(!len) return ret;

    str.resize(len);
    ret &= stream.ReadAlignedBytes((unsigned char*)&str[0], len);

    return ret;
}


//------------------------------------------------------------------------------ 
std::ostream & operator<<(std::ostream & out, const ServerInfo & info)
{
    out << info.address_.ToString()
        << ": "
        << info.name_
        << ", version "
        << info.version_
        << ", "
        << (unsigned)info.num_players_ << "/" << (unsigned)info.max_players_
        << " playing " << info.level_name_;
    return out;
}

    
}
}
