
#include "WaypointManagerClient.h"

#include "Paths.h"
#include "PuppetMasterClient.h"
#include "TerrainData.h"
#include "SceneManager.h"
#include "Serializer.h"
#include "UtilsOsg.h"

#include "GameLogicClient.h"

//------------------------------------------------------------------------------
WaypointClient::WaypointClient(const osg::Vec3 & pos, osg::StateSet * state_set) :
    level_(9)
{
    way_geode_ = new osg::Geode();
    way_box_ = new osg::Box;
    way_box_drawable_ = new osg::ShapeDrawable( way_box_.get() );


    way_box_->setCenter(pos);
    way_box_->setHalfLengths(osg::Vec3(0.1, 0.1, 0.1));

    way_box_drawable_->setColor(osg::Vec4(0.0f, 0.0f, 1.0f, 1.0f) ); // blue
    way_box_drawable_->setStateSet(state_set);

    way_geode_->addDrawable( way_box_drawable_.get() );
}

//------------------------------------------------------------------------------
WaypointClient::~WaypointClient()
{
}

//------------------------------------------------------------------------------
void WaypointClient::setPosition(const osg::Vec3 & pos)
{
    way_box_->setCenter(pos);
}

//------------------------------------------------------------------------------
const osg::Vec3 & WaypointClient::getPosition() const
{
    return way_box_->getCenter();
}

//------------------------------------------------------------------------------
void WaypointClient::setLevel(unsigned lvl)
{
    level_ = lvl;

    switch(lvl)
    {
    case 1:
        way_box_drawable_->setColor(osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f) ); // white
        break;
    case 9:
        way_box_drawable_->setColor(osg::Vec4(0.0f, 0.0f, 1.0f, 1.0f) ); // blue
        break;
    default:
        way_box_drawable_->setColor(osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f) ); // red - error
        break;
    }
}

//------------------------------------------------------------------------------
unsigned WaypointClient::getLevel() const
{
    return level_;
}

//------------------------------------------------------------------------------
osg::Geode * WaypointClient::getGeode()
{
    return way_geode_.get();
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
WaypointManagerClient::WaypointManagerClient(PuppetMasterClient * puppet_master) :
    puppet_master_(puppet_master),
    waypointing_active_(false),
    horz_scale_(0.0),
    logic_task_(INVALID_TASK_HANDLE)
{

    waypoints_group_ = new osg::Group();


    s_console.addFunction("startWaypointing",
                          ConsoleFun(this, &WaypointManagerClient::startWaypointing), &fp_group_);

    s_console.addFunction("stopWaypointing",
                          ConsoleFun(this, &WaypointManagerClient::stopWaypointing), &fp_group_);

    s_console.addFunction("saveWaypoints",
                          ConsoleFun(this, &WaypointManagerClient::saveWaypoints), &fp_group_);

    s_console.addFunction("loadWaypoints",
                          ConsoleFun(this, &WaypointManagerClient::loadWaypoints), &fp_group_);
}

//------------------------------------------------------------------------------
WaypointManagerClient::~WaypointManagerClient()
{
    s_scene_manager.scheduleNodeForDeletion(waypoints_group_.get());
}

//------------------------------------------------------------------------------
std::string WaypointManagerClient::startWaypointing(const std::vector<std::string> & args)
{
    /// add handle  Logic task for waypointing
    if(logic_task_ == INVALID_TASK_HANDLE)
    {
        logic_task_ = s_scheduler.addTask(PeriodicTaskCallback(this, &WaypointManagerClient::handleLogic),
                        1.0f / 25.0f,
                        "WaypointManagerClient::handleLogic",
                        &fp_group_); 
    }

    const terrain::TerrainData * td = puppet_master_->getGameState()->getTerrainData();

    unsigned res_x = td->getResX() / 2;
    unsigned res_z = td->getResZ() / 2;

    horz_scale_ = 2.0 * td->getHorzScale();

    osg::StateSet * state_set = new osg::StateSet();
    state_set->setAttribute(new osg::Program, osg::StateAttribute::OVERRIDE);
    state_set->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    state_set->setRenderBinDetails(BN_DEFAULT, "RenderBin");
    state_set->setMode(GL_BLEND, osg::StateAttribute::OFF);

    

    for(unsigned x = 0; x < res_x; x+=N_TH_WP)
    {
        std::vector<WaypointClient> vector_of_wps;
        wp_map_.push_back(vector_of_wps);

        for(unsigned z = 0; z < res_z; z+=N_TH_WP)
        {            
            float y = td->getHeightAtGrid(x,z,1);
            
            osg::Vec3 pos(osg::Vec3(((float)x) * horz_scale_, y + 0.05f , ((float)z) * horz_scale_ ));
            WaypointClient waypoint(pos, state_set);

            waypoints_group_->addChild(waypoint.getGeode());
                    
            wp_map_[x/N_TH_WP].push_back(waypoint);
        }
    }

    s_scene_manager.addNode(waypoints_group_.get());

    waypointing_active_ = true;

    return "";
}

//------------------------------------------------------------------------------
std::string WaypointManagerClient::stopWaypointing(const std::vector<std::string> & args)
{
    /// remove handle logic task
    s_scheduler.removeTask(logic_task_, &fp_group_);
    logic_task_ = INVALID_TASK_HANDLE;

    s_scene_manager.scheduleNodeForDeletion(waypoints_group_.get());
    waypoints_group_->removeChildren(0, waypoints_group_->getNumChildren());

    for(unsigned n = 0; n < wp_map_.size(); n++)
    {
        wp_map_[n].clear();
    }
    wp_map_.clear();

    waypointing_active_ = false;

    return "";
}

//------------------------------------------------------------------------------
std::string WaypointManagerClient::saveWaypoints(const std::vector<std::string> & args)
{
    if(!waypointing_active_) return "waypointing not activated";

    std::string wp_file = LEVEL_PATH + puppet_master_->getGameLogic()->getLevelName() + "/waypoints.bin";
    
    serializer::Serializer s(wp_file, serializer::SOM_WRITE | serializer::SOM_COMPRESS);

    unsigned w = 0;
    unsigned h = 0;

    w = wp_map_.size();
    if(w > 0)
    {
        h = wp_map_[0].size();
    }

    s.put(w);
    s.put(h);
    s.put(horz_scale_);

    for(unsigned x_index=0; x_index < w; x_index++)
    for(unsigned z_index=0; z_index < h; z_index++)
    {
         s.put(vecOsg2Gl(wp_map_[x_index][z_index].getPosition()));
         s.put(wp_map_[x_index][z_index].getLevel());
    }

    return "waypoints saved.";
}

//------------------------------------------------------------------------------
std::string WaypointManagerClient::loadWaypoints(const std::vector<std::string> & args)
{
    if(!waypointing_active_) return "waypointing not activated";    

    std::string wp_file = LEVEL_PATH + puppet_master_->getGameLogic()->getLevelName() + "/waypoints.bin";
    
    serializer::Serializer s(wp_file, serializer::SOM_READ | serializer::SOM_COMPRESS);

    unsigned w = 0;
    unsigned h = 0;
    Vector pos;
    unsigned lvl;


    s.get(w);
    s.get(h);
    s.get(horz_scale_);

    for(unsigned x_index=0; x_index < w; x_index++)
    for(unsigned z_index=0; z_index < h; z_index++)
    {

         s.get(pos);
         s.get(lvl);
         wp_map_[x_index][z_index].setLevel(lvl);
         wp_map_[x_index][z_index].setPosition(vecGl2Osg(pos));
    }

    return "waypoints loaded.";
}

//------------------------------------------------------------------------------
void WaypointManagerClient::handleLogic(float dt)
{
    if(!waypointing_active_) return;

    Vector cam_pos = s_scene_manager.getCamera().getTransform().getTranslation();

    unsigned x_index = round((cam_pos.x_/horz_scale_) / N_TH_WP);
    unsigned z_index = round((cam_pos.z_/horz_scale_) / N_TH_WP);

    // make sure that index is inside of array bounds 
    if((x_index >= 0) && (z_index >= 0) &&
       (x_index < wp_map_.size()) && (z_index < wp_map_[0].size()) )
    {
        if(player_input_.fire1_)
        {
            wp_map_[x_index][z_index].setLevel(1);
        } else if(player_input_.fire2_)
        {
            wp_map_[x_index][z_index].setLevel(9);
        }
    }

}

//------------------------------------------------------------------------------
void WaypointManagerClient::handleInput(const PlayerInput & input)
{
    if(!waypointing_active_) return;

    player_input_ = input;
}
