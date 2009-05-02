#ifndef TANK_GAMEHudCONQUEROR_DEFINED
#define TANK_GAMEHudCONQUEROR_DEFINED

#include <osg/ref_ptr>

#include "Minimap.h"

#include "Score.h"
#include "RegisteredFpGroup.h"



class PuppetMasterClient;
class HudAlphaBar;
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

//------------------------------------------------------------------------------
class GameHudTank
{
 public:
    GameHudTank(PuppetMasterClient * master);
    virtual ~GameHudTank();

    void update(float dt);
    
    void enable(bool e);

    Minimap * getMinimap();

    void onWindowResized();
    
    void activateHitMaker(const Vector & pos);
    void activateHitFeedback();

    void setAttitudeTexture(const std::string & tex);
   
    void setCrosshair(const std::string & crosshair_section);

 protected:
    
    void setupTankTurretHud(const std::string & attitude_tex);
    void setupHitMarkerHud();

    void enableUpgradeLight(UPGRADE_CATEGORY category, bool e);

    void setHitMarkerBlend(unsigned pos, float b);
    float getHitMarkerBlend(unsigned pos) const;

    void setHitFeedbackBlend(float b);
    float getHitFeedbackBlend() const;
    
    osg::ref_ptr<osg::Group> group_; ///< "Master" group to enable / disable hud. 
    osg::ref_ptr<osg::Geode> geode_; ///< "Master" geode for all simple drawables.
    
    osg::ref_ptr<osg::MatrixTransform> body_transform_; ///< Rotation of tank body
    osg::ref_ptr<osg::MatrixTransform> tank_orientation_; ///< Tank orientation visuals are below this transform
    
    osg::ref_ptr<HudAlphaBar> health_bar_;
    osg::ref_ptr<HudAlphaBar> reload_bar_;

    osg::ref_ptr<HudAlphaBar>       upgrade_bar_  [NUM_UPGRADE_CATEGORIES];
    osg::ref_ptr<HudTextureElement> upgrade_light_[NUM_UPGRADE_CATEGORIES];

    osg::ref_ptr<HudTextElement> hud_time_limit_;
    osg::ref_ptr<HudAlphaBar>    time_limit_bar_;

    osg::ref_ptr<HudTextElement> score_text_;
    osg::ref_ptr<HudTextElement> health_text_;
//    osg::ref_ptr<HudTextElement> ammo_text_;
    
    osg::ref_ptr<HudTextureElement> hit_marker_[NUM_HIT_MARKERS];
    osg::ref_ptr<HudTextureElement> hit_feedback_;

    osg::ref_ptr<HudTextureElement> crosshair_;

    Minimap minimap_;
    
    PuppetMasterClient * puppet_master_;

    RegisteredFpGroup fp_group_;
};


#endif
