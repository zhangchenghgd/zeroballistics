
#include "HudBar.h"

#include "ParameterManager.h"
#include "Vector2d.h"
#include "SceneManager.h"



//------------------------------------------------------------------------------
HudBar::HudBar(bool vertical, const std::string & section) :
    HudTextureElement(section),
    value_(0.0f),
    vertical_(vertical)
{
    setValue(1.0f);
}


//------------------------------------------------------------------------------
void HudBar::setValue(float val)
{
    val = clamp(val, 0.0f, 1.0f);
    if (val == value_) return;
    value_ = val;

    updateVb();
}

//------------------------------------------------------------------------------
float HudBar::getValue()
{
    return value_;
}


//------------------------------------------------------------------------------
void HudBar::updateVb()
{
    // Recreate buffers to reflect new size
    osg::Vec3Array * vertex = new osg::Vec3Array(4); // vec3 or computeBounds will complain
    setVertexArray(vertex);

    osg::Vec2Array* texcoord = new osg::Vec2Array(4);
    setTexCoordArray(0,texcoord);

    Vector2d pos;
    float size;
    getPositionAndSize(pos, size);

    if (vertical_)
    {
        (*vertex)[0].set(pos.x_,                    pos.y_,             0.0f);
        (*vertex)[1].set(pos.x_+size*aspect_ratio_, pos.y_,             0.0f);
        (*vertex)[2].set(pos.x_+size*aspect_ratio_, pos.y_+size*value_, 0.0f);
        (*vertex)[3].set(pos.x_,                    pos.y_+size*value_, 0.0f);

        (*texcoord)[0].set(0,0);
        (*texcoord)[1].set(1,0);
        (*texcoord)[2].set(1,value_);
        (*texcoord)[3].set(0,value_);
    } else
    {    
        (*vertex)[0].set(pos.x_,                              pos.y_,      0.0f);
        (*vertex)[1].set(pos.x_+size*aspect_ratio_*value_,    pos.y_,      0.0f);
        (*vertex)[2].set(pos.x_+size*aspect_ratio_*value_,    pos.y_+size, 0.0f);
        (*vertex)[3].set(pos.x_,                              pos.y_+size, 0.0f);

        (*texcoord)[0].set(0,0);
        (*texcoord)[1].set(value_,0);
        (*texcoord)[2].set(value_,1);
        (*texcoord)[3].set(0,1);
    }
}
