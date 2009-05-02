
#ifndef TANK_TEAM_INCLUDED
#define TANK_TEAM_INCLUDED


#include "Datatypes.h"


typedef uint8_t TEAM_ID;


const TEAM_ID INVALID_TEAM_ID = (TEAM_ID)-1;

//------------------------------------------------------------------------------
class Team
{
 public:
    Team();
    virtual ~Team();

    void setId(TEAM_ID id);
    TEAM_ID getId() const;    

    const std::string & getName() const;
    const Color & getColor() const;
    
    void setConfigName(const std::string & name);
    const std::string & getConfigName() const;
    
 protected:

    TEAM_ID id_;
    std::string config_name_;
};

#endif
