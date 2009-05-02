
#include "HudTextureElement.h"


#include <osg/Texture2D>
#include <osg/Image>
#include <osgDB/ReadFile>

#include "Log.h"
#include "TextureManager.h"
#include "ParameterManager.h"
#include "Vector2d.h"
#include "Datatypes.h"
#include "Paths.h"
#include "SceneManager.h"


//------------------------------------------------------------------------------
HudTextureElement::HudTextureElement(const std::string & section) :
    aspect_ratio_(1.0f),
    section_(section)
{
    texture_name_ = HUD_TEXTURE_PATH + s_params.get<std::string>("hud." + section_ + ".texture");
    init();
}

//------------------------------------------------------------------------------
HudTextureElement::HudTextureElement(const std::string & section, const std::string & texture) :
    aspect_ratio_(1.0f),
    section_(section)
{
    assert(!texture.empty());

    texture_name_ = texture;
    init();
}

//------------------------------------------------------------------------------
HudTextureElement::~HudTextureElement()
{
}



//------------------------------------------------------------------------------
void HudTextureElement::updateVb()
{
    Vector2d pos;
    float size;
    getPositionAndSize(pos, size);

    osg::Vec3Array * vertex = new osg::Vec3Array(4); // vec3 or computeBounds will complain
    setVertexArray(vertex);
    
    (*vertex)[0].set(pos.x_,                    pos.y_,      0.0f);
    (*vertex)[1].set(pos.x_+size*aspect_ratio_, pos.y_,      0.0f);
    (*vertex)[2].set(pos.x_+size*aspect_ratio_, pos.y_+size, 0.0f);
    (*vertex)[3].set(pos.x_,                    pos.y_+size, 0.0f);
}


//------------------------------------------------------------------------------
void HudTextureElement::init()
{
    Texture * tex_object = s_texturemanager.getResource(texture_name_);
    if (!tex_object) return;
    
    osg::ref_ptr<osg::Texture2D> tex = tex_object->getOsgTexture();
    osg::ref_ptr<osg::Image> image = tex->getImage();

    aspect_ratio_ = (float)image->s() / image->t();

    tex->setFilter(osg::Texture2D::MIN_FILTER,osg::Texture2D::LINEAR);
    tex->setFilter(osg::Texture2D::MAG_FILTER,osg::Texture2D::LINEAR);

    tex->setBorderColor(osg::Vec4(0,0,0,0));
    tex->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_BORDER);
    tex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_BORDER);
    
    getOrCreateStateSet()->setTextureAttributeAndModes(0,tex.get(),osg::StateAttribute::ON);

    setUseDisplayList(false);
    setUseVertexBufferObjects(true);

    addPrimitiveSet(new osg::DrawArrays(GL_QUADS,0,4));

    Color color  = s_params.get<Color>   ("hud." + section_ + ".color");
    
    osg::Vec2Array* texcoord = new osg::Vec2Array(4);
    setTexCoordArray(0,texcoord);
        
    osg::Vec4Array * osg_color = new osg::Vec4Array(1);
    (*osg_color)[0].set(color.r_, color.g_, color.b_, color.a_);
    setColorArray(osg_color);
    setColorBinding(osg::Geometry::BIND_OVERALL);

    (*texcoord)[0].set(0,0);
    (*texcoord)[1].set(1,0);
    (*texcoord)[2].set(1,1);
    (*texcoord)[3].set(0,1);


    s_scene_manager.addObserver(ObserverCallbackFun0(this, &HudTextureElement::onResolutionChanged),
                                SOE_RESOLUTION_CHANGED,
                                &fp_group_);    
    onResolutionChanged();
}



//------------------------------------------------------------------------------
/**
 *  Converts the position specified in pixels in the given parameter
 *  section to units which correspond to \see
 *  SceneManager::setWindowSize().
 */
Vector2d HudTextureElement::getScreenCoords(const std::string & section)
{
    Vector2d pos = s_params.get<Vector2d>("hud." + section + ".position");
    
    // Divide by reference screen size
    float ref_size = s_params.get<float>("hud.ref_screen_height");
    pos.x_ /= ref_size;
    pos.y_ /= ref_size;

    Vector2d ref_corner = s_params.get<Vector2d>("hud." + section + ".ref_corner");

    assert(ref_corner.x_ >= -1 && ref_corner.x_ <= 1);
    assert(ref_corner.y_ >= -1 && ref_corner.y_ <= 1);

    unsigned width, height;
    s_scene_manager.getWindowSize(width, height);    
    float x_coord_left_edge = -0.5f * ((float)width / height - 1.0f);
    float x_coord_right_edge = 1.0f - x_coord_left_edge;

    
    switch ((int)ref_corner.x_)
    {
    case -1:
        pos.x_ += x_coord_left_edge;
        break;
    case 0:
        pos.x_ += 0.5f;
        break;
    case 1:
        pos.x_  = x_coord_right_edge - pos.x_;
        break;
    default: assert(false);
    }
    switch ((int)ref_corner.y_)
    {
    case -1:
        break;
    case 0:
        pos.y_ += 0.5f;
        break;
    case 1:
        pos.y_ = 1.0f-pos.y_;
        break;
    default: assert(false);
    }

    return pos;
}

//------------------------------------------------------------------------------
float HudTextureElement::alignAndGetSize(const std::string & section, Vector2d & pos, float aspect_ratio)
{
    float size = (s_params.get<float> ("hud." + section + ".vert_size") /
                  s_params.get<float>("hud.ref_screen_height"));

    // Adjust position for alignment
    Vector2d ref_corner = s_params.get<Vector2d>("hud." + section + ".ref_corner");
    pos.x_ -= 0.5f * (ref_corner.x_+1) * size*aspect_ratio;
    pos.y_ -= 0.5f * (ref_corner.y_+1) * size;

    return size;
}



//------------------------------------------------------------------------------
void HudTextureElement::getPositionAndSize(Vector2d & pos, float & size) const
{
    try
    {
        pos  = getScreenCoords(section_);
    } catch (ParamNotFoundException & e)
    {
        // Assume that size is given and alignment will be done,
        // return center position per default
        pos = Vector2d(0.0f, 0.0f);
    }

    try
    {
        size = alignAndGetSize(section_, pos, aspect_ratio_);
    } catch (ParamNotFoundException & e)
    {
        // An offset by -0.5 puts the texture into the center (\see
        // HudTextureElement::updateVb()).
        pos = Vector2d(-0.5f, -0.5f);
        size = 1.0f;
    }
}



//------------------------------------------------------------------------------
void HudTextureElement::onResolutionChanged()
{
    updateVb();
}
