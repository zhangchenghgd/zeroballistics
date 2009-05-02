

#ifndef BLUEBEARD_ODE_MODEL_LOADER_INCLUDED
#define BLUEBEARD_ODE_MODEL_LOADER_INCLUDED

#include <string>

#include <loki/Singleton.h>


#include "OdeCollision.h"


class TiXmlNode;

namespace physics
{


class OdeGeom;
class OdeRigidBody;
class OdeSimulator; 

//------------------------------------------------------------------------------    
struct OdeModelInfo
{
    OdeModelInfo(const std::string & name, OdeRigidBody * body) :
        name_(name), body_(body) {}
    
    std::string name_;
    OdeRigidBody * body_;
};

    
#define s_ode_model_loader Loki::SingletonHolder<physics::OdeModelLoader, Loki::CreateUsingNew, SingletonOdeModelLoaderLifetime >::Instance()
//------------------------------------------------------------------------------
class OdeModelLoader
{
 public:
    OdeModelLoader();
    virtual ~OdeModelLoader();
    
    OdeRigidBody * instantiateModel(OdeSimulator * simulator, const std::string & name);
    
    
 protected:

    OdeRigidBody * loadModel(const std::string & name);

    OdeGeom * loadShape (const std::string & name, TiXmlNode * shape_node);

    OdeGeom * loadSphere    (TiXmlNode * sphere_node);
    OdeGeom * loadCCylinder (TiXmlNode * ccylinder_node);
    OdeGeom * loadBox       (TiXmlNode * box_node);
    OdeGeom * loadPlane     (TiXmlNode * plane_node);
    OdeGeom * loadRay       (TiXmlNode * ray_node);
    OdeGeom * loadTrimesh   (const std::string & name, TiXmlNode * trimesh_node);
    OdeGeom * loadContinuous(TiXmlNode * cont_node);
    
    Material loadMaterial(TiXmlNode * shape_node);

    bool removeDegenerates(Trimesh * trimesh) const;

    
    std::vector<OdeModelInfo> info_;
};


} // namespace physics 

#endif
