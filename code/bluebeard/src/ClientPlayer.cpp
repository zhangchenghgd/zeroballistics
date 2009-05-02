
#include "ClientPlayer.h"


#include "physics/OdeRigidBody.h"
#include "physics/OdeSimulator.h"


#include "Controllable.h"
#include "Log.h"
#include "GameState.h"
#include "PuppetMasterClient.h"
#include "NetworkCommand.h"
#include "Profiler.h"
#include "ParameterManager.h"




//------------------------------------------------------------------------------
enum CONTACT_CATEGORY_REPLAY_SIM
{
    CCRS_STATIC,
    CCRS_CONTROLLABLE
};


//------------------------------------------------------------------------------
void RemotePlayer::setControllable(Controllable * controllable)
{
    if (controllable_) controllable_->setOwner(UNASSIGNED_SYSTEM_ADDRESS);
    controllable_ = controllable;
    if (controllable_) controllable_->setOwner(id_);
}


//------------------------------------------------------------------------------
LocalPlayer::LocalPlayer() :
    Player(UNASSIGNED_SYSTEM_ADDRESS),
    cur_sequence_number_(0),
    hist_head_(0),
    hist_tail_(0),
    replay_simulator_(new physics::OdeSimulator("replaysim")),
    correct_controllable_(NULL),
    test_controllable_(NULL),
    history_size_(0),
    history_overflow_(false)
{
    s_console.addVariable("hist_size", &history_size_, &fp_group_);

    replay_simulator_->enableCategoryCollisions(CCRS_STATIC, CCRS_STATIC, false);
}

//------------------------------------------------------------------------------
LocalPlayer::~LocalPlayer()
{    
    DELNULL(correct_controllable_);
    DELNULL(test_controllable_);
}

//------------------------------------------------------------------------------
/**
 *  Deletes test & correct controllable and resets the replay
 *  simulator.
 */
void LocalPlayer::reset()
{
    DELNULL(correct_controllable_);
    DELNULL(test_controllable_);
    
    // This must happen before replay_simulator_ reset because of geom destruction    
    heightfield_geom_.reset(NULL); 
    replay_simulator_.reset(new physics::OdeSimulator("replaysim"));

    replay_simulator_->enableCategoryCollisions(CCRS_STATIC, CCRS_STATIC, false);

    assert(static_body_.empty());
}


//------------------------------------------------------------------------------
/**
 *  Sets the input for the local player.
 *
 *  Player input is recorded in history_ to be able to replay player
 *  commands after server corrections.
 */
void LocalPlayer::handleInput(const PlayerInput & input)
{
    assert(controllable_);

    writeStateToHistory(controllable_, hist_tail_);
    
    history_[hist_tail_].sequence_number_ = cur_sequence_number_;
    history_[hist_tail_].input_           = controllable_->getPlayerInput();

    advanceHistoryIndex(hist_tail_);

    if (hist_tail_ == hist_head_)
    {
        advanceHistoryIndex(hist_head_);
        s_log << Log::warning << "Connection problem.\n";
        history_overflow_ = true;
    } else
    {
        history_overflow_ = false;
    }

    // if overflow, nullify input
    if(history_overflow_)
    {
        controllable_->setPlayerInput(PlayerInput());
    }    
    else
    {
        controllable_->setPlayerInput(input);
    }

//     s_log << Log::debug('b')
//           << "history entry \t\t" << (unsigned)cur_sequence_number_
//           << " set to pos " << controllable_->getPosition().z_
//           << "\n";

#ifdef ENABLE_DEV_FEATURES
    history_size_ = (int)hist_tail_ - hist_head_;
    if (history_size_ < 0) history_size_ += HISTORY_SIZE;
#endif
}
    


//------------------------------------------------------------------------------
/**
 *  The correction is compared to the position stored in the history
 *  for the corresponding frame. If positions match, nothing
 *  happens. If not, player state is reset to the corrected frame and
 *  player input since then is replayed to acquire the new current
 *  player state.
 *
 */
void LocalPlayer::serverCorrection(uint8_t sequence_number,
                                   RakNet::BitStream & correct_state)
{
    PROFILE(LocalPlayer::serverCorrection);
    
    if (!controllable_) return;

#ifdef ENABLE_DEV_FEATURES    
    s_log << Log::debug('b')
          << "received correction "
          << (unsigned)sequence_number
          << "\n";
    s_log << Log::debug('b')
          << "Head sequence_number is "
          << (unsigned)history_[hist_head_].sequence_number_
          << "\n";
#endif


    ADD_STATIC_CONSOLE_VAR(bool, force_correction, false);
    ADD_STATIC_CONSOLE_VAR(bool, skip_correction, false);
    ADD_STATIC_CONSOLE_VAR(bool, replaying_history, false);
    

    if(hist_head_ == hist_tail_)
    {   
        s_log << Log::debug('n')
              << "Received correction "
              << (unsigned)sequence_number
              << " although if there is no input on the way.\n";
        return;
    }


    // Discard out-of-order corrections
    int seq_diff = seqNumberDifference(sequence_number, history_[hist_head_].sequence_number_);
    if (seq_diff < 0 && seq_diff > -50)
    {
        s_log << Log::debug('n')
              << "dropping out-of-order packet in LocalPlayer::serverCorrection. expected "
              << (unsigned)history_[hist_head_].sequence_number_
              << ", got "
              << (unsigned)sequence_number
              << "\n";
        return;
    }

    
    // Discard old moves
    while (seqNumberDifference(sequence_number, history_[hist_head_].sequence_number_) > 0)
    {
        s_log << Log::debug('b') << "dropping history entry "
              << (unsigned)history_[hist_head_].sequence_number_ << "\n";
        
        advanceHistoryIndex(hist_head_);
        if (hist_head_ == hist_tail_)
        {
            s_log << Log::debug('b') << "up to date\n";
            // We have processed all outstanding moves and are
            // up-to-date.
            return;
        }
    } 

    // Correction should now match exactly the state previously saved
    // in our history. Discard history entries otherwise
    while (history_[hist_head_].sequence_number_ != sequence_number)
    {
        if (hist_head_ == hist_tail_)
        {
            s_log << "History consumed. Bailing out.\n";
            return;
        } else
        {
            s_log << Log::debug('n')
                  << "Got mismatching correction. Got "
                  << (unsigned)sequence_number
                  << ", expected "
                  << (unsigned)history_[hist_head_].sequence_number_
                  << ". Discarding history entry.\n";

            advanceHistoryIndex(hist_head_);
        }
    }


    correct_controllable_->readStateFromBitstream(correct_state, OST_CORE, 0);
    readStateFromHistory(test_controllable_, hist_head_);
    test_controllable_->setSleeping(true); // XXXX just neccessary because test_controllable_ is in sim


//     s_log << Log::debug('b')
//           << "received correction "
//           << (unsigned)sequence_number
//           << " with position "
//           << correct_controllable_->getPosition().z_
//           << "\n";


#ifdef ENABLE_DEV_FEATURES
    history_size_ = (int)hist_tail_ - hist_head_;
    if (history_size_ < 0) history_size_ += HISTORY_SIZE;

    s_log << Log::debug('b')
          << "history size: "
          << history_size_
          << "\n";
#endif

    
    if (!force_correction && correct_controllable_->isStateEqual(test_controllable_))
    {
//         s_log << Log::debug('b') << "correction "
//               << (unsigned)sequence_number << " matches.\n";

        replaying_history = false;
     
        advanceHistoryIndex(hist_head_);
        return;
    } else if (skip_correction) return;

    replaying_history = true;
    
    // Now we need to replay the stored moves.
    s_log << Log::debug('n') << "state mismatch. starting correction replay\n";    

    writeStateToHistory(correct_controllable_, hist_head_);
    
    unsigned cur_index = hist_head_;    
    unsigned s1 = history_[hist_head_].sequence_number_;
    unsigned s2;
    float dt = 1.0f / s_params.get<float>("physics.fps");
    PlayerInput input;
    do
    {
        advanceHistoryIndex(cur_index);

        if (cur_index == hist_tail_)
        {
            s2 = cur_sequence_number_;
            input = controllable_->getPlayerInput();
        } else
        {
            s2 = history_[cur_index].sequence_number_;
            input = history_[cur_index].input_;            
        }

        correct_controllable_->setPlayerInput(input);
        
        for (unsigned i=s1; seqNumberDifference(s2,i) > 0; ++i)
        {
            correct_controllable_->frameMove(dt);
            replay_simulator_    ->frameMove(dt);
        }
        
        if (cur_index != hist_tail_)
        {
            writeStateToHistory(correct_controllable_, cur_index);
        } else
        {
            RakNet::BitStream state;
            correct_controllable_->writeStateToBitstream(state, OST_CORE | OST_CLIENT_SIDE_PREDICTION);

            controllable_->readStateFromBitstream(state, OST_CORE, 0);
        }
        
        s1 = s2;
    } while (cur_index != hist_tail_);

    advanceHistoryIndex(hist_head_);
}


//------------------------------------------------------------------------------
void LocalPlayer::setId(const SystemAddress & id)
{
    id_ = id;
}


//------------------------------------------------------------------------------
/**
 *  Sets the object controlled by the local player.
 *
 *  - Create the objects for client side prediction
 *    - One object to perform the replay
 *    - One object to load the history state into for comparison
 *
 *  - Adjust collision groups for new & old controllable
 */
void LocalPlayer::setControllable(Controllable * new_controllable)
{
    if (new_controllable && new_controllable == controllable_)
    {
        s_log << Log::warning << "Controlled object was already set.\n";
        return;
    }


    // A controllable is already set, put it into uncontrolled state.
    if (controllable_)
    {
        controllable_->setPlayerInput(PlayerInput());
        controllable_->setLocallyControlled(false);
        
        controllable_->getTarget()->changeSpace(NULL);

        controllable_->getTarget()->enableGravity(false);
        controllable_->getTarget()->setAutoDisable(true);

        controllable_->setOwner(UNASSIGNED_SYSTEM_ADDRESS);
    }

    DELNULL(correct_controllable_);
    DELNULL(test_controllable_);
    
    controllable_ = new_controllable;


    if (controllable_)
    {
        correct_controllable_ = controllable_->cloneForReplay(replay_simulator_.get());

        // correct_controllable_ keeps it shapes to collide with
        // static level geometry.
        correct_controllable_->setCollisionCategory(CCRS_CONTROLLABLE, false);
        correct_controllable_->setLocallyControlled(true);
        correct_controllable_->setLocation(CL_REPLAY_SIM);
        correct_controllable_->getTarget()->setName("correct_controllable");
        
        // test_controllable exists solely to compare object states;
        // no collision detection
        test_controllable_ = controllable_->cloneForReplay(replay_simulator_.get());
        assert(!test_controllable_->getProxy());
        test_controllable_->getTarget()->changeSpace(NULL);
        test_controllable_->setSleeping(true);
        test_controllable_->setLocation(CL_REPLAY_SIM);
        test_controllable_->getTarget()->setName("test_controllable");


        controllable_->setLocallyControlled(true);
        controllable_->setLocation(CL_CLIENT_SIDE);
        controllable_->getTarget()->changeSpace(NULL,
                                                controllable_->getTarget()->getSimulator()->getActorSpace());
        controllable_->getTarget()->enableGravity(true);
        controllable_->getTarget()->setAutoDisable(false);
        controllable_->getTarget()->setSleeping(false);
            
        controllable_->setCollisionCategory(CCC_CONTROLLED, false);

        controllable_->setOwner(id_);
    }

    cur_sequence_number_ = 0;
    hist_head_ = hist_tail_ = 0;
}



//------------------------------------------------------------------------------
void LocalPlayer::incSequenceNumber()
{
    ++cur_sequence_number_;
}


//------------------------------------------------------------------------------
uint8_t LocalPlayer::getSequenceNumber() const
{
    return cur_sequence_number_;
}


//------------------------------------------------------------------------------
/**
 *  
 */
bool LocalPlayer::isLevelDataSet() const
{
    return heightfield_geom_.get();
}

//------------------------------------------------------------------------------
/**
 *  Sets the static level data for client side prediction collision
 *  detection.
 */
void LocalPlayer::setLevelData(GameState * state)
{
    assert(replay_simulator_->isEmpty());

    for (GameState::GameObjectContainer::iterator it = state->getGameObjects().begin();
         it != state->getGameObjects().end();
         ++it)
    {
        RigidBody * rigid_body = dynamic_cast<RigidBody*>(it->second); /// XXX ugly dynamic cast here??
        assert(rigid_body);

        bool any_geom_added = false;

        if(rigid_body && rigid_body->isStatic())
        {
            physics::OdeRigidBody * b = replay_simulator_->instantiate(rigid_body->getTarget());

            b->setTransform(rigid_body->getTransform());

            unsigned g=0;
            while (g < b->getGeoms().size())
            {
                if (b->getGeoms()[g]->isSensor())
                {
                    b->deleteGeom(g);
                } else
                {
                    b->getGeoms()[g]->setCategory(CCRS_STATIC);
                    s_log << Log::debug('l')
                          << "added " << b->getGeoms()[g]->getName() << " to replay simulator\n";

                    any_geom_added = true;
                    
                    ++g;
                }
            }

            if (any_geom_added)
            {
                static_body_[rigid_body] = b;
                rigid_body->addObserver(ObserverCallbackFun2(this, &LocalPlayer::onRigidBodyDeleted),
                                        GOE_SCHEDULED_FOR_DELETION,
                                        &fp_group_);
            }
        }
    }

    heightfield_geom_.reset(state->createHeightfieldGeom(replay_simulator_->getStaticSpace()));
    heightfield_geom_->setCategory(CCRS_STATIC, replay_simulator_.get());
}


//------------------------------------------------------------------------------
/**
 *  Returns the status of the history_overflow_ flag
 */
bool LocalPlayer::isHistoryOverflow()
{
    return history_overflow_;
}

//------------------------------------------------------------------------------
const physics::OdeSimulator * LocalPlayer::getReplaySimulator() const
{
    return replay_simulator_.get();
}

//------------------------------------------------------------------------------
Controllable * LocalPlayer::getReplaySimulatorControllable()
{
    return correct_controllable_;
}
    
//------------------------------------------------------------------------------
/**
 *  Advance an index into the circular history array.
 */
void LocalPlayer::advanceHistoryIndex(unsigned & index)
{
    if (++index == HISTORY_SIZE) index=0;
}

//------------------------------------------------------------------------------
void LocalPlayer::writeStateToHistory(const Controllable * c, unsigned index)
{
    // XXXX hack
    RakNet::BitStream stream;
    c->writeStateToBitstream(stream, OST_CORE | OST_CLIENT_SIDE_PREDICTION);
    history_[index].state_.resize(stream.GetNumberOfBytesUsed());
    memcpy(&history_[index].state_[0], stream.GetData(), stream.GetNumberOfBytesUsed());
}


//------------------------------------------------------------------------------
void LocalPlayer::readStateFromHistory(Controllable * c, unsigned index)
{
    RakNet::BitStream state((unsigned char*)&history_[index].state_[0],
                            history_[index].state_.size(),
                            false);
    c->readStateFromBitstream(state, OST_CORE, 0);
}


//------------------------------------------------------------------------------
void LocalPlayer::onRigidBodyDeleted(Observable* o, unsigned)
{
    std::map<RigidBody*, physics::OdeRigidBody*>::iterator it = static_body_.find((RigidBody*)o);
    assert(it != static_body_.end());

    delete it->second;

    static_body_.erase(it);
}


