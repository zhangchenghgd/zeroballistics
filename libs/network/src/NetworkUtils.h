
#ifndef NETWORK_UTILS_INCLUDED
#define NETWORK_UTILS_INCLUDED


#include "utility_Math.h"  ///< used for _copysign -> copysign

#include <raknet/RakPeer.h>


#include "RegisteredFpGroup.h"

class Matrix;

std::istream & operator>>(std::istream & in, SystemAddress & id);
std::ostream & operator<<(std::ostream & out, const SystemAddress & id);


namespace network
{

void initializeSecurity(RakPeerInterface * iface, const std::string & keyfile, bool public_key);
    
void writeAngleToBitstream(RakNet::BitStream & stream, float angle);
void readAngleFromBitstream (RakNet::BitStream & stream, float & angle);

void writeToBitstream (RakNet::BitStream & stream, std::string str);
bool readFromBitstream(RakNet::BitStream & stream, std::string & str);

void writeToBitstream (RakNet::BitStream & stream, const Matrix & m, bool quantized);
bool readFromBitstream(RakNet::BitStream & stream,       Matrix & m, bool quantized);

void writeToBitstream (RakNet::BitStream & to_stream,   RakNet::BitStream & from_stream);
void readFromBitstream(RakNet::BitStream & from_stream, RakNet::BitStream & to_stream);




void defaultPacketAction(const Packet * packet, RakPeerInterface * rak_peer_interface);
void printPacketIds();
 

//------------------------------------------------------------------------------
class NetworkStatistics
{
 public:
    NetworkStatistics(RakPeerInterface * i);
 protected:

    void update(float dt);
    
    float kb_per_sec_sent_;
    float kb_per_sec_received_;

    RakNetStatistics last_stats_;

    RakPeerInterface * interface_;

    unsigned prev_received_;
    unsigned prev_sent_;

    RegisteredFpGroup fp_group_;
};



}

#endif
