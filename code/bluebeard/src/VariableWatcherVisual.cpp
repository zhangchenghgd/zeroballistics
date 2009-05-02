
#include "VariableWatcherVisual.h"


#include <osg/Geode>
#include <osg/Point>
#include <osg/BlendFunc>

#include <osgText/Text>


#include "VariableWatcher.h"
#include "SceneManager.h"
#include "ParameterManager.h"
#include "Paths.h"



//------------------------------------------------------------------------------
/**
 *  Passes the update call to VariableWatcherVisual.
 */
class VariableWatcherVisualUpdateCallback : public osg::Drawable::UpdateCallback
{
  public:
    virtual void update (osg::NodeVisitor *, osg::Drawable * drawable)
        {
            ((VariableWatcherVisual*)drawable)->update();
        }
};


//------------------------------------------------------------------------------
/**
 *  Adds itself to scenemanager root node
 */
VariableWatcherVisual::VariableWatcherVisual()
{    
    geode_ = new osg::Geode();    
    geode_->setName("Variable Watcher");    
    geode_->addDrawable(this);
    s_scene_manager.addHudNode(geode_);

    osg::ref_ptr<osg::StateSet> state_set = getOrCreateStateSet();

    osg::Point * p = new osg::Point();
    p->setSize(2);
    state_set->setAttribute(p);
    
    setUseDisplayList(false);

    setUpdateCallback(new VariableWatcherVisualUpdateCallback);
}


//------------------------------------------------------------------------------
void VariableWatcherVisual::drawImplementation(osg::RenderInfo& renderInfo) const
{
    float offset_x = s_params.get<float>("variable_watcher.offset_x");
    float offset_y = s_params.get<float>("variable_watcher.offset_y");
    float units = s_params.get<float>("variable_watcher.units_per_point");
    float height = s_params.get<float>("variable_watcher.graph_height");
    float space = s_params.get<float>("variable_watcher.graph_space");

    glPushAttrib(GL_CURRENT_BIT);
    
    for (unsigned i=0; i<s_variable_watcher.getGraphedVars().size(); ++i)
    {
        const GraphedVar & cur_var = *s_variable_watcher.getGraphedVars()[i];

        float min,max;
        cur_var.calcMinMax(min,max);
        
        label_[i]->setText(toString(max) + "\n" +
                           cur_var.getName() + ": " + cur_var.getCurValue() + "\n" +
                           toString(min));
        label_[i]->setPosition( osg::Vec3(offset_x + 
                                          units*(NUM_WATCHER_SAMPLES+5),
                                          offset_y, 1.0) );
        
        renderGraphedVar(s_variable_watcher.getGraphedVars()[i], offset_x, offset_y);

        offset_y -= height + space;
    }

    glPopAttrib();
}


//------------------------------------------------------------------------------
osg::Object* VariableWatcherVisual::cloneType() const
{
    return new VariableWatcherVisual();
}


//------------------------------------------------------------------------------
osg::Object* VariableWatcherVisual::clone(const osg::CopyOp& copyop) const
{
    assert(false);
    return NULL;
}


//------------------------------------------------------------------------------
/**
 *  Create any new labels we need / destroy unneeded labels.
 */
void VariableWatcherVisual::update()
{
    // Delete unneeded labels
    for (unsigned i=s_variable_watcher.getGraphedVars().size();
         i < label_.size(); ++i)
    {
        geode_->removeDrawable(label_[i].get());
    }
    
    unsigned num_labels = label_.size();
    label_.resize(s_variable_watcher.getGraphedVars().size());
    
    for (unsigned l=num_labels; l<s_variable_watcher.getGraphedVars().size(); ++l)
    {
        label_[l] = new osgText::Text();

        label_[l]->setCharacterSize(0.02);
        label_[l]->setFont(FONT_PATH + "FreeMono.ttf");
        label_[l]->setFontResolution(30,30);
        label_[l]->setAxisAlignment(osgText::Text::SCREEN);
        label_[l]->setAlignment(osgText::Text::LEFT_CENTER);
        label_[l]->setColor( osg::Vec4(1,1,1,1) );

        geode_->addDrawable(label_[l].get());
    }
}


//------------------------------------------------------------------------------
void VariableWatcherVisual::renderGraphedVar(const GraphedVar * var, float x, float y) const
{
    float graph_height = s_params.get<float>("variable_watcher.graph_height");
    float half_height = graph_height * 0.5;
    float units_per_point = s_params.get<float>("variable_watcher.units_per_point");
    float min, max;
    var->calcMinMax(min, max);
    
    // box around graph
    glColor4f(0.3f, 0.3f, 0.3f, 0.4f);    
    glBegin(GL_QUADS);
    glVertex2f(x,               y - half_height - 0.002);
    glVertex2f(x+NUM_WATCHER_SAMPLES * units_per_point, y - half_height - 0.002);
    glVertex2f(x+NUM_WATCHER_SAMPLES * units_per_point, y + half_height + 0.002);
    glVertex2f(x,               y + half_height + 0.002);
    glEnd();    
    
    
    // plot values
    glColor3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_POINTS);
    for (unsigned cur_value = 0; cur_value < NUM_WATCHER_SAMPLES; ++cur_value)
    {
        float scaled_value = var->getScaledValue(cur_value);
        assert(scaled_value >= -0.5f &&
               scaled_value <=  0.5f);
        glVertex2f(x + (float)cur_value*units_per_point, y + scaled_value * graph_height);
    }
    glEnd();

    // zero line
    if ((min < 0) && (max > 0))
    {
        float zero_height = (float)graph_height*max / (max - min);
        glBegin(GL_LINES);
        glColor3f(0.0f, 1.0f, 0.0f);
        glVertex2f(x,                                               y - zero_height + half_height);
        glVertex2f(x+NUM_WATCHER_SAMPLES * units_per_point, y - zero_height + half_height);
        glEnd();
    }    
}
