#ifndef TANKGAME_WAYPOINTMANAGERSERVER_INCLUDED
#define TANKGAME_WAYPOINTMANAGERSERVER_INCLUDED

#include <deque>
#include <map>

#include "Vector.h"
#include "Singleton.h"
#include "AStarSearch.h"


#ifdef FULL_METAL_SOCCER_MODE
const unsigned N_TH_WP = 1;  ///< compensate lower terrain resolution for soccer
#else
const unsigned N_TH_WP = 3;
#endif

//------------------------------------------------------------------------------
class WaypointServer
{
public:
    WaypointServer() {};
    ~WaypointServer() {};

    Vector pos_;
    unsigned short level_;
};

//------------------------------------------------------------------------------
/**
 *  used as a templated class for the A star search algorithm
 **/
class WaypointSearchNode
{
public:

	WaypointSearchNode();
	WaypointSearchNode(unsigned int px, unsigned int pz);

    ~WaypointSearchNode();

	float GoalDistanceEstimate( WaypointSearchNode &nodeGoal );
	bool IsGoal( WaypointSearchNode &nodeGoal );
	bool GetSuccessors( AStarSearch<WaypointSearchNode> *astarsearch, WaypointSearchNode *parent_node );
	float GetCost( WaypointSearchNode &successor );
	bool IsSameState( WaypointSearchNode &rhs );

	void PrintNodeInfo(); 

	unsigned int x_;	 // the (x,z) positions of the node
	unsigned int z_;	

};


#define s_waypoint_manager_server Loki::SingletonHolder<WaypointManagerServer, Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance()
//------------------------------------------------------------------------------
class WaypointManagerServer
{

    DECLARE_SINGLETON(WaypointManagerServer);

public: 

    virtual ~WaypointManagerServer();

    void loadWaypoints(const std::string & lvl_name);

    std::deque<WaypointServer*> findPath(WaypointSearchNode * start, WaypointSearchNode * end);

    void getNearestOpenWaypoint(const Vector & pos, unsigned & x, unsigned & z);
    void getRandomOpenWaypoint(unsigned & x, unsigned & z);

    unsigned getMapValue(unsigned int x, unsigned int z);

private:

    unsigned w_;
    unsigned h_;
    float horz_scale_;

    std::vector<Vector> open_wp_; ///< this vector stores all open waypoints

    std::vector< std::vector<WaypointServer> > wp_map_; ///< the 2D array that stores 
                                                        ///< all the waypoints
};



#endif
