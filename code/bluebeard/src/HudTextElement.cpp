
#include "HudTextElement.h"

#include "ParameterManager.h"
#include "Vector2d.h"
#include "Datatypes.h"
#include "Paths.h"
#include "HudTextureElement.h"
#include "SceneManager.h" // for render bin

//------------------------------------------------------------------------------
HudTextElement::HudTextElement(const std::string & section) :
    section_(section)
{
    setCharacterSize(s_params.get<float>("hud." + section + ".char_size") /
                     s_params.get<float>("hud.ref_screen_height"));

    osgText::Font * font = osgText::readFontFile(FONT_PATH + s_params.get<std::string>("hud." + section + ".font_name"));
    Color color = s_params.get<Color>("hud." + section + ".color");


    
    setFont(font);
    setFontResolution(40,40);

    setAxisAlignment(osgText::Text::SCREEN);
        
    setColor( osg::Vec4(color.r_,
                        color.g_,
                        color.b_,
                        color.a_) );


    setAlignment((osgText::Text::AlignmentType)s_params.get<unsigned>("hud." + section + ".text_align"));


    float backdrop_offset = s_params.get<float>("hud." + section + ".backdrop_offset");
    if (backdrop_offset)
    {
        setBackdropType(osgText::Text::OUTLINE);
        setBackdropOffset(backdrop_offset);
        setBackdropImplementation(osgText::Text::NO_DEPTH_BUFFER);
        setBackdropColor(osg::Vec4(0,0,0,1));
    }

    getOrCreateStateSet()->setRenderBinDetails(BN_HUD_TEXT, "RenderBin");

    recalcTextPos();
}

//------------------------------------------------------------------------------
void HudTextElement::recalcTextPos()
{
    Vector2d pos = HudTextureElement::getScreenCoords(section_);
    setPosition(osg::Vec3(pos.x_, pos.y_, 1.0f));
}
