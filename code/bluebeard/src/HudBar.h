
#ifndef TANK_HUD_BAR_INCLUDED
#define TANK_HUD_BAR_INCLUDED



#include "HudTextureElement.h"



//------------------------------------------------------------------------------
enum HUD_BAR_FILL_DIRECTION
{
    HBFD_LEFT_RIGHT,
    HBFD_RIGHT_LEFT,
    HBFD_TOP_DOWN,
    HBFD_BOTTOM_UP
};


//------------------------------------------------------------------------------
class HudBar : public HudTextureElement
{
 public:

    HudBar(const std::string & section);

    void setValue(float v);
    float getValue();
    
    virtual const char * className () const { return "HudBar"; }
    
 protected:

    virtual void updateVb();

    float value_;

    HUD_BAR_FILL_DIRECTION fill_direction_;
};



#endif
