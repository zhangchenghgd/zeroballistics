

#ifndef TANK_MINE_VISUAL_INCLUDED
#define TANK_MINE_VISUAL_INCLUDED

#include "RigidBodyVisual.h"
#include "Team.h"

struct SystemAddress;

namespace osg
{
    class Geode;
}

//------------------------------------------------------------------------------
class TankMineVisual : public RigidBodyVisual
{
 public:
    virtual ~TankMineVisual();
    VISUAL_IMPL(TankMineVisual);

    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv);

    static void setLocalPlayerTeam(TEAM_ID tid); // used to show mine warning billboard
    static void setLocalPlayerId(const SystemAddress & player_id); // used to show mine warning billboard
    
 protected:
    TankMineVisual();

    virtual void onModelChanged();

    osg::ref_ptr<osg::Geode> warning_billboard_;

    static TEAM_ID local_player_team_id_;  ///< If this is valid,
                                           ///local_player_id_ must be
                                           ///UNASSIGNED_SYSTEM_ADDRESS. Used
                                           ///to determine which mines
                                           ///receive a warning
                                           ///billboard.
    static SystemAddress local_player_id_; ///< If this is valid,
                                           ///local_player_team_id_
                                           ///must be
                                           ///INVALID_TEAM_ID. Used to
                                           ///determine which mines
                                           ///receive a warning
                                           ///billboard.
};





#endif
