
#include "ServerPlayer.h"

#include "NetworkCommand.h"
#include "Log.h"
#include "ParameterManager.h"


const float TOTAL_DELAY_TRACKING_SPEED = 0.01;
const unsigned MAX_NUM_STEPS_OVERFULL = 10;

//------------------------------------------------------------------------------
ServerPlayer::ServerPlayer(const SystemAddress & id) :
    Player(id),
    send_correction_(false),
    cur_input_steps_(0),
    deque_overflow_(false),
    total_delay_(0.2f),
    rcon_authorized_(false),
    num_needed_readies_(0),
    steps_input_deque_overfull_(0),
    deque_underflow_(false),
    deque_size_(0)
{
    s_console.addVariable("network_delay",   &network_delay_,  &fp_group_);
    s_console.addVariable("total_delay",     &total_delay_,    &fp_group_);
    s_console.addVariable("deque_overflow",  &deque_overflow_, &fp_group_);
    s_console.addVariable("deque_underflow", &deque_overflow_, &fp_group_);
    s_console.addVariable("deque_size",      &deque_size_,     &fp_group_);
}

//------------------------------------------------------------------------------
/**
 *  Registers delay console var.
 */
ServerPlayer::ServerPlayer(const ServerPlayer & other) :
    Player             (other),
    send_correction_   (other.send_correction_),
    cur_input_steps_   (other.cur_input_steps_),
    deque_overflow_    (other.deque_overflow_),
    total_delay_       (other.total_delay_),
    rcon_authorized_   (other.rcon_authorized_),
    num_needed_readies_(other.num_needed_readies_),
    steps_input_deque_overfull_(0),
    deque_underflow_   (other.deque_underflow_),
    deque_size_        (other.deque_size_)
{ 
    s_console.addVariable("network_delay",   &network_delay_,   &fp_group_);
    s_console.addVariable("total_delay",     &total_delay_,     &fp_group_);
    s_console.addVariable("deque_overflow",  &deque_overflow_,  &fp_group_);
    s_console.addVariable("deque_underflow", &deque_underflow_, &fp_group_);    
    s_console.addVariable("deque_size",      &deque_size_,      &fp_group_);
}

//------------------------------------------------------------------------------
ServerPlayer::~ServerPlayer()
{
}

//------------------------------------------------------------------------------
/**
 *  Corrections states are valid before the step with the contained
 *  number is taken.
 *
 *  \param seq_number Will be set to the seq number the correction
 *  should have if true is returned.
 *
 *  \return True if a correction is to be sent to the client, false
 *  otherwise.
 *
 */
bool ServerPlayer::handleInput(uint8_t & seq_number)
{
    assert(controllable_);
    deque_size_ = input_.size();    // For debugging only
    
    ++cur_input_steps_;    

    // Completely flush input buffer if it grows too large
    if (input_.size() > s_params.get<unsigned>("server.network.max_input_deque_size"))
    {
        while (input_.size() > 1) input_.pop_front();
        seq_number = input_[0].first;
        controllable_->setPlayerInput(input_[0].second);
        cur_input_steps_ = 0;

        deque_overflow_ = true;

#ifdef ENABLE_DEV_FEATURES        
        s_log << Log::debug('D')
              << " Deque overflow for "
              << getName()
              << "\n";
#endif        

        return true;
    }
    deque_overflow_ = false;

    
    
    if (input_.size() > 1)
    {
        steps_input_deque_overfull_++;
        
        // input_[0] is the input currently used in the simulation for
        // this player.  The difference between sequence numbers is
        // the number of physic steps we have to take with this
        // input. We only know the number of steps to take when there
        // is at least another input in the deque.
        unsigned target_steps = seqNumberDifference(input_[1].first, input_[0].first);

        // This is needed e.g. for projectile offset to compensate for
        // network and input deque delay
        float send_input_dt = (float)target_steps / s_params.get<float>("physics.fps");
        total_delay_ *= 1.0f - TOTAL_DELAY_TRACKING_SPEED;
        total_delay_ += TOTAL_DELAY_TRACKING_SPEED * (network_delay_ + (input_.size()-1)*send_input_dt);

        
        if (steps_input_deque_overfull_ > MAX_NUM_STEPS_OVERFULL)
        {
            target_steps--;
            
#ifdef ENABLE_DEV_FEATURES
            s_log << Log::debug('D')
                  << "Shrinking deque size for "
                  << getName()
                  << ".\n";
#endif
        }

        if (cur_input_steps_ >= target_steps)
        {            
            input_.pop_front();
            seq_number = input_[0].first;
            controllable_->setPlayerInput(input_[0].second);

            deque_underflow_ = cur_input_steps_ > target_steps;
            cur_input_steps_ = 0;

#ifdef ENABLE_DEV_FEATURES            
            if (deque_underflow_)
            {
                s_log << Log::debug('D')
                      << "Deque underflow for "
                      << getName()
                      << "\n";
            }
#endif

#ifdef DISABLE_NETWORK_OPTIMIZATIONS
            return true;
#else
            send_correction_ ^= true;
            return send_correction_;
#endif
        } 

    } else steps_input_deque_overfull_ = 0;
    
    return false;
}

//------------------------------------------------------------------------------
void ServerPlayer::setControllable(Controllable * new_controllable)
{
    input_.clear();
    cur_input_steps_ = 0;
    
    if (controllable_)
    {
        controllable_->setPlayerInput(PlayerInput());
        controllable_->setOwner(UNASSIGNED_SYSTEM_ADDRESS);
    }

    controllable_ = new_controllable;

    if (controllable_)
    {
        controllable_->setOwner(id_);
    }
}



//------------------------------------------------------------------------------
void ServerPlayer::enqueueInput(uint8_t seq_number, const PlayerInput & input)
{    
    if (!input_.empty() &&
        (seqNumberDifference(seq_number, input_.rbegin()->first) <= 0 ||
         seqNumberDifference(seq_number, input_.rbegin()->first) > 50))
    {
        // Drop out-of-order packets
        s_log << Log::debug('n')
              << "ServerPlayer::enqueueInput() : Dropped out-of-order packet "
              << (unsigned)seq_number
              << ". Current: "
              << (unsigned)input_.rbegin()->first
              << "\n";

        return;
    }
#ifdef ENABLE_DEV_FEATURES
    else
    {
        s_log << Log::debug('b')
              << "ServerPlayer::enqueueInput "
              << (unsigned)seq_number
              << ", ";

        if (input_.empty())
        {
            s_log << "input is empty.\n";
        } else
        {
            s_log << "diff is "
                  << seqNumberDifference(seq_number, input_.rbegin()->first)
                  << ", input size is "
                  << input_.size()
                  << "\n";
        }
    }
#endif
   
    input_.push_back(std::make_pair(seq_number, input));
}

//------------------------------------------------------------------------------
/**
 *  The total delay a player input experiences before being acted
 *  upon. Network + buffer delay.
 */
float ServerPlayer::getTotalInputDelay() const
{
    return total_delay_;
}

//------------------------------------------------------------------------------
void ServerPlayer::setRconAuthorized(bool authorized)
{
    rcon_authorized_ = authorized;
}

//------------------------------------------------------------------------------
bool ServerPlayer::isRconAuthorized() const
{
    return rcon_authorized_;
}

//------------------------------------------------------------------------------
unsigned ServerPlayer::getNeededReadies() const
{
    return num_needed_readies_;
}

//------------------------------------------------------------------------------
void ServerPlayer::incNeededReadies()
{
    ++num_needed_readies_;
}


//------------------------------------------------------------------------------
void ServerPlayer::decNeededReadies()
{
    --num_needed_readies_;
}

