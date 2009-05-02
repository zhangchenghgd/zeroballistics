
#include "WaypointManagerServer.h"

#include <limits>

#include "Log.h"
#include "Paths.h"
#include "Serializer.h"

#undef min
#undef max


//------------------------------------------------------------------------------
WaypointManagerServer::WaypointManagerServer() :
    w_(1),
    h_(1),
    horz_scale_(1.0f)
{

}

//------------------------------------------------------------------------------
WaypointManagerServer::~WaypointManagerServer()
{

}

//------------------------------------------------------------------------------
void WaypointManagerServer::loadWaypoints(const std::string & lvl_name)
{
    wp_map_.clear();
    open_wp_.clear();

    std::string wp_file = LEVEL_PATH + lvl_name + "/waypoints.bin";

    try
    {
        serializer::Serializer s(wp_file, serializer::SOM_READ | serializer::SOM_COMPRESS);

        unsigned lvl;
        Vector pos;

        s.get(w_);
        s.get(h_);
        s.get(horz_scale_);

        for(unsigned x_index=0; x_index < w_; x_index++)
        {
            std::vector<WaypointServer> vector_of_wps;
            wp_map_.push_back(vector_of_wps);

            for(unsigned z_index=0; z_index < h_; z_index++)
            {
                 s.get(pos);
                 s.get(lvl);

                 WaypointServer wp_server;
                 wp_server.level_ = lvl;
                 wp_server.pos_ = pos;

                 wp_map_[x_index].push_back(wp_server);

                 /// store point in open map if level lesser than 9
                 if(lvl < 9)
                 {
                     open_wp_.push_back(Vector(x_index,0.0,z_index));
                 }
            }
        }
    }
    catch(serializer::IoException e)
    {   
        s_log << Log::warning << " Unable to load Waypoint map for level: " << wp_file << "\n Error: "
              << e.getMessage();
    }

}

//------------------------------------------------------------------------------
std::deque<WaypointServer*> WaypointManagerServer::findPath(WaypointSearchNode * start, WaypointSearchNode * end)
{
    std::deque<WaypointServer*> result;

    // no waypoints loaded here -> bail
    if(open_wp_.empty() || wp_map_.empty()) return result;    

	AStarSearch<WaypointSearchNode> astarsearch;

	unsigned int SearchCount = 0;

		
		// Set Start and goal states
		
		astarsearch.SetStartAndGoalStates( *start, *end );

		unsigned int SearchState;
		unsigned int SearchSteps = 0;

		do
		{
			SearchState = astarsearch.SearchStep();

			SearchSteps++;
		}
		while( SearchState == AStarSearch<WaypointSearchNode>::SEARCH_STATE_SEARCHING );

		if( SearchState == AStarSearch<WaypointSearchNode>::SEARCH_STATE_SUCCEEDED )
		{
				// s_log << "Search found goal state\n";

				WaypointSearchNode *node = astarsearch.GetSolutionStart();
                
                result.push_back(&wp_map_[node->x_][node->z_]);

				int steps = 0;

				//node->PrintNodeInfo();
				for( ;; )
				{
					node = astarsearch.GetSolutionNext();

					if( !node )
					{
						break;
					}

                    result.push_back(&wp_map_[node->x_][node->z_]);

					//node->PrintNodeInfo();
					steps ++;
				
				};

				//s_log << "Solution steps " << steps << "\n";

				// Once you're done with the solution you can free the nodes up
				astarsearch.FreeSolutionNodes();

	
		}
		else if( SearchState == AStarSearch<WaypointSearchNode>::SEARCH_STATE_FAILED ) 
		{
			//s_log << "Search terminated. Did not find goal state\n";		
		}

		// Display the number of loops the search went through
		// s_log << "SearchSteps : " << SearchSteps << "\n";

		SearchCount ++;

		astarsearch.EnsureMemoryFreed();
	

    return result;
}

//------------------------------------------------------------------------------
void WaypointManagerServer::getNearestOpenWaypoint(const Vector & pos, unsigned & x, unsigned & z)
{
    /// use int due to minus calculations for distance
    int x_index = round((pos.x_/horz_scale_) / N_TH_WP);
    int z_index = round((pos.z_/horz_scale_) / N_TH_WP);

    int smallest = std::numeric_limits<int>::max();

    // iterate over accessible points and get the one with the smallest distance to
    // the given point (pos).
    std::vector<Vector>::const_iterator it;
    for (it = open_wp_.begin(); it != open_wp_.end(); ++it)
    {
        /// Chebyshev distance
        int distance = max( abs(x_index - (int)(*it).x_) , 
                            abs(z_index - (int)(*it).z_) );

        if( distance < smallest )
        {
            smallest = distance;
            x = (*it).x_;
            z = (*it).z_;
        }
    }

    // if it is the case that there are no open points at all,
    if(open_wp_.empty())
    {
        x = 0;
        z = 0;
    }

}

//------------------------------------------------------------------------------
void WaypointManagerServer::getRandomOpenWaypoint(unsigned & x, unsigned & z)
{
    Vector pos(0.0,0.0,0.0);

    // create random position inside of map bounds
    pos.x_ = ((float)(rand()%w_)) * horz_scale_ * N_TH_WP;
    pos.z_ = ((float)(rand()%h_)) * horz_scale_ * N_TH_WP;

    getNearestOpenWaypoint(pos, x, z);
}

//------------------------------------------------------------------------------
unsigned WaypointManagerServer::getMapValue(unsigned int x, unsigned int z)
{
    if( x < 0 ||
        x >= wp_map_.size() ||
	     z < 0 ||
	     z >= wp_map_.size()
      )
    {
	    return 9;	 
    }

    return wp_map_[x][z].level_;  
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
WaypointSearchNode::WaypointSearchNode()
{ 
    x_ = z_ = 0; 
}

//------------------------------------------------------------------------------
WaypointSearchNode::WaypointSearchNode(unsigned int px, unsigned int pz) 
{ 
    x_=px; 
    z_=pz; 
}

//------------------------------------------------------------------------------
WaypointSearchNode::~WaypointSearchNode()
{
}

//------------------------------------------------------------------------------
float WaypointSearchNode::GoalDistanceEstimate( WaypointSearchNode &nodeGoal )
{
	float xd = float( ( (float)x_ - (float)nodeGoal.x_ ) );
	float zd = float( ( (float)z_ - (float)nodeGoal.z_ ) );

	return xd + zd;
}

//------------------------------------------------------------------------------
bool WaypointSearchNode::IsGoal( WaypointSearchNode &nodeGoal )
{
	if( (x_ == nodeGoal.x_) &&
		(z_ == nodeGoal.z_) )
	{
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// This generates the successors to the given Node. It uses a helper function called
// AddSuccessor to give the successors to the AStar class. The A* specific initialisation
// is done for each node internally, so here you just set the state information that
// is specific to the application
bool WaypointSearchNode::GetSuccessors( AStarSearch<WaypointSearchNode> *astarsearch, WaypointSearchNode *parent_node )
{

	int parent_x = -1; 
	int parent_z = -1; 

	if( parent_node )
	{
		parent_x = parent_node->x_;
        parent_z = parent_node->z_;
	}
	

	WaypointSearchNode NewNode;

	// push each possible move except allowing the search to go backwards

    // horizontal and vertical directions
    if( (s_waypoint_manager_server.getMapValue( x_-1, z_ ) < 9) 
		&& !((parent_x == (int)x_-1) && (parent_z == (int)z_))
	  ) 
	{
		NewNode = WaypointSearchNode( x_-1, z_ );
		astarsearch->AddSuccessor( NewNode );
	}	

	if( (s_waypoint_manager_server.getMapValue( x_, z_-1 ) < 9) 
		&& !((parent_x == (int)x_) && (parent_z == (int)z_-1))
	  ) 
	{
		NewNode = WaypointSearchNode( x_, z_-1 );
		astarsearch->AddSuccessor( NewNode );
	}	

	if( (s_waypoint_manager_server.getMapValue( x_+1, z_ ) < 9)
		&& !((parent_x == (int)x_+1) && (parent_z == (int)z_))
	  ) 
	{
		NewNode = WaypointSearchNode( x_+1, z_ );
		astarsearch->AddSuccessor( NewNode );
	}	
		
	if( (s_waypoint_manager_server.getMapValue( x_, z_+1 ) < 9) 
		&& !((parent_x == (int)x_) && (parent_z == (int)z_+1))
		)
	{
		NewNode = WaypointSearchNode( x_, z_+1 );
		astarsearch->AddSuccessor( NewNode );
	}	

    // diagonal directions
    if( (s_waypoint_manager_server.getMapValue( x_-1, z_-1 ) < 9) 
		&& !((parent_x == (int)x_-1) && (parent_z == (int)z_-1))
	  ) 
	{
		NewNode = WaypointSearchNode( x_-1, z_-1 );
		astarsearch->AddSuccessor( NewNode );
	}	

	if( (s_waypoint_manager_server.getMapValue( x_+1, z_+1 ) < 9) 
		&& !((parent_x == (int)x_+1) && (parent_z == (int)z_+1))
	  ) 
	{
		NewNode = WaypointSearchNode( x_+1, z_+1 );
		astarsearch->AddSuccessor( NewNode );
	}	

	if( (s_waypoint_manager_server.getMapValue( x_+1, z_-1 ) < 9)
		&& !((parent_x == (int)x_+1) && (parent_z == (int)z_-1))
	  ) 
	{
		NewNode = WaypointSearchNode( x_+1, z_-1 );
		astarsearch->AddSuccessor( NewNode );
	}	
		
	if( (s_waypoint_manager_server.getMapValue( x_-1, z_+1 ) < 9) 
		&& !((parent_x == (int)x_-1) && (parent_z == (int)z_+1))
		)
	{
		NewNode = WaypointSearchNode( x_-1, z_+1 );
		astarsearch->AddSuccessor( NewNode );
	}


	return true;
}

//------------------------------------------------------------------------------
// given this node, what does it cost to move to successor. In the case
// of our map the answer is the map terrain value at this node since that is 
// conceptually where we're moving
// ADDITION: make movement to diagonal successor a bit more expensive so that
//           going the "straight" way gets more attractive
float WaypointSearchNode::GetCost( WaypointSearchNode &successor )
{
	// diagonal
	if(x_ != successor.x_ && z_ != successor.z_)
	{
		return (float) s_waypoint_manager_server.getMapValue( x_, z_ ) + 1.0f;
	}
	else // normal neighbour fields
	{
		return (float) s_waypoint_manager_server.getMapValue( x_, z_ );
	}
}

//------------------------------------------------------------------------------
bool WaypointSearchNode::IsSameState( WaypointSearchNode &rhs )
{
	// same state in a maze search is simply when (x,y) are the same
	if( (x_ == rhs.x_) &&
		(z_ == rhs.z_) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

//------------------------------------------------------------------------------
void WaypointSearchNode::PrintNodeInfo()
{
    s_log << "WP search node: " << x_ << "/" << z_ << "\n";
}



