
#ifndef TANK_HUD_TEXTURE_ELEMENT_INCLUDED
#define TANK_HUD_TEXTURE_ELEMENT_INCLUDED


#include <osg/Geometry>

#include "Utils.h" 
#include "RegisteredFpGroup.h"
#include "Vector2d.h"


//------------------------------------------------------------------------------
class HudTextureElement : public osg::Geometry
{
 public:

    HudTextureElement(const std::string & section);
    HudTextureElement(const std::string & section, const std::string & texture);
    
    virtual ~HudTextureElement();

    virtual const char * className () const { return "HudTextureElement"; }
    virtual osg::Object* cloneType() const { assert(false); return NULL; }
    virtual osg::Object* clone(const osg::CopyOp& copyop) const { assert(false); return NULL; }
    
    static Vector2d getScreenCoords(const std::string & section);
    static float alignAndGetSize   (const std::string & section, Vector2d & pos, float aspect_ratio = 1.0f);
    
 protected:

    void getPositionAndSize(Vector2d & pos, float & size) const;
    
    virtual void updateVb();
    
    void init();
    void onResolutionChanged();
    
    float aspect_ratio_; ///< The aspect ratio of the texture image
                         ///file. Width / Height.

    std::string section_;

    std::string texture_name_;
    
    RegisteredFpGroup fp_group_;
};



#endif
