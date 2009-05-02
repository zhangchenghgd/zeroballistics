
#include "Profiler.h"

#include "Scheduler.h"
#include "ParameterManager.h"


namespace profiler
{

const unsigned NUM_CALLS_COL_WIDTH  = 10;
const unsigned MSECS_COL_WIDTH      = 10;
const unsigned PERCENTAGE_COL_WIDTH = 10;

const unsigned NAME_COL_WIDTH       = 40;

    
//------------------------------------------------------------------------------
std::ostream & operator<<(std::ostream & out, const ProfileNode & node)
{
    float parent_time = node.getParent()->getFrameTime();
    if (parent_time == 0.0f) parent_time = node.getFrameTime();


    out << " calls: " << std::setw(NUM_CALLS_COL_WIDTH)  << node.getFrameCalls()
        << " msecs: " << std::setw(MSECS_COL_WIDTH)      << node.getFrameTime() * 1000.0f
        << " %: "     << std::setw(PERCENTAGE_COL_WIDTH);

    if (!equalsZero(parent_time))
    {
        float percentage = 100.0f * node.getFrameTime() / parent_time;
        out << percentage;
    } else out << "n.a.";
    out << "\n";

    
    return out;
}



//------------------------------------------------------------------------------
/**
 *  Writes profiler data for the node pointed to by the specified
 *  iterator to a stream. The node must have a parent.
 */
std::ostream & operator<<(std::ostream & out, ProfileIterator & profiler_iterator)
{
    assert(profiler_iterator->getParent() != NULL);

    const ProfileNode * parent = profiler_iterator->getParent();

    if (parent->getParent())
    {
        out << "0  " << std::setw(NAME_COL_WIDTH) << parent->getName() << *parent;
    } else out << "\n";
    
    out << "--------------------------------------------------------------------------------\n";
    
    unsigned n=1;
    float sum_of_times = 0.0f;
    while (!profiler_iterator.isDone())
    {
        bool  hasChild = profiler_iterator->getChild() != NULL;
        
        out << n << (hasChild ? "* " : "  ")
            << std::setw(NAME_COL_WIDTH) << profiler_iterator->getName()
            << *profiler_iterator;
        
        sum_of_times += profiler_iterator->getFrameTime();
        ++n;
        profiler_iterator.next();
    }

    float parent_time = parent->getFrameTime();
    if (parent_time == 0.0f) parent_time = sum_of_times;

    
    float percentage = 0;
    if (!equalsZero(parent_time)) percentage = 100.0f * (parent_time - sum_of_times) / parent_time;
    
    if (percentage > s_params.get<float>("profiler.hide_threshold"))
    {
        out << std::setw(NAME_COL_WIDTH+3) << "   missing   "
            << " calls: " << std::setw(NUM_CALLS_COL_WIDTH) << "n.a."
            << " msecs: " << std::setw(MSECS_COL_WIDTH)     << (parent_time - sum_of_times)*1000.0f
            << " %: "     << std::setw(PERCENTAGE_COL_WIDTH) << percentage << "\n";
    }

   
    return out;
}


//------------------------------------------------------------------------------
/**
 *  Name must be a static string, only the pointer values are compared!
 */
ProfileNode::ProfileNode( const char * name, ProfileNode * parent ) :
    name_( name ),
    total_calls_( 0 ),
    total_time_( 0 ),
    frame_calls_( 0 ),
    frame_time_( 0 ),
    recursion_counter_( 0 ),
    parent_( parent ),
    child_( NULL ),
    sibling_( NULL )
{
    reset();
}


//------------------------------------------------------------------------------
ProfileNode::~ProfileNode()
{
    delete child_;
    delete sibling_;
}

//------------------------------------------------------------------------------
/**
 *  Find the child with the specified name. If it doesn't exist yet,
 *  add it.
 */
ProfileNode * ProfileNode::getOrCreateChild( const char * name )
{
    ProfileNode * child = child_;
    while ( child )
    {
        if ( child->name_ == name )
        {
            return child;
        }
        child = child->sibling_;
    }

    // We didn't find it, so add it
    ProfileNode * node = new ProfileNode( name, this );
    node->sibling_ = child_;
    child_ = node;
    return node;
}

//------------------------------------------------------------------------------
void ProfileNode::clearAll()
{
    DELNULL(child_);
    DELNULL(sibling_);
    reset();
}

//------------------------------------------------------------------------------
void ProfileNode::reset()
{
    total_calls_ = 0;
    total_time_ = 0.0f;

    if ( child_ )
    {
        child_->reset();
    }
    if ( sibling_)
    {
        sibling_->reset();
    }
}


//------------------------------------------------------------------------------
void ProfileNode::calcFrameValues(float frame_count_inv)
{
    frame_time_ = total_time_ * frame_count_inv;
    frame_calls_ = total_calls_ * frame_count_inv;

    if ( child_ )
    {
        child_->calcFrameValues(frame_count_inv);
    }
    if ( sibling_)
    {
        sibling_->calcFrameValues(frame_count_inv);
    }
}

//------------------------------------------------------------------------------
void ProfileNode::call()
{
    total_calls_++;
    if (recursion_counter_++ == 0)
    {
        getCurTime(start_time_);
    }
}


//------------------------------------------------------------------------------
bool ProfileNode::finish()
{
    if ( --recursion_counter_ == 0 && total_calls_ != 0 )
    { 
        TimeValue time;
        getCurTime(time);
        total_time_ += getTimeDiff(time, start_time_) * 0.001f;
    }
    return ( recursion_counter_ == 0 );
}



//------------------------------------------------------------------------------
ProfileIterator::ProfileIterator( const ProfileNode * start )
{
    current_parent_ = start;
    current_child_ = current_parent_->getChild();
}

//------------------------------------------------------------------------------
/**
 *  Resets the child pointer to the first child in the linked list.
 *
 *  \return Whether there is any child.
 */
bool ProfileIterator::first()
{
    current_child_ = current_parent_->getChild();

    return current_child_ != NULL;
}


//------------------------------------------------------------------------------
/**
 *  Sets the child pointer to the next child in the linked list.
 */
void ProfileIterator::next()
{
    current_child_ = current_child_->getSibling();
}


//------------------------------------------------------------------------------
/**
 *  Returns whether there are any more childs.
 */
bool ProfileIterator::isDone() const
{
    return current_child_ == NULL;
}

//------------------------------------------------------------------------------
/**
 *  Enters the child with the specified number in the linked list.
 */
void ProfileIterator::enterChild( int index )
{
    current_child_ = current_parent_->getChild();
    while ( (current_child_) && (index != 0) )
    {
        index--;
        current_child_ = current_child_->getSibling();
    }

    if ( current_child_ && current_child_->getChild() )
    {
        current_parent_ = current_child_;
        current_child_ = current_parent_->getChild();
    }
}

//------------------------------------------------------------------------------
/**
 *  Enters the parent of the current node. Doesn't allow to enter the
 *  root node, so a node pointed to by an iterator always has a
 *  parent.
 */
void ProfileIterator::enterParent( void )
{
    assert(current_parent_);
    if ( current_parent_->getParent() != NULL )
    {
        current_child_ = current_parent_;
        current_parent_ = current_parent_->getParent();
    }
}

//------------------------------------------------------------------------------
const ProfileNode * ProfileIterator::operator->() const
{
    assert(current_child_);
    return current_child_;
}


//------------------------------------------------------------------------------
const ProfileNode & ProfileIterator::operator*() const
{
    assert(current_child_);
    return *current_child_;
}




//------------------------------------------------------------------------------
Profiler::Profiler() : root_( "Root", NULL),
                       current_node_(&root_),
                       iterator_(&root_),
                       reset_(false),
                       refresh_(true),
                       frame_counter_(0)
{
    root_.call();

    s_scheduler.addTask(PeriodicTaskCallback(this, &Profiler::reset),
                        4.9f,
                        "Profile::reset",
                        &fp_group_);   // PPPP    
}


//------------------------------------------------------------------------------
Profiler::~Profiler()
{
}


//------------------------------------------------------------------------------
/**
 *  Writes the timing information of all nodes into the buffer, then
 *  removes all ProfilNodes accumulated until now, and starts over.
 */
const char * Profiler::getSummary()
{
#ifdef ENABLE_PROFILING
    if (!root_.getChild()) return "";
    
    root_.finish();
    root_.calcFrameValues(1.0f);

    std::ostringstream out;

    out << std::left;
    out << std::setprecision(4);

    out << "\n----------------------------- Total Profile ------------------------------------\n";

    out << "Total Frame time: "
        << root_.getFrameTime() * 1000.0f
        << " msecs ("
        << 1.0f / root_.getFrameTime()
        << " FPS)\n";
    
    ProfileIterator it(&root_);
    writeToStream(it, out, 0);

    out << "--------------------------------------------------------------------------------\n\n";
    
    buffer_ = out.str();
    
    root_.clearAll();

    frame_counter_ = 0;
    
    root_.call();

    return buffer_.c_str();
#else
    return "Profiling is disabled. Enable by defining ENABLE_PROFILING.";
#endif

}

//------------------------------------------------------------------------------
/**
 *  Clears all child profile nodes and starts completely anew.
 */
void Profiler::clearAll()
{
    root_.finish();
    root_.clearAll();
    root_.call();
}

//------------------------------------------------------------------------------
/**
 *  Accumulated times will be reset next frame.
 */
void Profiler::reset(float dt)
{
    reset_ = true;
}


//------------------------------------------------------------------------------
void Profiler::refresh(float dt)
{
    refresh_ = true;
}


//------------------------------------------------------------------------------
/**
 *  Must be called every frame to update the stats.
 */
void Profiler::frameMove()
{
    root_.finish();
    
    if (reset_)
    {
        reset_ = false;
        root_.reset(); 
        frame_counter_ = 0;
    } else if (refresh_)
    {
        refresh_ = false;
        writeFrameToBuffer();
    }

    ++frame_counter_;
    
    root_.call();
}

//------------------------------------------------------------------------------
const char * Profiler::getString() const
{
    return buffer_.c_str();
}


//------------------------------------------------------------------------------
void Profiler::enterChild0()
{
    iterator_.enterChild(0);
    refresh_ = true;
}
//------------------------------------------------------------------------------
void Profiler::enterChild1()
{
    iterator_.enterChild(1);
    refresh_ = true;
}
//------------------------------------------------------------------------------
void Profiler::enterChild2()
{
    iterator_.enterChild(2);
    refresh_ = true;
}
//------------------------------------------------------------------------------
void Profiler::enterChild3()
{
    iterator_.enterChild(3);
    refresh_ = true;
}
//------------------------------------------------------------------------------
void Profiler::enterChild4()
{
    iterator_.enterChild(4);
    refresh_ = true;
}
//------------------------------------------------------------------------------
void Profiler::enterChild5()
{
    iterator_.enterChild(5);
    refresh_ = true;
}
//------------------------------------------------------------------------------
void Profiler::enterChild6()
{
    iterator_.enterChild(6);
    refresh_ = true;
}
//------------------------------------------------------------------------------
void Profiler::enterChild7()
{
    iterator_.enterChild(7);
    refresh_ = true;
}
//------------------------------------------------------------------------------
void Profiler::enterChild8()
{
    iterator_.enterChild(8);
    refresh_ = true;
}

//------------------------------------------------------------------------------
void Profiler::enterParent()
{
    iterator_.enterParent();
    refresh_ = true;
}

//------------------------------------------------------------------------------
/**
 *  Steps one level deeper into the tree, if a child already exists
 *  with the specified name then it accumulates the profiling;
 *  otherwise a new child node is added to the profile tree. Only to
 *  be called by ProfileSample.
 */
void Profiler::startProfile( const char * name )
{
    if (name != current_node_->getName())
    {
        current_node_ = current_node_->getOrCreateChild( name );
    } 
	
    current_node_->call();
}

//------------------------------------------------------------------------------
/**
 *  Stops taking time for the currently active profile node and steps
 *  up the hierarchy to its parent. Only to be called by
 *  ProfileSample.
 */
void Profiler::stopProfile()
{
    if (current_node_->finish())
    {
        current_node_ = current_node_->getParent();
    }
}


//------------------------------------------------------------------------------
/**
 *  Writes fps, triangle count and debug info into the buffer_ variable.
 */
void Profiler::writeFrameToBuffer()
{
    if (!frame_counter_) return;

    
    std::ostringstream out;
    
    root_.calcFrameValues(1.0f / frame_counter_);
    
    out << std::left;
    out << std::setprecision(3);
    
    out << 1.0f / root_.getFrameTime() << " fps\n\n";

    if (iterator_.first())
    {
        out << iterator_;
    }
    
    buffer_ = out.str();
}

//------------------------------------------------------------------------------
/**
 *  Writes the data from all stored Profiles to the text buffer.
 *
 *  \todo percentage threshold in parameterfile
 */
void Profiler::writeToStream(ProfileIterator & it, std::ostringstream & stream, unsigned offset)
{
    float parent_time = it->getParent()->getFrameTime();

    std::string pipes = "";
    if (offset != 0)
    {
        for (unsigned l=0; l<offset-1; ++l) pipes += "| ";
        pipes += "|->";
    }
    
    float sum_of_times = 0.0f;
    unsigned n=0;
    while (!it.isDone())
    {
        stream << std::setw(55) << (pipes + it->getName());
        stream << *it;

        if (it->getChild())
        {
            it.enterChild(n);
            writeToStream(it, stream, offset+1);
        }

        sum_of_times += it->getFrameTime();
        
        it.next();
        ++n;
    }

    if (parent_time == 0.0f) parent_time = sum_of_times;
    
    float percentage = equalsZero(parent_time) ? 0.0f : 100.0f * (parent_time - sum_of_times) / parent_time;

    if (percentage > s_params.get<float>("profiler.hide_threshold"))
    {
        pipes = "";
        if (offset != 0)
        {
            for (unsigned l=0; l<offset-1; ++l) pipes += "| ";
            pipes += "+--";
        }

        stream << std::setw(74) << (pipes + "missing")
               << "msecs: "     << std::setw(11) << (parent_time - sum_of_times) * 1000.0f
               << "%: "         <<std::setw(6)  << percentage << "\n";
    }

    it.enterParent();
}

} //namespace profiler
