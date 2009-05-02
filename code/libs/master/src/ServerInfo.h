
#ifndef MASTER_SERVER_SERVER_INFO_INCLUDED
#define MASTER_SERVER_SERVER_INFO_INCLUDED

#include <string>

#include <raknet/RakNetTypes.h>

#include "Datatypes.h"
#include "VersionInfo.h"

namespace network
{

namespace master
{



 
//------------------------------------------------------------------------------
class ServerInfo
{
 public:
    ServerInfo();
    
    void writeToBitstream (RakNet::BitStream & stream) const;
    bool readFromBitstream(RakNet::BitStream & stream);
    

    std::string name_;      ///< A descriptive name which can be chosen by the server admin.
    std::string level_name_;

    std::string game_mode_;


    uint8_t max_players_;
    uint8_t num_players_;

    VersionInfo version_;
    
    SystemAddress address_;
    uint16_t internal_port_; ///< Port the server has in internal
                             ///network. Needed for direct connection
                             ///attempt, as this port is not
                             ///neccessarily the one seen by the
                             ///master server.

    bool operator==(const SystemAddress & ad) { return address_ == ad; }
    

    // XXXX hack because string functions don't work on ference
    static void writeString(RakNet::BitStream & stream, std::string str);
    static bool readString (RakNet::BitStream & stream, std::string & str);
};

std::ostream & operator<<(std::ostream & out, const ServerInfo & info);
 
 
} // namespace master    


}
#endif
