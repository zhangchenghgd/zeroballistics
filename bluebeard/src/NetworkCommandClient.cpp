//******************** Commands issued by client ********************//

#include "NetworkCommandClient.h"

#include <raknet/RakPeerInterface.h>
#include <raknet/RakNetworkFactory.h>
#include <raknet/GetTime.h>

#include "Log.h"
#include "GameState.h"
#include "PuppetMasterServer.h"
#include "GameLogicServer.h"

//------------------------------------------------------------------------------
namespace network
{

//********** PlayerInputCmd **********//

//------------------------------------------------------------------------------
PlayerInputCmd::PlayerInputCmd(const SystemAddress & player_id) :
    NetworkCommandClient(player_id)
{
}


//------------------------------------------------------------------------------
PlayerInputCmd::PlayerInputCmd(const PlayerInput & input,
                               uint8_t sequence_number) :
    input_(input),
    sequence_number_(sequence_number)
{
}

//------------------------------------------------------------------------------
/**
 *  
 */
void PlayerInputCmd::execute(PuppetMasterServer * master)
{
    master->handleInput(player_id_, input_, sequence_number_, timestamp_);    
}

//------------------------------------------------------------------------------
uint8_t PlayerInputCmd::getSequenceNumber() const
{
    return sequence_number_;
}

//------------------------------------------------------------------------------
void PlayerInputCmd::getNetworkOptions(PacketReliability & reliability,
                                       PacketPriority    & priority,
                                       unsigned          & channel)
{
    reliability = UNRELIABLE;
    priority    = LOW_PRIORITY;
    channel     = NC_DONTCARE;
}



//------------------------------------------------------------------------------
void PlayerInputCmd::writeToBitstream (RakNet::BitStream & stream)
{
    timestamp_ = RakNet::GetTime();

    stream.Write((char)ID_TIMESTAMP);
    stream.Write(timestamp_);
    
    stream.Write((char)TPI_PLAYER_INPUT);
    stream.Write(sequence_number_);

    input_.writeToBitstream(stream);
}


//------------------------------------------------------------------------------
void PlayerInputCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;

    stream.Read(packet_id);
    stream.Read(timestamp_);

    stream.Read(packet_id);
    stream.Read(sequence_number_);
    
    input_.readFromBitstream(stream);
}



//********** RconCmd **********//

//------------------------------------------------------------------------------
RconCmd::RconCmd(const SystemAddress & player_id) :
    NetworkCommandClient(player_id)
{
}


//------------------------------------------------------------------------------
RconCmd::RconCmd(const std::string & cmd_and_args) :
    cmd_and_args_(cmd_and_args)
{
}

//------------------------------------------------------------------------------
/**
 *  
 */
void RconCmd::execute(PuppetMasterServer * master)
{
    master->rcon(player_id_, cmd_and_args_);    
}


//------------------------------------------------------------------------------
void RconCmd::getNetworkOptions(PacketReliability & reliability,
                                PacketPriority    & priority,
                                unsigned          & channel)
{
    reliability = UNRELIABLE;
    priority    = LOW_PRIORITY;
    channel     = NC_DONTCARE;
}



//------------------------------------------------------------------------------
void RconCmd::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write((char)TPI_RCON_CMD);

    network::writeToBitstream(stream, cmd_and_args_);
}


//------------------------------------------------------------------------------
void RconCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    stream.Read(packet_id);

    network::readFromBitstream(stream, cmd_and_args_);
}


//********** ChatCmd **********//


//------------------------------------------------------------------------------
ChatCmd::ChatCmd(const SystemAddress & player_id) :
    NetworkCommandClient(player_id)
{
}


//------------------------------------------------------------------------------
ChatCmd::ChatCmd(const std::string & msg) :
    msg_(msg)
{
}

//------------------------------------------------------------------------------
/**
 *  
 */
void ChatCmd::execute(PuppetMasterServer * master)
{
    master->chat(player_id_, msg_);
}


//------------------------------------------------------------------------------
void ChatCmd::getNetworkOptions(PacketReliability & reliability,
                                PacketPriority    & priority,
                                unsigned          & channel)
{
    reliability = RELIABLE;
    priority    = LOW_PRIORITY;
    channel     = NC_DONTCARE;
}



//------------------------------------------------------------------------------
void ChatCmd::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write((char)TPI_CHAT);

    network::writeToBitstream(stream, msg_);
}


//------------------------------------------------------------------------------
void ChatCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    stream.Read(packet_id);

    network::readFromBitstream(stream, msg_);
}



//********** SetPlayerNameCmd **********//


//------------------------------------------------------------------------------
SetPlayerNameCmd::SetPlayerNameCmd(const SystemAddress & player_id) :
    NetworkCommandClient(player_id)
{
}


//------------------------------------------------------------------------------
SetPlayerNameCmd::SetPlayerNameCmd(const std::string & name) :
    name_(name)
{
}

//------------------------------------------------------------------------------
/**
 *  
 */
void SetPlayerNameCmd::execute(PuppetMasterServer * master)
{
    master->setPlayerName(player_id_, name_);
}


//------------------------------------------------------------------------------
void SetPlayerNameCmd::getNetworkOptions(PacketReliability & reliability,
                                PacketPriority    & priority,
                                unsigned          & channel)
{
    reliability = RELIABLE;
    priority    = LOW_PRIORITY;
    channel     = NC_DONTCARE;
}



//------------------------------------------------------------------------------
void SetPlayerNameCmd::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write((char)TPI_SET_PLAYER_NAME);

    network::writeToBitstream(stream, name_);
}


//------------------------------------------------------------------------------
void SetPlayerNameCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    stream.Read(packet_id);

    network::readFromBitstream(stream, name_);
}




//********** CustomClientCmd **********//


//------------------------------------------------------------------------------
CustomClientCmd::CustomClientCmd(const SystemAddress & player_id) :
    NetworkCommandClient(player_id)
{
}

//------------------------------------------------------------------------------
CustomClientCmd::CustomClientCmd(uint8_t type, RakNet::BitStream & data) :
    type_(type),
    args_stream_(data.GetData(), data.GetNumberOfBytesUsed(), true)
{
    
    s_log << Log::debug('n')
          << "CustomClientCmd constructed with type "
          << (unsigned)type
          << "\n";
}

//------------------------------------------------------------------------------
void CustomClientCmd::execute(PuppetMasterServer * master)
{
    master->getGameLogic()->executeCustomCommand(type_, args_stream_);
    
    s_log << Log::debug('n')
          << "CustomClientCmd executed with type "
          << (unsigned)type_
          << "\n";
    
}

//------------------------------------------------------------------------------
void CustomClientCmd::getNetworkOptions(PacketReliability & reliability,
                                            PacketPriority    & priority,
                                            unsigned          & channel)
{
    reliability = RELIABLE_ORDERED;
    priority    = MEDIUM_PRIORITY;
    channel     = NC_GAMESTATE;
}

//------------------------------------------------------------------------------
void CustomClientCmd::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write((char)TPI_CUSTOM_CLIENT_CMD);
    stream.Write(type_);

    network::writeToBitstream(stream, args_stream_);
}

//------------------------------------------------------------------------------
void CustomClientCmd::readFromBitstream(RakNet::BitStream & stream)
{
    char packet_id;
    stream.Read(packet_id);
    stream.Read(type_);

    network::readFromBitstream(stream, args_stream_);
}




} // namespace network


