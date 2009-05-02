

#ifndef TANK_NETWORK_COMMAND_COMMON_INCLUDED
#define TANK_NETWORK_COMMAND_COMMON_INCLUDED

#include <map>
#include <deque>
#include <string>

#include <raknet/RakNetTypes.h>
#include <raknet/MessageIdentifiers.h>
#include <raknet/PacketPriority.h>
#include <raknet/RakNetDefines.h>

#include "MessageIds.h"

#include "TimeStructs.h"
#include "Datatypes.h"
#include "RegisteredFpGroup.h"
#include "Scheduler.h"
#include "NetworkUtils.h"


class PuppetMasterServer;
class PuppetMasterClient;
class RakPeerInterface;



#ifdef __BITSTREAM_NATIVE_END
#error __BITSTREAM_NATIVE_END DEFINED!
#endif


//------------------------------------------------------------------------------
namespace network
{ 

//------------------------------------------------------------------------------
enum TANK_PACKET_ID
{
    TPI_FIRST = FIRST_USER_PACKET_ID,
    
    TPI_CHAT = TPI_FIRST,

    TPI_LOAD_LEVEL,
    
    TPI_SET_PLAYER_NAME,

    TPI_CREATE_PLAYER,
    TPI_DELETE_PLAYER,

    TPI_SET_CONTROLLABLE,
    TPI_SET_CONTROLLABLE_STATE,
    
    TPI_CREATE_GAME_OBJECT,
    TPI_DELETE_GAME_OBJECT,
    TPI_REPLACE_GAME_OBJECT,
    
    TPI_SET_GAME_OBJECT_STATE_CORE,
    TPI_SET_GAME_OBJECT_STATE_EXTRA,
    TPI_SET_GAME_OBJECT_STATE_BOTH,
    
    TPI_PLAYER_INPUT,

    TPI_RESET_GAME,
    TPI_RCON_CMD,
    TPI_STRING_MESSAGE_CMD,
    TPI_SET_GAME_LOGIC_STATE_CMD,

    TPI_REQUEST_READY,
    TPI_READY,

    TPI_KICK,
    
    TPI_CUSTOM_SERVER_CMD,
    TPI_CUSTOM_CLIENT_CMD,
    
    TPI_LAST
};


//------------------------------------------------------------------------------ 
enum ACCOUNT_TYPE
{
    AT_INCOMING,
    AT_OUTGOING
};
 
const unsigned NUM_PACKET_TYPES = TPI_LAST - TPI_FIRST; 


//------------------------------------------------------------------------------ 
enum NETWORK_CHANNEL
{
    NC_DONTCARE,
    NC_CHAT,
    NC_GAMESTATE
};





//------------------------------------------------------------------------------
class NetworkCommand
{
 public:
    NetworkCommand();
    virtual ~NetworkCommand();

    static void initAccounting(RegisteredFpGroup * fp_group);
    static std::string printAndResetNetSummary(const std::vector<std::string>&);
    
 protected:

    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel) = 0;
                                   
    
    virtual void writeToBitstream (RakNet::BitStream & stream) = 0;
    virtual void readFromBitstream(RakNet::BitStream & stream) = 0;    



    
    static void accountPacket(RakNet::BitStream & stream, ACCOUNT_TYPE  type);
    
    static TimeValue accounting_start_time_;
    static unsigned num_bits_accounted_[2][NUM_PACKET_TYPES];
    static RegisteredFpGroup * fp_group_;
};


//------------------------------------------------------------------------------
/**
 *  A network command sent by the server and executed on the client.
 */
class NetworkCommandServer : public NetworkCommand
{
 public:

    NetworkCommandServer();
    
    void send(RakPeerInterface * iface,
              const SystemAddress & dest_id,
              bool broadcast);


    static NetworkCommandServer * createFromPacket(unsigned char packet_id, Packet * p, RakPeerInterface * rak_peer_interface);    
    virtual void execute(PuppetMasterClient * master) = 0;
};


//------------------------------------------------------------------------------
/**
 *  A network command sent by the client and executed on the server.
 */
class NetworkCommandClient : public NetworkCommand
{
 public:

    NetworkCommandClient(const SystemAddress & player_address = UNASSIGNED_SYSTEM_ADDRESS);
    
    void send(RakPeerInterface * iface);


    static NetworkCommandClient * createFromPacket(unsigned char packet_id, Packet * p, RakPeerInterface * rak_peer_interface);    
    virtual void execute(PuppetMasterServer * master) = 0;

    const SystemAddress & getPlayerAddress() const;
    
 protected:
    SystemAddress player_address_; ///< The player the command
                                   ///originated from.
};






//------------------------------------------------------------------------------
/**
 *  This type simply carries its id and no payload.
 */
 class SimpleCmd : public NetworkCommandServer, public NetworkCommandClient
{
 public:
    SimpleCmd();
    SimpleCmd(TANK_PACKET_ID id);

    using NetworkCommandServer::send;
    using NetworkCommandClient::send;
    
    virtual void execute(PuppetMasterClient * master);
    virtual void execute(PuppetMasterServer * master);

 protected:

    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);

    TANK_PACKET_ID id_;
};



std::vector<std::string> enumerateInterfaces();
bool isSameSubnet(const std::string & a1, const std::string & a2);
    



} // namespace network




//------------------------------------------------------------------------------
/**
 *  Returns the difference between a and b, taking into account wraparound.
 *
 *  (*) 30 - 25  = 5
 *  (*) 25 - 30  = -5
 *  (*) 0  - 255 = 1
 */
inline int seqNumberDifference(uint8_t a, uint8_t b)
{
    int v1 = (int) a      -  b;
    int v2 = (int)(a+256) -  b;
    int v3 = (int) a      - (b+256);

    // return smallest possible value.
    if (std::abs(v1) < std::abs(v2))
    {
        return std::abs(v1) < std::abs(v3) ? v1 : v3;
    } else
    {
        return std::abs(v2) < std::abs(v3) ? v2 : v3;
    }
}



#endif // TANK_NETWORK_COMMAND_COMMON_INCLUDED
