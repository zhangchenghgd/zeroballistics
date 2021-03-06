
#include "Player.h"

#include <limits>

#undef min
#undef max

const unsigned MAX_ANNOYING_CLIENT_REQUESTS = 4;

//------------------------------------------------------------------------------
Player::Player(const SystemAddress & id) :
    id_(id),
    controllable_(NULL),
    annoying_client_requests_(0),
    network_delay_(0.0f)
{
    
}


//------------------------------------------------------------------------------
Player::~Player()
{
}


//------------------------------------------------------------------------------
void Player::setName(const std::string & name)
{
    name_ = name;
}


//------------------------------------------------------------------------------
const SystemAddress & Player::getId() const
{
    return id_;
}

//------------------------------------------------------------------------------
const std::string & Player::getName() const
{
    return name_;
}

//------------------------------------------------------------------------------
Controllable * Player::getControllable()
{
    return controllable_;
}

//------------------------------------------------------------------------------
Controllable * Player::getControllable() const
{
    return controllable_;
}

//------------------------------------------------------------------------------
bool Player::operator==(const SystemAddress & address) const
{
    return address == id_;
}

//------------------------------------------------------------------------------
bool Player::allowAnnoyingClientRequest() const
{
    if(annoying_client_requests_ < MAX_ANNOYING_CLIENT_REQUESTS)
    {
        return true;
    }
    else
    {
        return false;
    }
}

//------------------------------------------------------------------------------
void Player::incAnnoyingClientRequest()
{
    ++annoying_client_requests_;
}

//------------------------------------------------------------------------------
void Player::decAnnoyingClientRequest()
{
    if(annoying_client_requests_ > 0) --annoying_client_requests_;
}

//------------------------------------------------------------------------------
void Player::setNetworkDelay(float d)
{
    network_delay_ = std::max(0.0f, d);
}

//------------------------------------------------------------------------------
const float & Player::getNetworkDelay() const
{
    return network_delay_;
}
