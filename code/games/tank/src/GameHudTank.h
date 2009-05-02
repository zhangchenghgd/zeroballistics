#ifndef TANK_GAME_HUD_TANK_DEFINED
#define TANK_GAME_HUD_TANK_DEFINED

#include <osg/ref_ptr>

#include "Minimap.h"

#include "RegisteredFpGroup.h"



class PuppetMasterClient;
class HudAlphaBar;
class HudBar;
class HudTextElement;

namespace osgText
{
    class Text;
}

namespace osg
{
    class MatrixTransform;
    class Geode;
    class Group;
}

const unsigned NUM_HIT_MARKERS = 4;
const float BLEND_OUT_HIT_MARKER_TIME = 0.5f;


//------------------------------------------------------------------------------
class GameHudTank
{
    friend class GameHudTankUpdater;
 public:
    GameHudTank(PuppetMasterClient * master, const std::string & config_file);
    virtual ~GameHudTank();
    
    void enable(bool e);

    Minimap * getMinimap();

    virtual void onWindowResized();
    
    void activateHitMaker(const Vector & pos);

    virtual void activateHitFeedback() {}

    virtual void setAttitudeTexture(const std::string & tex) {};
   
    void setCrosshair(const std::string & crosshair_section);

 protected:

    virtual void update(float dt);

    void setupHitMarkerHud();
    
    void setHitMarkerBlend(unsigned pos, float b);
    float getHitMarkerBlend(unsigned pos) const;

    
    osg::ref_ptr<osg::Group> group_; ///< "Master" group to enable / disable hud. 
    osg::ref_ptr<osg::Geode> geode_; ///< "Master" geode for all simple drawables.    
    
    osg::ref_ptr<HudTextureElement> hit_marker_[NUM_HIT_MARKERS];

    osg::ref_ptr<HudTextureElement> crosshair_;

    std::auto_ptr<Minimap> minimap_;
    
    PuppetMasterClient * puppet_master_;

    RegisteredFpGroup fp_group_;
};


#endif
