
#include "HudBar.h"

#include "ParameterManager.h"
#include "Vector2d.h"
#include "SceneManager.h"



//------------------------------------------------------------------------------
HudBar::HudBar(const std::string & section) :
    HudTextureElement(section),
    value_(0.0f)
{
    const std::string & fill_dir = s_params.get<std::string>("hud." + section + ".fill_direction");

    if (fill_dir == "RL") fill_direction_ = HBFD_RIGHT_LEFT; else
    if (fill_dir == "LR") fill_direction_ = HBFD_LEFT_RIGHT; else
    if (fill_dir == "TD") fill_direction_ = HBFD_TOP_DOWN;   else
    if (fill_dir == "BU") fill_direction_ = HBFD_BOTTOM_UP;  else
    {
        throw Exception("Fill dir must be RL, LR, TD or BU");
    }
    
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

    switch (fill_direction_)
    {
    case HBFD_LEFT_RIGHT:
        (*vertex)[0].set(pos.x_,                              pos.y_,      0.0f);
        (*vertex)[1].set(pos.x_+size*aspect_ratio_*value_,    pos.y_,      0.0f);
        (*vertex)[2].set(pos.x_+size*aspect_ratio_*value_,    pos.y_+size, 0.0f);
        (*vertex)[3].set(pos.x_,                              pos.y_+size, 0.0f);

        (*texcoord)[0].set(0,0);
        (*texcoord)[1].set(value_,0);
        (*texcoord)[2].set(value_,1);
        (*texcoord)[3].set(0,1);
        break;
    case HBFD_RIGHT_LEFT:
        (*vertex)[0].set(pos.x_+size*aspect_ratio_*(1.0f-value_),    pos.y_,      0.0f);
        (*vertex)[1].set(pos.x_+size*aspect_ratio_,           pos.y_,      0.0f);
        (*vertex)[2].set(pos.x_+size*aspect_ratio_,           pos.y_+size, 0.0f);
        (*vertex)[3].set(pos.x_+size*aspect_ratio_*(1.0f-value_),    pos.y_+size, 0.0f);

        (*texcoord)[0].set(1-value_,        0);
        (*texcoord)[1].set(1, 0);
        (*texcoord)[2].set(1, 1);
        (*texcoord)[3].set(1-value_,        1);
        break;
    case HBFD_TOP_DOWN:
        (*vertex)[0].set(pos.x_,                    pos.y_+size*(1.0f-value_), 0.0f);
        (*vertex)[1].set(pos.x_+size*aspect_ratio_, pos.y_+size*(1.0f-value_), 0.0f);
        (*vertex)[2].set(pos.x_+size*aspect_ratio_, pos.y_+size, 0.0f);
        (*vertex)[3].set(pos.x_,                    pos.y_+size, 0.0f);

        (*texcoord)[0].set(0,1-value_);
        (*texcoord)[1].set(1,1-value_);
        (*texcoord)[2].set(1,1);
        (*texcoord)[3].set(0,1);
        break;
    case HBFD_BOTTOM_UP:
        (*vertex)[0].set(pos.x_,                    pos.y_,             0.0f);
        (*vertex)[1].set(pos.x_+size*aspect_ratio_, pos.y_,             0.0f);
        (*vertex)[2].set(pos.x_+size*aspect_ratio_, pos.y_+size*value_, 0.0f);
        (*vertex)[3].set(pos.x_,                    pos.y_+size*value_, 0.0f);

        (*texcoord)[0].set(0,0);
        (*texcoord)[1].set(1,0);
        (*texcoord)[2].set(1,value_);
        (*texcoord)[3].set(0,value_);
        break;
    }
}
