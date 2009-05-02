

#ifndef BLUEBEARD_PLAYER_INCLUDED
#define BLUEBEARD_PLAYER_INCLUDED


#include <string>

#include <raknet/BitStream.h>


#include "RegisteredFpGroup.h"

class Controllable;

// if we're being compiled by Microsoft Visual C++, turn off some warnings
// that are used in the regex for player name
#if defined(_MSC_VER)
  #pragma warning(disable:4129)   //warning C4129: '{' : unrecognized character escape sequence
#endif

const std::string PLAYER_NAME_SPECIAL_CHARS = "\\^\\.\\$\\|\\(\\)\\[\\]\\*\\+\\?\\_\\-\\#\\,\\@\\!\\=\\~\\'";
const std::string PLAYER_NAME_ALLOWED_CHARS = "0-9a-zA-Z";
const std::string PLAYER_NAME_REGEX = "([" + PLAYER_NAME_SPECIAL_CHARS + 
                                             PLAYER_NAME_ALLOWED_CHARS + 
                                             "]{3})([" +
                                             PLAYER_NAME_SPECIAL_CHARS + 
                                             PLAYER_NAME_ALLOWED_CHARS + 
                                             "\\s]{0,17})";

//------------------------------------------------------------------------------
class Player
{
 public:
    Player(const SystemAddress & id);
    virtual ~Player();

    void setName(const std::string & name);
    
    const SystemAddress & getId() const;
    const std::string & getName() const;
    Controllable * getControllable();
    Controllable * getControllable() const;

    bool operator==(const SystemAddress & address) const;
    
    // XXX fast fix to avoid client message flooding
    bool allowAnnoyingClientRequest() const;
    void incAnnoyingClientRequest();
    void decAnnoyingClientRequest();

 protected:

    SystemAddress id_;
    std::string name_;
    Controllable * controllable_;

    unsigned annoying_client_requests_;

    RegisteredFpGroup fp_group_;
};


#endif
