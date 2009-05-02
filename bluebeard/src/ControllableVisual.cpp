

#include "ControllableVisual.h"


#include <osg/MatrixTransform>
#include <osg/BlendFunc>
#include <osgText/Text>

#include "Controllable.h"
#include "UtilsOsg.h"
#include "SceneManager.h"
#include "Paths.h"
#include "OsgNodeWrapper.h"


//------------------------------------------------------------------------------
ControllableVisual::~ControllableVisual()
{
}

//------------------------------------------------------------------------------
void ControllableVisual::operator() (osg::Node *node, osg::NodeVisitor *nv)
{
    RigidBodyVisual::operator()(node, nv);

    ADD_STATIC_CONSOLE_VAR(bool, render_player_names, true);

    // very little perfomance benefit
    if(prev_render_player_names_ == render_player_names) return;
    prev_render_player_names_ = render_player_names;

    if(render_player_names)
    {
        labelgeode_->setNodeMask(NODE_MASK_VISIBLE);
    }
    else
    {
        labelgeode_->setNodeMask(NODE_MASK_INVISIBLE);
    }

}

//------------------------------------------------------------------------------
Matrix ControllableVisual::getTrackingPos(const Vector & offset)
{
    return matOsg2Gl(s_scene_manager.getWorldCoords(osg_wrapper_->getOsgNode()));
}


//------------------------------------------------------------------------------
void ControllableVisual::setLabelText(const std::string & text)
{
    label_->setText(text);
}

//------------------------------------------------------------------------------
void ControllableVisual::setLabelColor(const Color & color)
{
    label_->setColor( osg::Vec4(color.r_,
                                color.g_,
                                color.b_,
                                color.a_) );
}



//------------------------------------------------------------------------------
ControllableVisual::ControllableVisual()
{
}


//------------------------------------------------------------------------------
/**
 *  Creates the osg model for the rigid body.
 */
void ControllableVisual::onModelChanged()
{
    RigidBodyVisual::onModelChanged();
    
    prev_render_player_names_ = false;

    label_ = new osgText::Text();
    label_->setCharacterSize(0.1);
    label_->setFont(FONT_PATH + "eden.ttf");
    label_->setText("Controllable");
    label_->setFontResolution(40,40);
    label_->setAxisAlignment(osgText::Text::SCREEN);
    label_->setDrawMode(osgText::Text::TEXT);

   // label_->setCharacterSizeMode(osgText::Text::OBJECT_COORDS_WITH_MAXIMUM_SCREEN_SIZE_CAPPED_BY_FONT_HEIGHT);
    
    label_->setAlignment(osgText::Text::CENTER_TOP);

    float offset_up = osg_wrapper_->getOsgNode()->getBound()._radius;
    label_->setPosition( osg::Vec3(0, offset_up, 0) );
    label_->setColor( osg::Vec4(1.0f, 1.0f, 1.0f, 1.0f) );

    // Declare a geode to contain the tank's text label:
    labelgeode_ = new osg::Geode();
    labelgeode_->setName("Player Label");
    labelgeode_->addDrawable(label_.get());

    // Disable shader and lighting for text
    labelgeode_->getOrCreateStateSet()->setAttribute(new osg::Program);
    labelgeode_->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    // Because of lodnode blending equation...
    labelgeode_->getOrCreateStateSet()->setAttribute(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    osg_wrapper_->getOsgNode()->addChild(labelgeode_.get());
}

