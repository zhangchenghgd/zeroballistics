
#include "HudAlphaBar.h"

#include "ParameterManager.h"
#include "Vector2d.h"
#include "SceneManager.h"



//------------------------------------------------------------------------------
HudAlphaBar::HudAlphaBar(const std::string & section) :
    HudTextureElement(section),
    value_(1.0f)
{
    setValue(0.0f);
}


//------------------------------------------------------------------------------
void HudAlphaBar::setValue(float val)
{
    val = clamp(val, 0.0f, 1.0f);
    if (val == value_) return;
    value_ = val;

    Color color  = s_params.get<Color>   ("hud." + section_ + ".color");
    
    osg::Vec4Array * osg_color = new osg::Vec4Array(1);
    setColorArray(osg_color);
    (*osg_color)[0].set(color.r_, color.g_, color.b_, value_);
}

//------------------------------------------------------------------------------
float HudAlphaBar::getValue()
{
    return value_;
}

