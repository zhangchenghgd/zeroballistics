

#include "NetworkCommand.h"


// interface enumeration stuff
#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#endif




#include <raknet/RakPeerInterface.h>
#include <raknet/RakNetworkFactory.h>
#include <raknet/GetTime.h>
#include <raknet/RakNetStatistics.h>

#include <loki/static_check.h>

#include "Log.h"

// For factory
#include "NetworkCommandClient.h"
#include "NetworkCommandServer.h"

#include "ParameterManager.h"

#include "Matrix.h"


//------------------------------------------------------------------------------
namespace network
{

TimeValue NetworkCommand::accounting_start_time_;
unsigned NetworkCommand::num_bits_accounted_[2][NUM_PACKET_TYPES];
RegisteredFpGroup * NetworkCommand::fp_group_ = NULL;

    
//------------------------------------------------------------------------------
/**
 *  Used for network traffic reporting
 */
const char * TANK_PACKET_NAME[] =
{
    "TPI_CHAT                                 ",
    
    "TPI_LOAD_LEVEL                           ",

    "TPI_SET_PLAYER_NAME                      ",

    "TPI_CREATE_PLAYER                        ",
    "TPI_DELETE_PLAYER                        ",

    "TPI_SET_CONTROLLABLE                     ",
    "TPI_SET_CONTROLLABLE_STATE               ",
    
    "TPI_CREATE_GAME_OBJECT                   ",
    "TPI_DELETE_GAME_OBJECT                   ",
    "TPI_REPLACE_GAME_OBJECT                  ",
    
    "TPI_SET_GAME_OBJECT_STATE_CORE           ",
    "TPI_SET_GAME_OBJECT_STATE_EXTRA          ",
    "TPI_SET_GAME_OBJECT_STATE_BOTH           ",
    
    "TPI_PLAYER_INPUT                         ",

    "TPI_RESET_GAME                           ",
    "TPI_RCON_CMD                             ",
    "TPI_STRING_MESSAGE_CMD                   ",
    "TPI_SET_GAME_LOGIC_STATE_CMD             ",

    "TPI_REQUEST_READY                        ",
    "TPI_READY                                ",

    "TPI_KICK                                 ",

    "TPI_CUSTOM_SERVER_CMD                    ",
    "TPI_CUSTOM_CLIENT_CMD                    "             
};



    
//------------------------------------------------------------------------------
NetworkCommand::NetworkCommand()
{
    LOKI_STATIC_CHECK(NUM_PACKET_TYPES == sizeof(TANK_PACKET_NAME)/sizeof(char*),
                      update_tank_packet_names);
}


//------------------------------------------------------------------------------
NetworkCommand::~NetworkCommand()
{
}


//------------------------------------------------------------------------------
void NetworkCommand::initAccounting(RegisteredFpGroup * fp_group)
{
    fp_group_ = fp_group;
    getCurTime(accounting_start_time_);
    s_console.addFunction("printAndResetNetSummary",
                          ConsoleFun(&NetworkCommand::printAndResetNetSummary),
                          fp_group_);
}

//------------------------------------------------------------------------------
std::string NetworkCommand::printAndResetNetSummary(const std::vector<std::string>&)
{
    TimeValue cur_time;
    getCurTime(cur_time);

    float passed_secs = getTimeDiff(cur_time, accounting_start_time_) / 1000.0f;
    
    for (unsigned type=0; type<2; ++type)
    {
        s_log << "\n\n";
        if (type==0)
        {
            s_log << std::string("Total incoming traffic:\n");
        } else
        {
            s_log << std::string("Total outgoing traffic:\n");
        }

        // First calc total
        float total = 0;
        for (unsigned i=0; i< NUM_PACKET_TYPES; ++i)
        {
            float kb = (float)num_bits_accounted_[type][i]/8/1024;        
            total += kb;
        }

        if (total == 0)
        {
            s_log << "no network traffic generated\n";
            continue;
        }
    
        for (unsigned i=0; i< NUM_PACKET_TYPES; ++i)
        {
            float kb = (float)num_bits_accounted_[type][i]/8/1024;
            num_bits_accounted_[type][i] = 0;

            s_log << TANK_PACKET_NAME[i]
                  << " : "
                  << std::setprecision(2) << std::setw(8) << kb / passed_secs
                  << " kB/s ("
                  << std::setprecision(2) << std::setw(8) << kb
                  << " kB total, "
                  << std::setprecision(2) << std::setw(8) << kb / total * 100.0f
                  << " %)\n";
        }

        s_log << "\n"
              << total / passed_secs
              << " kB/s on average ("
              << total
              << " kB total in "
              << passed_secs
              << " secs).\nThat's "
              << 1024.0f / total * passed_secs
              << " seconds for one megabyte.\n";
    }

    accounting_start_time_ = cur_time;
    
    return "";
}



//------------------------------------------------------------------------------
void NetworkCommand::accountPacket(RakNet::BitStream & stream, ACCOUNT_TYPE type)
{
    stream.ResetReadPointer();

    uint8_t packet_id;    
    stream.Read(packet_id);
    if (packet_id == ID_TIMESTAMP)
    {
        uint32_t ts;
        stream.Read(ts);
        stream.Read(packet_id);
    }


    num_bits_accounted_[type][packet_id - TPI_FIRST] += stream.GetNumberOfBitsUsed();

    stream.ResetReadPointer();
}


//------------------------------------------------------------------------------
NetworkCommandServer::NetworkCommandServer()
{
}


//------------------------------------------------------------------------------
void NetworkCommandServer::send(RakPeerInterface * iface,
                                const SystemAddress & dest_id,
                                bool broadcast)
{
    assert(broadcast || (dest_id != UNASSIGNED_SYSTEM_ADDRESS));

    RakNet::BitStream stream;
    writeToBitstream(stream);

    if (stream.GetNumberOfBitsUsed() == 0) return;

    PacketReliability r;
    PacketPriority p;
    unsigned c;
    getNetworkOptions(r,p,c);
    
    if (!iface->Send(&stream, p, r, c, dest_id, broadcast))
    {
//         s_log << Log::warning
//               << "Failed to send a message in NetworkCommandServer::send()\n";
    }

    accountPacket(stream, AT_OUTGOING);
}


//------------------------------------------------------------------------------
/**
 *  Factory method to build command objects from input packets.
 *  Resulting command objects have to be deleted by the user.
 *
 *  Returns NULL for unknown / unexpected packets.
 */
NetworkCommandServer * NetworkCommandServer::createFromPacket(unsigned char packet_id,
                                                              Packet * p,
                                                              RakPeerInterface * rak_peer_interface)
{

    NetworkCommandServer * ret = NULL;

    switch (packet_id)
    {        
    case TPI_LOAD_LEVEL:
        ret = new LoadLevelCmd;
        break;
        
    case TPI_CREATE_PLAYER:
        ret = new CreatePlayerCmd;
        break;
    case TPI_DELETE_PLAYER:
        ret = new DeletePlayerCmd;
        break;

        
    case TPI_SET_CONTROLLABLE:
        ret = new SetControllableCmd;
        break;
    case TPI_SET_CONTROLLABLE_STATE:
        ret = new SetControllableStateCmd;
        break;

    case TPI_CREATE_GAME_OBJECT:
        ret = new CreateGameObjectCmd;
        break;
    case TPI_DELETE_GAME_OBJECT:
        ret = new DeleteGameObjectCmd;
        break;
    case TPI_REPLACE_GAME_OBJECT:
        ret = new ReplaceGameObjectCommand;
        break;
    case TPI_SET_GAME_OBJECT_STATE_CORE:
    case TPI_SET_GAME_OBJECT_STATE_EXTRA:
    case TPI_SET_GAME_OBJECT_STATE_BOTH:
        ret = new SetGameObjectStateCmd();
        break;  
    case TPI_STRING_MESSAGE_CMD:
        ret = new StringMessageCmd();
        break;
        
    case TPI_CUSTOM_SERVER_CMD:
        ret = new CustomServerCmd();
        break;
    }

    if (!ret)
    {
        defaultPacketAction(p, rak_peer_interface);
        return NULL;
    }
    
    
    try
    {
        RakNet::BitStream stream((unsigned char*)p->data, p->length, false);
        accountPacket(stream, AT_INCOMING);
        ret->readFromBitstream(stream);
    } catch (const Exception & e)
    {
        s_log << Log::warning << "Ignoring packet with id " << (unsigned)packet_id
              << " but unexpected size from " << p->systemAddress << ".\n";
        return NULL;
    }
    
    return ret;
}


//------------------------------------------------------------------------------
NetworkCommandClient::NetworkCommandClient(const SystemAddress & address) :
    player_address_(address)
{
}


//------------------------------------------------------------------------------
void NetworkCommandClient::send ( RakPeerInterface * iface)
{
    RakNet::BitStream stream;
    writeToBitstream(stream);

    if (stream.GetNumberOfBitsUsed() == 0) return;
    
    PacketReliability r;
    PacketPriority p;
    unsigned c;
    getNetworkOptions(r,p,c);
    
    if (!iface->Send(&stream, p, r, c, UNASSIGNED_SYSTEM_ADDRESS, true))
    {
        s_log << Log::warning
              << "Failed to send a message in NetworkCommandClient::send\n";
    }

    accountPacket(stream, AT_OUTGOING);
}



//------------------------------------------------------------------------------
/**
 *  Factory method to build command objects from input packets.
 *  Resulting command objects have to be deleted by the user.
 *
 *  Returns NULL for unknown / unexpected packets.
 */
NetworkCommandClient * NetworkCommandClient::createFromPacket(unsigned char packet_id,
                                                              Packet * p,
                                                              RakPeerInterface * rak_peer_interface)
{

    NetworkCommandClient * ret = NULL;

    switch (packet_id)
    {
    case TPI_CHAT:
        ret = new ChatCmd(p->systemAddress);
        break;

    case TPI_SET_PLAYER_NAME:
        ret = new SetPlayerDataCmd(p->systemAddress);
        break;

    case TPI_PLAYER_INPUT:
        ret = new PlayerInputCmd(p->systemAddress);
        break;
    case TPI_RCON_CMD:
        ret = new RconCmd(p->systemAddress);
        break;

    case TPI_CUSTOM_CLIENT_CMD:
        ret = new CustomClientCmd(p->systemAddress);
        break;
    }

    if (!ret)
    {
        defaultPacketAction(p, rak_peer_interface);
        return NULL;
    }
    
    
    try
    {
        RakNet::BitStream stream((unsigned char*)p->data, p->length, false);
        accountPacket(stream, AT_INCOMING);        
        ret->readFromBitstream(stream);
    } catch (const Exception & e)
    {
        s_log << Log::warning << "Ignoring packet with id " << (unsigned)packet_id
              << " but unexpected size from " << p->systemAddress << ".\n";
        return NULL;
    }
    
    return ret;
}



//------------------------------------------------------------------------------
const SystemAddress & NetworkCommandClient::getPlayerAddress() const
{
    return player_address_;
}


//******************** Commands issued by both client and server ********************//




//********** SimpleCmd **********//

//------------------------------------------------------------------------------
SimpleCmd::SimpleCmd()
{
}

//------------------------------------------------------------------------------
SimpleCmd::SimpleCmd(TANK_PACKET_ID id) : id_(id)
{
}


//------------------------------------------------------------------------------
void SimpleCmd::execute(PuppetMasterClient * master)
{
    s_log << Log::error << "Executing SimpleCmd\n";
}

//------------------------------------------------------------------------------
void SimpleCmd::execute(PuppetMasterServer * master)
{
    s_log << Log::error << "Executing SimpleCmd\n";
}

//------------------------------------------------------------------------------
void SimpleCmd::getNetworkOptions(PacketReliability & reliability,
                                  PacketPriority    & priority,
                                  unsigned          & channel)
{
    reliability = RELIABLE_ORDERED;
    priority    = MEDIUM_PRIORITY;
    channel     = NC_GAMESTATE;
}

//------------------------------------------------------------------------------
void SimpleCmd::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write((char)id_);
}

//------------------------------------------------------------------------------
void SimpleCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    stream.Read(packet_id);
    id_ = (TANK_PACKET_ID)packet_id;
}
    


//------------------------------------------------------------------------------
/**
 *  Returns an array of IP addresses corresponding to the computer's
 *  network interfaces.
 */
std::vector<std::string> enumerateInterfaces()
{
    std::vector<std::string> ret;
    
#ifdef _WIN32

    SOCKET sd = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
    if (sd == SOCKET_ERROR) 
    {
        s_log << Log::error << "Failed to get a socket. error: " << WSAGetLastError();
        return ret;
    }

    INTERFACE_INFO InterfaceList[50];
    unsigned long nBytesReturned;
    if (WSAIoctl(sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList,
		sizeof(InterfaceList), &nBytesReturned, 0, 0) == SOCKET_ERROR) 
    {
        s_log << Log::error << "Failed calling WSAIoctl. error: " << WSAGetLastError();

        closesocket (sd);   ///< close the opened socket
        sd = INVALID_SOCKET;
        
        return ret;
    }

    int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);

    // iterate over interfaces found
    for (int i = 0; i < nNumInterfaces; ++i) 
    {
        sockaddr_in *pAddress;
        pAddress = (sockaddr_in *) & (InterfaceList[i].iiAddress);
        std::string dotted_ip = inet_ntoa(pAddress->sin_addr);

        if (dotted_ip.find("127") == 0)
        {
            s_log << Log::debug('m')
                  << "Ignoring local interface\n";
            continue;
        }
        
        ret.push_back(dotted_ip);

        /* 
        /// Additional information on the interface, currently not needed
        pAddress = (sockaddr_in *) & (InterfaceList[i].iiBroadcastAddress);
        std::cout << " has bcast " << inet_ntoa(pAddress->sin_addr);

        pAddress = (sockaddr_in *) & (InterfaceList[i].iiNetmask);
        std::cout << " and netmask " << inet_ntoa(pAddress->sin_addr) << std::endl;

        std::cout << " Iface is ";
        u_long nFlags = InterfaceList[i].iiFlags;
        if (nFlags & IFF_UP) std::cout << "up";
        else                 std::cout << "down";
        if (nFlags & IFF_POINTTOPOINT) std::cout << ", is point-to-point";
        if (nFlags & IFF_LOOPBACK)     std::cout << ", is a loopback iface";
        std::cout << ", and can do: ";
        if (nFlags & IFF_BROADCAST) std::cout << "bcast ";
        if (nFlags & IFF_MULTICAST) std::cout << "multicast ";
        std::cout << std::endl;
        */
    }

    closesocket (sd);   ///< close the opened socket
    sd = INVALID_SOCKET;

#else
/* Get a socket handle. */
    int sck = socket(AF_INET, SOCK_DGRAM, 0);
    if(sck < 0)
    {
        s_log << Log::error
              << "Couldn't create socket\n";
        return ret;
    }

/* Query available interfaces. */
    char    buf[1024];
    ifconf  ifc;
    
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if(ioctl(sck, SIOCGIFCONF, &ifc) < 0)
    {
        s_log << Log::error
              << "Couldn't enumerate interfaces\n";
        close(sck);
        return ret;
    }

/* Iterate through the list of interfaces. */
    ifreq * ifr             = ifc.ifc_req;
    unsigned num_interfaces = ifc.ifc_len / sizeof(struct ifreq);
    for(unsigned i = 0; i < num_interfaces; ++i)
    {
        ifreq *item = &ifr[i];

        std::string dotted_ip = inet_ntoa(((struct sockaddr_in *)&item->ifr_addr)->sin_addr);
        
	/* Show the device name and IP address */
        s_log << Log::debug('m')
              << "Found network interface: "
              << item->ifr_name
              << ": "
              << dotted_ip
              << "\n";

        if (dotted_ip.find("127") == 0)
        {
            s_log << Log::debug('m')
                  << "Ignoring local interface\n";
            continue;
        }
        
        ret.push_back(dotted_ip);
    }

    close(sck);
    
#endif
    
    return ret;
}


//------------------------------------------------------------------------------
bool isSameSubnet(const std::string & a1, const std::string & a2)
{
    unsigned net_mask = 0x00ffffff; // XXXXX assume class C network for now....


    SystemAddress a1_bin;
    SystemAddress a2_bin;
    a1_bin.SetBinaryAddress(a1.c_str());
    a2_bin.SetBinaryAddress(a2.c_str());

    return ((a1_bin.binaryAddress & net_mask) ==
            (a2_bin.binaryAddress & net_mask));
}



} // namespace network


