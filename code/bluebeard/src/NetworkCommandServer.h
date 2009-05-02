

#ifndef TANK_NETWORK_COMMAND_SERVER_INCLUDED
#define TANK_NETWORK_COMMAND_SERVER_INCLUDED

//******************** Commands issued by server ********************//

#include <raknet/RakNetTypes.h>
#include <raknet/MessageIdentifiers.h>
#include <raknet/PacketPriority.h>
#include <raknet/BitStream.h>

#include "PlayerInput.h"
#include "GameState.h"
#include "NetworkCommand.h"
#include "GameObject.h"


class GameLogicServer;
class Controllable;
class Matrix;

//------------------------------------------------------------------------------
namespace network
{



//------------------------------------------------------------------------------
/**
 *  Used in StringMessageCmd. String messages from server to
 *  client.
 */
enum STRING_MESSAGE_TYPE
{
    SMT_RCON_RESPONSE,    ///<

    SMT_CHAT,             ///< Ordinary chat message. pid1 is chatting player.
    SMT_TEAM_CHAT,        ///< Chat message only seen by own team. pid1 is chatting player.

    SMT_GAME_INFORMATION,
    SMT_PLAYER_NAME
};





//------------------------------------------------------------------------------
class LoadLevelCmd : public NetworkCommandServer
{
 public:
    LoadLevelCmd();
    LoadLevelCmd(const std::string & map_name,
                 const std::string & game_logic_type);
    
    virtual void execute(PuppetMasterClient * master);
    
 protected:
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);
    
    std::string map_name_;
    std::string game_logic_type_;
};
 

//------------------------------------------------------------------------------
class CreatePlayerCmd : public NetworkCommandServer
{
 public:
    CreatePlayerCmd();
    CreatePlayerCmd(const SystemAddress & id);
    
    virtual void execute(PuppetMasterClient * master);
    
 protected:
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);
    
    SystemAddress id_; ///< Specifies the id of the player to be created.
};



//------------------------------------------------------------------------------
class DeletePlayerCmd : public NetworkCommandServer
{
 public:
    DeletePlayerCmd();
    DeletePlayerCmd(const SystemAddress & id);

    virtual void execute(PuppetMasterClient * master);
    
 protected:
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);
        
    SystemAddress id_; ///< Specifies the id of the player to be deleted.
};


//------------------------------------------------------------------------------
/**
 *  Used to assign a controllable object to a player.
 */
class SetControllableCmd : public NetworkCommandServer
{
 public:
    SetControllableCmd();
    SetControllableCmd(const SystemAddress & player_id,
                       Controllable * controllable);

    virtual void execute(PuppetMasterClient * master);
    
 protected:
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);

    SystemAddress player_id_;
    uint16_t controllable_id_;
};


//------------------------------------------------------------------------------
class SetControllableStateCmd : public NetworkCommandServer
{
 public:
    SetControllableStateCmd();
    SetControllableStateCmd(uint8_t sequence_number,
                            const Controllable * object);

    virtual void execute(PuppetMasterClient * master);
    
 protected:
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);

    RakNet::BitStream state_;
    uint8_t sequence_number_;
};



//------------------------------------------------------------------------------
class CreateGameObjectCmd : public NetworkCommandServer
{
 public:
    CreateGameObjectCmd();
    CreateGameObjectCmd(GameObject * object);

    virtual void execute(PuppetMasterClient * master);
    
 protected:
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);

    uint16_t id_;        ///< Will identify the object for further
                         ///communication.
    std::string type_;   ///< The type of GameObject (Name of the class).


    RakNet::BitStream object_init_values_stream_; ///< The object writes its
                                            ///inital data into this stream.

    RakNet::BitStream object_state_stream_; ///< The object writes its
                                            ///data into this
                                            ///stream. Same data as in
                                            ///SetGameObjectStateCmd.
    
    uint32_t timestamp_;
};


//------------------------------------------------------------------------------
class ReplaceGameObjectCommand : public NetworkCommandServer
{
 public:
    ReplaceGameObjectCommand();
    ReplaceGameObjectCommand(uint16_t object_id,
                             uint16_t replacement_start_id,
                             const std::string appendix_list);

    virtual void execute(PuppetMasterClient * master);
    
 protected:
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);

    uint16_t replaced_id_;                   ///< The id of the object to replace.
    uint16_t start_replacing_id_;
    std::string appendix_list_;     ///< \see ObjectParts.h
};


//------------------------------------------------------------------------------
class DeleteGameObjectCmd : public NetworkCommandServer
{
 public:
    DeleteGameObjectCmd();
    DeleteGameObjectCmd(uint16_t id);

    virtual void execute(PuppetMasterClient * master);

 protected:
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);

    uint16_t id_; ///< Id of the game object to be deleted.
};


//------------------------------------------------------------------------------
class SetGameObjectStateCmd : public NetworkCommandServer
{
 public:
    SetGameObjectStateCmd();
    SetGameObjectStateCmd(const GameObject * object,
                          OBJECT_STATE_TYPE type);

    virtual void execute(PuppetMasterClient * master);

 protected:
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);
    
    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);

    OBJECT_STATE_TYPE type_;
    
    uint16_t id_; ///< Id of the game object which is to be
                  ///changed/updated.

    RakNet::BitStream object_state_stream_; ///< The object writes its
                                            ///state data into this stream.

    uint32_t timestamp_;
};

//------------------------------------------------------------------------------
/**
 *  Represents a string message from the server to the client.
 *
 *  \see SERVER_MESSAGE_TYPE
 */
class StringMessageCmd : public NetworkCommandServer
{
 public:
    StringMessageCmd();
    StringMessageCmd(STRING_MESSAGE_TYPE type,
                     const std::string & message,
                     const SystemAddress & pid1 = UNASSIGNED_SYSTEM_ADDRESS);

    virtual void execute(PuppetMasterClient * master);

 protected:
    
    virtual void getNetworkOptions(PacketReliability & reliability,
                                   PacketPriority    & priority,
                                   unsigned          & channel);

    virtual void writeToBitstream (RakNet::BitStream & stream);
    virtual void readFromBitstream(RakNet::BitStream & stream);

    uint8_t type_;
    std::string msg_;

    SystemAddress pid1_;
};

//------------------------------------------------------------------------------
/**
 *  Arbitrary command containing arguments in a bitstream, is
 *  forwarded to client GameLogic.
 */
class CustomServerCmd : public NetworkCommandServer
{
 public:
    CustomServerCmd();
    CustomServerCmd(uint8_t type, RakNet::BitStream & data);

    virtual void execute(PuppetMasterClient * master);

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




#endif // TANK_NETWORK_COMMAND_SERVER_INCLUDED
