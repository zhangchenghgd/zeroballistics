

#ifndef TANK_NETWORK_COMMAND_CLIENT_INCLUDED
#define TANK_NETWORK_COMMAND_CLIENT_INCLUDED

//******************** Commands issued by client ********************//

#include <raknet/RakNetTypes.h>
#include <raknet/MessageIdentifiers.h>
#include <raknet/PacketPriority.h>
#include <raknet/BitStream.h>

#include "PlayerInput.h"
#include "GameState.h"
#include "NetworkCommand.h"



//------------------------------------------------------------------------------
namespace network
{ 
class NetworkCommand;

 
//------------------------------------------------------------------------------
class PlayerInputCmd : public NetworkCommandClient
{
 public:
    PlayerInputCmd(const SystemAddress & player_id);
    PlayerInputCmd(const PlayerInput & input, uint8_t sequence_number);

    virtual void execute(PuppetMasterServer * master);

    uint8_t getSequenceNumber() const;
 protected:
    
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);

    PlayerInput input_;
    uint8_t sequence_number_;

    uint32_t timestamp_;    
};

 
//------------------------------------------------------------------------------ 
class RconCmd : public NetworkCommandClient
{
 public:
    RconCmd(const SystemAddress & player_id);
    RconCmd(const std::string & cmd_and_args);

    virtual void execute(PuppetMasterServer * master);

 protected:
    
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);

    std::string cmd_and_args_;
};



//------------------------------------------------------------------------------ 
class ChatCmd : public NetworkCommandClient
{
 public:
    ChatCmd(const SystemAddress & player_id);
    ChatCmd(const std::string & msg);

    virtual void execute(PuppetMasterServer * master);

 protected:
    
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);

    std::string msg_;
};


//------------------------------------------------------------------------------ 
class SetPlayerNameCmd : public NetworkCommandClient
{
 public:
    SetPlayerNameCmd(const SystemAddress & player_id);
    SetPlayerNameCmd(const std::string & name);

    virtual void execute(PuppetMasterServer * master);

 protected:
    
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);

    std::string name_;
};


//------------------------------------------------------------------------------
/**
 *  Arbitrary command containing arguments in a bitstream, is
 *  forwarded to server GameLogic.
 */
class CustomClientCmd : public NetworkCommandClient
{
 public:
    CustomClientCmd(const SystemAddress & player_id);
    CustomClientCmd(uint8_t type, RakNet::BitStream & data);

    virtual void execute(PuppetMasterServer * master);

 protected:
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);
    
    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);

    uint8_t type_;

    RakNet::BitStream args_stream_; ///< Arguments for the command are
                                    ///written to & read from this
                                    ///stream.
};


//******************** End of command types ********************//


} // namespace network





#endif // TANK_NETWORK_COMAND_CLIENT_INCLUDED
