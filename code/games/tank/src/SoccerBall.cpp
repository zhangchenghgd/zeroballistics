

#include "SoccerBall.h"

#include <raknet/BitStream.h>
#include <raknet/GetTime.h>

#include "AutoRegister.h"
#include "GameState.h"



REGISTER_CLASS(GameObject, SoccerBall);


SoccerBall::SoccerBall() :
    rel_object_id_(INVALID_GAMEOBJECT_ID),
    game_state_(NULL)
{
}


//------------------------------------------------------------------------------
SoccerBall::~SoccerBall()
{
}


//------------------------------------------------------------------------------
void SoccerBall::setGameState(GameState * game_state)
{
    game_state_ = game_state;
}


//------------------------------------------------------------------------------
void SoccerBall::sendPosRelativeTo(uint16_t object_id)
{
    rel_object_id_ = object_id;
}

//------------------------------------------------------------------------------
void SoccerBall::setRelPos(const Vector & pos)
{
    rel_pos_ = pos;
}


//------------------------------------------------------------------------------
uint16_t SoccerBall::getRelObjectId() const
{
    return rel_object_id_;
}


//------------------------------------------------------------------------------
void SoccerBall::frameMove(float dt)
{
    RigidBody::frameMove(dt);

    return;
    
    if (!proxy_object_) return;

    RigidBody * body = (RigidBody*)game_state_->getGameObject(rel_object_id_);
    if (body)
    {
        Vector new_pos = body->getTransform().transformPoint(rel_pos_);

         setPosition(new_pos);
//         setGlobalLinearVel(body->getGlobalLinearVel());

//        getTarget()->setPosition(new_pos);
//        getTarget()->setGlobalLinearVel(body->getGlobalLinearVel());
//         handleProxyInterpolation();
//         handleProxyInterpolation();


//        proxy_object_->setPosition(proxy_object_->getPosition() + dt*body->getGlobalLinearVel());
        
    }
}


//------------------------------------------------------------------------------
void SoccerBall::writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const
{
    RigidBody::writeStateToBitstream(stream, type);

//    return;
    
    RigidBody * body = (RigidBody*)game_state_->getGameObject(rel_object_id_);

    if (body)
    {
        assert(game_state_);

        stream.Write(true);
        stream.Write(rel_object_id_);
        Vector rel_pos = body->getTransform().transformPointInv(getPosition());
        stream.WriteVector(rel_pos.x_, rel_pos.y_, rel_pos.z_);
        
    } else
    {
        stream.Write(false);
    }
}


//------------------------------------------------------------------------------
void SoccerBall::readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp)
{
    RigidBody::readStateFromBitstream(stream, type, timestamp);

//    return;
    
    bool rel_pos_given;
    stream.Read(rel_pos_given);

    if (rel_pos_given && game_state_)
    {
        stream.Read(rel_object_id_);

        stream.ReadVector(rel_pos_.x_,
                          rel_pos_.y_,
                          rel_pos_.z_);
        
        RigidBody * body = (RigidBody*)game_state_->getGameObject(rel_object_id_);
        if (body)
        {

//            getTarget()->setGlobalLinearVel(body->getGlobalLinearVel());
//            getProxy()->setGlobalLinearVel(2*body->getGlobalLinearVel());
            
            
            Vector new_pos = body->getTransform().transformPoint(rel_pos_);


//             uint32_t cur_time = RakNet::GetTime();
//             if (cur_time <= timestamp) return;
//             float dt = (float)(cur_time - timestamp) * 0.001f;

//              rel_pos_ += body->getLocalLinearVel() * dt;
            
//            new_pos += body->getGlobalLinearVel() / 10.0f;


//             new_pos *= 0.1f;
//             new_pos += getPosition() * 0.9;
            
            setPosition(new_pos);

            // setting the target only has nicer results for low lag,
            // but breaks things with higher ping.
//            getTarget()->setPosition(new_pos);


/*
            // correct position
            uint32_t cur_time = RakNet::GetTime();
            if (cur_time <= timestamp) return;
            float dt = (float)(cur_time - timestamp) * 0.001f;

            getTarget()->setPosition(getTarget()->getPosition() + 5*dt*body->getGlobalLinearVel());
*/
        }

//        getProxy()->enableGravity(true);
//        enableProxyInterpolation(false);
    } else
    {
        rel_object_id_ = INVALID_GAMEOBJECT_ID;
//        enableProxyInterpolation(true);
//        getProxy()->enableGravity(false);
    }
}


