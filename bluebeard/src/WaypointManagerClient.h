#ifndef TANKGAME_WAYPOINTMANAGER_INCLUDED
#define TANKGAME_WAYPOINTMANAGER_INCLUDED

#include <vector>

#include <osg/Geode>
#include <osg/Drawable>
#include <osg/Shape>
#include <osg/ShapeDrawable>

#include "RegisteredFpGroup.h"
#include "Scheduler.h"
#include "PlayerInput.h"

class PuppetMasterClient;
class PlayerInput;

namespace osg
{
    class Geode;
}


#include "WaypointManagerServer.h" ///< only used for N_TH_WP constant

//------------------------------------------------------------------------------
class WaypointClient
{
public:
    WaypointClient(const osg::Vec3 & pos, osg::StateSet * state_set);
    ~WaypointClient();

    void setPosition(const osg::Vec3 & pos);
    const osg::Vec3 & getPosition() const;

    void setLevel(unsigned lvl);
    unsigned getLevel() const;

    osg::Geode * getGeode();
private:

    osg::ref_ptr<osg::Geode> way_geode_;
    osg::ref_ptr<osg::Box> way_box_;
    osg::ref_ptr<osg::ShapeDrawable> way_box_drawable_;

    unsigned short level_;
};


//------------------------------------------------------------------------------
class WaypointManagerClient
{
 public:
    WaypointManagerClient(PuppetMasterClient * puppet_master);
    ~WaypointManagerClient();

    std::string startWaypointing(const std::vector<std::string> & args);
    std::string stopWaypointing(const std::vector<std::string> & args);

    std::string saveWaypoints(const std::vector<std::string> & args);
    std::string loadWaypoints(const std::vector<std::string> & args);

    std::string findPath(const std::vector<std::string> & args);

    void handleLogic(float dt);
    void handleInput(const PlayerInput & input);

 protected:

    PuppetMasterClient * puppet_master_;
    PlayerInput player_input_;

    osg::ref_ptr<osg::Group> waypoints_group_;

    bool waypointing_active_;
    float horz_scale_;

    std::vector< std::vector<WaypointClient> > wp_map_;

    hTask logic_task_;

    RegisteredFpGroup fp_group_;

};

#endif
