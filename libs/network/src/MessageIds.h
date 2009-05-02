

#ifndef NETWORK_MESSAGE_IDS_INCLDUED
#define NETWORK_MESSAGE_IDS_INCLDUED

#include <raknet/MessageIdentifiers.h>

namespace network
{

namespace master
{



//------------------------------------------------------------------------------
enum MASTER_PACKET_ID
{
    MPI_REQUEST_AUTH_TOKEN = ID_USER_PACKET_ENUM,
    MPI_AUTH_TOKEN,
    
    // periodically must be sent by the game server to stay in the list. contains complete server info.
    MPI_HEARTBEAT,
    MPI_REMOVE_SERVER,

    
    MPI_REQUEST_SERVER_LIST, // sent by client to request the list of servers
    MPI_REQUEST_NAT_PUNCHTHROUGH, // client sends this to master
                                  // server, together with server
                                  // address he wants to connect to.
    

    MPI_CUSTOM_MESSAGE,
    MPI_SERVER_LIST,         // master answer
    
    MPI_LAST
};


} // namespace master



    
namespace ranking
{




//------------------------------------------------------------------------------
enum RANKING_PACKET_ID
{
    RPI_CREDENTIALS = network::master::MPI_LAST,
    RPI_AUTHORIZATION_FAILED,
    RPI_SESSION_KEY,

    RPI_REQUEST_MATCH_ID, /// register a match.
    RPI_MATCH_ID,         /// ranking server answer, contains registered match id

    RPI_GAME_STATS,       /// Match statistics.
    RPI_STATS_REJECTED,
    RPI_STATS_ACK,
    
    RPI_LAST
};


} // namespace ranking



//------------------------------------------------------------------------------
enum VERSION_HANDSHAKE_PACKET_ID
{
    VHPI_VERSION_INFO_INTERNAL = ranking::RPI_LAST, // (internal) packet contains the version information
    VHPI_VERSION_ACK,                               // (internal) peer's version is accepted.
    VHPI_VERSION_MISMATCH,
    VHPI_TYPE_MISMATCH,
    VHPI_VERSION_INFO,
    
    VHPI_LAST
};


const unsigned FIRST_USER_PACKET_ID = VHPI_LAST;

} // namespace network

#endif
