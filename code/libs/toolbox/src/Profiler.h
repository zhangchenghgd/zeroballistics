
#ifndef STUNTS_PROFILER_INCLUDED
#define STUNTS_PROFILER_INCLUDED

#include <sstream>
#include <iomanip>
#include <ostream>

#include "Datatypes.h"
#include "Singleton.h"
#include "TimeStructs.h"
#include "Scheduler.h"

namespace profiler
{
    
class Profiler;
class ProfileNode;
class ProfileIterator;
class ProfileSample;

std::ostream & operator<<(std::ostream & out, const ProfileNode & node);
std::ostream & operator<<(std::ostream & out, ProfileIterator & profile_iterator);
 

//------------------------------------------------------------------------------
/**
 *  A node in the Profile Hierarchy Tree.
 *
 *  Nodes at the same level are stored in a linked list through the
 *  sibling_ - pointer.
 */
class ProfileNode
{
 public:
    ProfileNode( const char * name, ProfileNode * parent );
    ~ProfileNode();

    ProfileNode * getOrCreateChild(  const char * name );
    ProfileNode * getParent()  const { return parent_; }
    ProfileNode * getSibling() const { return sibling_; }
    ProfileNode * getChild()   const { return child_; }

    void clearAll();
    void reset();
    void calcFrameValues(float frame_count_inv);
    
    void call();
    bool finish();

    const char * getName()       const { return name_; }
    float        getFrameCalls() const { return frame_calls_; }
    float        getFrameTime()  const { return frame_time_; }

 private:
    const char * name_;
    
    unsigned total_calls_;       ///< The total number of times this node was called since the last reset()
    float    total_time_;        ///< The total time spent in this node in seconds.
    
    float    frame_calls_;         ///< The number of times a frame this node was called.
    float    frame_time_;          ///< The time spent per frame in this node in seconds.
    
    TimeValue start_time_;        ///< The time the current invocation of this node started.
    unsigned recursion_counter_; ///< Used to track how often this node has been called in a row.

    ProfileNode * parent_;
    ProfileNode * child_;
    ProfileNode * sibling_;
};




//------------------------------------------------------------------------------ 
/**
 *  An iterator to navigate through the tree.
 */
class ProfileIterator
{
 public:
    ProfileIterator( const ProfileNode * start );

    bool first(void);
    void next(void);
    bool isDone(void) const;

    void enterChild( int index );
    void enterParent();

    const ProfileNode * operator->() const;
    const ProfileNode & operator*() const;
 private:

    const ProfileNode * current_parent_;
    const ProfileNode * current_child_;
};




#define s_profiler Loki::SingletonHolder<profiler::Profiler, Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance()
//------------------------------------------------------------------------------
class Profiler
{
    DECLARE_SINGLETON(Profiler);
    friend class profiler::ProfileSample;
 public:

    virtual ~Profiler();
    
    const char * getSummary();
    void clearAll();

    void reset(float dt);
    void refresh(float dt);

    void frameMove();
    const char * getString() const;

    void enterChild0();
    void enterChild1();
    void enterChild2();
    void enterChild3();
    void enterChild4();
    void enterChild5();
    void enterChild6();
    void enterChild7();
    void enterChild8();
    void enterParent();

 private:
    void startProfile( const char * name );
    void stopProfile();

    void writeFrameToBuffer();
    void writeToStream(ProfileIterator & it, std::ostringstream & stream, unsigned offset);


    ProfileNode root_;
    ProfileNode * current_node_;
    ProfileIterator iterator_;

    bool reset_;
    bool refresh_;                ///< Indicates whether the display
                                  ///must be refreshed (entered a
                                  ///child or parent node).
    
    unsigned frame_counter_;
    
    std::string buffer_;          ///< A multiline-string containing profile information.

    RegisteredFpGroup fp_group_;
};


//------------------------------------------------------------------------------
/**
 *  ProfileSampleClass is a simple way to profile a function's scope
 *  Use the PROFILE macro at the start of scope to time
 */
class ProfileSample
{
 public:
    ProfileSample( const char * name )
	{ 
            s_profiler.startProfile( name ); 
	}
	
    ~ProfileSample()
	{ 
            s_profiler.stopProfile(); 
	}
};

#define	PROFILE( name ) profiler::ProfileSample _profile_sample_( #name )


} // namespace profiler

#endif // #ifndef STUNTS_PROFILER_INCLUDED
