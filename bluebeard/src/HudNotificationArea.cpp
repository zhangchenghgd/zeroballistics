
#include "HudNotificationArea.h"

#include <osg/Geode>


#include "HudTextElement.h"
#include "SceneManager.h"
#include "ParameterManager.h"
#include "HudTextureElement.h"


//------------------------------------------------------------------------------
HudNotificationArea::HudNotificationArea(const std::string & section,
                                         osg::Geode * geode) :
    last_line_(0),
    last_nonempty_line_(0),
    section_(section)
{
    label_.resize(s_params.get<unsigned>("hud." + section_ + ".num_lines"));

    float max_text_width = 0.0;
    try
    {
        max_text_width = (float)s_params.get<unsigned>("hud." + section_ + ".text_width") /
                                s_params.get<float>("hud.ref_screen_height");
    } catch (ParamNotFoundException & e) {}
    
    for (unsigned l=0; l<label_.size(); ++l)
    {
        label_[l] = new HudTextElement(section);
        label_[l]->setMaximumWidth(max_text_width); 
        geode->addDrawable(label_[l].get());
    }

    addLine("");
}

//------------------------------------------------------------------------------
HudNotificationArea::~HudNotificationArea()
{
    for (unsigned l=0; l<label_.size(); ++l)
    {
        assert(label_[l]->getNumParents() == 1);
        ((osg::Geode*)label_[l]->getParent(0))->removeDrawable(label_[l].get());
    }
}


//------------------------------------------------------------------------------
void HudNotificationArea::clear()
{
    for (unsigned l=0; l<label_.size(); ++l)
    {
        label_[l]->setText("");
    }
}


//------------------------------------------------------------------------------
void HudNotificationArea::addLine(const std::string & msg, const Color & color)
{
    last_line_ = last_line_ == 0 ? label_.size()-1 : last_line_-1;
    label_[last_line_]->setText(msg);
    label_[last_line_]->setColor(osg::Vec4(color.r_,
                                           color.g_,
                                           color.b_,
                                           color.a_));

    if (msg != "\n") last_nonempty_line_ = last_line_;

    // push up the text if it takes more than 1 line
    unsigned lc = label_[last_line_]->getLineCount();
    if (lc > 1)
    {
        for (unsigned i=0; i<lc-1; ++i) addLine("\n");
    }
    
    recalcTextPos();
}

//------------------------------------------------------------------------------
void HudNotificationArea::appendToLine(const std::string & msg)
{
    unsigned cur_line_count = label_[last_nonempty_line_]->getLineCount();
    label_[last_nonempty_line_]->setText(label_[last_nonempty_line_]->getText().createUTF8EncodedString() + msg);
    unsigned new_line_count = label_[last_nonempty_line_]->getLineCount();

    // push up the text if it takes more than 1 line
    if (cur_line_count != new_line_count)
    {
        for (unsigned i=0; i<new_line_count-cur_line_count; ++i) addLine("\n");        
    }
}


//------------------------------------------------------------------------------
/**
 *  After a screen resize, the aspect ratio and with it the required
 *  text position may have changed.
 */
void HudNotificationArea::recalcTextPos()
{
    Vector2d pos = HudTextureElement::getScreenCoords(section_);
    
    for (unsigned l=0; l<label_.size(); ++l)
    {
        osgText::Text * cur_label = label_[(last_line_ + l) % label_.size()].get();
        cur_label->setPosition(osg::Vec3(pos.x_,
                                         pos.y_ + (float)l*s_params.get<float>("hud." + section_ + ".char_size") /
                                         s_params.get<float>("hud.ref_screen_height"),
                                         1.0f));
    }
}
