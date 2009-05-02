
#ifndef TANK_GAME_HUD_TANK_ZB_DEFINED
#define TANK_GAME_HUD_TANK_ZB_DEFINED


#include "GameHudTank.h"


#include "Score.h"


//------------------------------------------------------------------------------
class GameHudTankZb : public GameHudTank
{
 public:
    GameHudTankZb(PuppetMasterClient * master, const std::string & config_file);
    virtual ~GameHudTankZb();

    virtual void onWindowResized();
    virtual void activateHitFeedback();
    virtual void setAttitudeTexture(const std::string & tex);   

 protected:

    virtual void update(float dt);
    
    void setupTankTurretHud(const std::string & attitude_tex);

    void enableUpgradeLight(UPGRADE_CATEGORY category, bool e);

    void setHitFeedbackBlend(float b);
    float getHitFeedbackBlend() const;
    
    osg::ref_ptr<osg::MatrixTransform> body_transform_; ///< Rotation of tank body
    osg::ref_ptr<osg::MatrixTransform> tank_orientation_; ///< Tank orientation visuals are below this transform
    
    osg::ref_ptr<HudAlphaBar> health_bar_;
    osg::ref_ptr<HudAlphaBar> reload_bar_;

    osg::ref_ptr<HudBar> reload_bar_skill1_;
    osg::ref_ptr<HudBar> reload_bar_skill2_;

    osg::ref_ptr<HudAlphaBar>       upgrade_bar_  [NUM_UPGRADE_CATEGORIES];
    osg::ref_ptr<HudTextureElement> upgrade_light_[NUM_UPGRADE_CATEGORIES];

    osg::ref_ptr<HudTextElement> hud_time_limit_;
    osg::ref_ptr<HudAlphaBar>    time_limit_bar_;

    osg::ref_ptr<HudTextElement> score_text_;
    osg::ref_ptr<HudTextElement> health_text_;
    
    osg::ref_ptr<HudTextureElement> hit_feedback_;
};


#endif
