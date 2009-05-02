
#include "Team.h"


#include "ParameterManager.h"

const std::string TEAM_CONFIG_DIR = "data/config/";

//------------------------------------------------------------------------------
Team::Team() : id_(INVALID_TEAM_ID)
{
}


//------------------------------------------------------------------------------
Team::~Team()
{
}

//------------------------------------------------------------------------------
void Team::setId(TEAM_ID id)
{
    id_    = id;
}
    

//------------------------------------------------------------------------------
TEAM_ID Team::getId() const
{
    return id_;
}

//------------------------------------------------------------------------------
const std::string & Team::getName() const
{
    return s_params.get<std::string>(config_name_ + ".name");
}



//------------------------------------------------------------------------------
void Team::setConfigName(const std::string & name)
{
    config_name_ = name;
}

//------------------------------------------------------------------------------
const std::string & Team::getConfigName() const
{
    return config_name_;
}
