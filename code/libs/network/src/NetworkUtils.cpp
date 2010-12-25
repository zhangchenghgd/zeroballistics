

#include "NetworkUtils.h"

#include <fstream>

#include <raknet/GetTime.h>
#include <raknet/StringCompressor.h>
#include <raknet/MessageIdentifiers.h>


#include "Scheduler.h"
#include "Console.h"
#include "Matrix.h"
#include "Log.h"
#include "MessageIds.h"
#include "VersionInfo.h"


const float UPDATE_NET_STATS_DT = 0.8f;

    
//------------------------------------------------------------------------------
std::istream & operator>>(std::istream & in, SystemAddress & id)
{
    assert(0);

    return in;
}



//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const SystemAddress & id)
{
    out << id.ToString();
    return out;
}



namespace network
{


//------------------------------------------------------------------------------
/**
 *  Reads either public or private key from the given file and
 *  initializes raknet security.
 */
/* TODO CM fix security
void initializeSecurity(RakPeerInterface * iface,
                        const std::string & keyfile,
                        bool public_key)
{
    std::ifstream in(keyfile.c_str(), std::ios_base::binary);

    if (!in)
    {
        throw Exception("Could not read key file " + keyfile);
    }
    

    if (public_key)
    {
	big::u32 e;
	RSA_BIT_SIZE n;

        in.read((char*)&e, sizeof(e));
        in.read((char*)&n, sizeof(n));

        if (!in)
        {
            throw Exception("Failed to read public key " + keyfile);
        }
            
        iface->InitializeSecurity((const char*)&e, (const char*)&n, NULL, NULL);
    } else
    {
        BIGHALFSIZE(RSA_BIT_SIZE, p);
        BIGHALFSIZE(RSA_BIT_SIZE, q);

        in.read((char*)&p, sizeof(p));
        in.read((char*)&q, sizeof(q));

        if (!in)
        {
            throw Exception("Failed to read private key " + keyfile);
        }
        
        iface->InitializeSecurity(NULL, NULL, (const char*)&p, (const char*)&q);
    }
}
*/

//------------------------------------------------------------------------------
void writeAngleToBitstream(RakNet::BitStream & stream, float angle)
{
    angle = normalizeAngle(angle);
    angle -= PI;
    stream.WriteCompressed(angle / PI);
}


//------------------------------------------------------------------------------
void readAngleFromBitstream (RakNet::BitStream & stream, float & angle)
{
    stream.ReadCompressed(angle);
    angle *= PI;
    angle += PI;
}



//------------------------------------------------------------------------------
void writeToBitstream(RakNet::BitStream & stream, std::string str)
{
    if (str.length() > 254) str.resize(254);
    stream.Write((uint8_t)str.length());
    StringCompressor::Instance()->EncodeString(str.c_str(), str.length()+1, &stream);




    
/* cleartext variant
    if (str.length() > 254) str.resize(254);
    stream.Write((uint8_t)str.length());

    if (str.empty()) return;
    
    stream.WriteAlignedBytes((unsigned char*)&str[0], str.length());

*/
}


//------------------------------------------------------------------------------
bool readFromBitstream(RakNet::BitStream & stream, std::string & str)
{
    uint8_t len;
    stream.Read(len);
    std::vector<char> tmp(len+1);
    if (!StringCompressor::Instance()->DecodeString(&tmp[0], len+1, &stream)) return false;
    str = &tmp[0];

    return true;




/* cleartext variant    
    bool ret = true;
    
    uint8_t len = 0;
    ret &= stream.Read(len);

    if(!len) return ret;

    str.resize(len);
    ret &= stream.ReadAlignedBytes((unsigned char*)&str[0], len);

    return ret;
*/
}

//------------------------------------------------------------------------------
void writeToBitstream (RakNet::BitStream & stream, const Matrix & m, bool quantized)
{
    if (!quantized)
    {
        stream.Write(m._11);
        stream.Write(m._12);
        stream.Write(m._13);
        stream.Write(m._14);

        stream.Write(m._21);
        stream.Write(m._22);
        stream.Write(m._23);
        stream.Write(m._24);

        stream.Write(m._31);
        stream.Write(m._32);
        stream.Write(m._33);
        stream.Write(m._34);

        stream.Write(m._41);
        stream.Write(m._42);
        stream.Write(m._43);
        stream.Write(m._44);
    } else
    {
        stream.WriteOrthMatrix(m._11, m._21, m._31,
                               m._12, m._22, m._32,
                               m._13, m._23, m._33);
        stream.WriteVector(m._14,m._24,m._34);
    }
}


//------------------------------------------------------------------------------
bool readFromBitstream(RakNet::BitStream & stream, Matrix & m, bool quantized)
{
    if (!quantized)
    {
        stream.Read(m._11);
        stream.Read(m._12);
        stream.Read(m._13);
        stream.Read(m._14);

        stream.Read(m._21);
        stream.Read(m._22);
        stream.Read(m._23);
        stream.Read(m._24);

        stream.Read(m._31);
        stream.Read(m._32);
        stream.Read(m._33);
        stream.Read(m._34);

        stream.Read(m._41);
        stream.Read(m._42);
        stream.Read(m._43);
        stream.Read(m._44);
    } else
    {
        if (!stream.ReadOrthMatrix(m._11, m._21, m._31,
                                   m._12, m._22, m._32,
                                   m._13, m._23, m._33)) return false;
        if (!stream.ReadVector(m._14,m._24,m._34)) return false;

        m._41 = m._42 = m._43 = 0.0f;
        m._44 = 1.0f;
    }
    
    return true;
}

//------------------------------------------------------------------------------
void writeToBitstream(RakNet::BitStream & to_stream, RakNet::BitStream & from_stream)
{
    uint32_t used_bytes = (uint32_t)from_stream.GetNumberOfBytesUsed();
    
    to_stream.Write(used_bytes);

    if (used_bytes)
    {
        to_stream.WriteAlignedBytes(from_stream.GetData(),
                                    from_stream.GetNumberOfBytesUsed());
    }
}

//------------------------------------------------------------------------------
void readFromBitstream(RakNet::BitStream & read_stream, RakNet::BitStream & write_stream)
{
    uint32_t num_bytes;
    read_stream.Read(num_bytes);

    if (num_bytes)
    {
        std::vector<uint8_t> buf(num_bytes);
        read_stream.ReadAlignedBytes(&buf[0], num_bytes);

        write_stream.WriteAlignedBytes(&buf[0], num_bytes);
    }
}


//------------------------------------------------------------------------------
void defaultPacketAction(const Packet * packet, RakPeerInterface * rak_peer_interface)
{
    s_log << Log::millis << " ";
    
    switch (packet->data[0])
    {
    case ID_NEW_INCOMING_CONNECTION:
        s_log << "ID_NEW_INCOMING_CONNECTION from "
              << packet->systemAddress
              << ". Connections: "
              << rak_peer_interface->NumberOfConnections()
              << "\n";
        break;
    case ID_DISCONNECTION_NOTIFICATION:
        s_log << "ID_DISCONNECTION_NOTIFICATION from "
              << packet->systemAddress
              << ". Connections: "
              << rak_peer_interface->NumberOfConnections()
              << "\n";
        break;
    case ID_CONNECTION_LOST:
        s_log << "ID_CONNECTION_LOST from "
              << packet->systemAddress
              << ". Connections: "
              << rak_peer_interface->NumberOfConnections()
              << "\n";
        break;
    case ID_PONG:
        s_log << "ID_PONG from " << packet->systemAddress << "\n";
        break;
    case ID_ADVERTISE_SYSTEM:
        s_log << "ID_ADVERTISE_SYSTEM from " << packet->systemAddress << "\n";
        break;
    case ID_DOWNLOAD_PROGRESS:
        std::cout << "ID_DOWNLOAD_PROGRESS from " << packet->systemAddress << "\n";
        break;
    case ID_ALREADY_CONNECTED:
        std::cout << "ID_ALREADY_CONNECTED from " << packet->systemAddress << "\n";
        break;
    case ID_MODIFIED_PACKET:
        s_log << "ID_MODIFIED_PACKET from " << packet->systemAddress << "\n";
        break;
    case ID_TIMESTAMP:
        s_log << "ID_TIMESTAMP from " << packet->systemAddress << "\n";
        break;

    case VHPI_VERSION_MISMATCH:
    case VHPI_TYPE_MISMATCH:
    case VHPI_VERSION_INFO:
    {
        RakNet::BitStream stream(&packet->data[1], packet->length-1, false);
        
        VersionInfo info;
        info.readFromBitstream(stream);

        if (packet->data[0] == VHPI_VERSION_MISMATCH)
        {
            s_log << "Version mismatch from "
                  << packet->systemAddress
                  << ": "
                  << info
                  << "\n";
        } else if (packet->data[0] == VHPI_TYPE_MISMATCH)
        {
            s_log << "Type mismatch from "
                  << packet->systemAddress
                  << ": "
                  << info
                  << "\n";
        } else
        {
            s_log << packet->systemAddress
                  << " has version "
                  << info
                  << "\n";
        }
        break;
    }
        
        
    default:
        s_log << "Ignoring packet with unexpected id "
              << (unsigned)packet->data[0]
              << " from "
              << packet->systemAddress
              << "\n";
        printPacketIds();
    }
}



//------------------------------------------------------------------------------
void printPacketIds()
{
#ifdef ENABLE_DEV_FEATURES
    s_log << "ID_CONNECTION_REQUEST_ACCEPTED: " << (unsigned)ID_CONNECTION_REQUEST_ACCEPTED << "\n"
          << "ID_DOWNLOAD_PROGRESS: " << (unsigned)ID_DOWNLOAD_PROGRESS << "\n"
          << "ID_USER_PACKET_ENUM: " << (unsigned)ID_USER_PACKET_ENUM << "\n"
          << "MPI_LAST: " << (unsigned)network::master::MPI_LAST << "\n"
          << "RPI_LAST: " << (unsigned)network::ranking::RPI_LAST << "\n"
          << "FIRST_USER_PACKET_ID: " << (unsigned)FIRST_USER_PACKET_ID << "\n";
#endif    
}


//------------------------------------------------------------------------------
NetworkStatistics::NetworkStatistics(RakPeerInterface * i) :
    interface_(i),
    prev_received_(0),
    prev_sent_(0)
    
{
    // TODO CM fix stats
    /*
    RakNetStatistics * cur_stats = interface_->GetStatistics(UNASSIGNED_SYSTEM_ADDRESS);

    prev_sent_     = cur_stats->totalBitsSent;
    prev_received_ = cur_stats->bitsReceived;

    s_scheduler.addTask(PeriodicTaskCallback(this, &NetworkStatistics::update),
                        UPDATE_NET_STATS_DT,
                        "NetworkStatistics::update",
                        &fp_group_);

    s_console.addVariable("kb_per_sec_sent",     &kb_per_sec_sent_,     &fp_group_);
    s_console.addVariable("kb_per_sec_received", &kb_per_sec_received_, &fp_group_);
    */
}



//------------------------------------------------------------------------------
void NetworkStatistics::update(float dt)
{
    // TODO CM fix stats
    /*
    RakNetStatistics * cur_stats = interface_->GetStatistics(UNASSIGNED_SYSTEM_ADDRESS);


    kb_per_sec_sent_     = (float)(cur_stats->totalBitsSent - prev_sent_    ) / dt / 8000.0f;
    kb_per_sec_received_ = (float)(cur_stats->bitsReceived  - prev_received_) / dt / 8000.0f;

    prev_sent_     = cur_stats->totalBitsSent;
    prev_received_ = cur_stats->bitsReceived;
    */
}




}
